#include "Helpers.h"
#include "Theme.h"
#include "State.h"
#include "../Render/RenderUtils.h"
#include "LiquidGlass.h"
#include "../Java.h"
#include <Windows.h>
#include <gl/GL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace Render {
namespace ClickGUIHelpers {

static inline float colorA(DWORD c) {
  return ((c >> 24) & 0xFF) / 255.0f;
}

static int s_activeSliderId = -1;
static int s_activeNumericId = -1;
static std::string s_numericBuffer;
static bool s_numericCommit = false;
static bool s_numericCancel = false;
static int s_activeHexId = -1;
static std::string s_hexBuffer;
static bool s_hexCommit = false;
static bool s_hexCancel = false;

struct PickerState {
  float hue = 0.0f;
  float saturation = 1.0f;
  float value = 1.0f;
  bool initialized = false;
  bool draggingSv = false;
  bool draggingHue = false;
  std::uint32_t synchronizedColor = 0;
};
static std::unordered_map<int, PickerState> s_pickerStates;

static std::uint32_t hsvToColor(float hue, float saturation, float value) {
  const float h6 = hue * 6.0f;
  const int sector = static_cast<int>(h6) % 6;
  const float fraction = h6 - static_cast<int>(h6);
  const float p = value * (1.0f - saturation);
  const float q = value * (1.0f - fraction * saturation);
  const float t = value * (1.0f - (1.0f - fraction) * saturation);
  float r = value, g = p, b = q;
  switch (sector) {
  case 0: r = value; g = t; b = p; break;
  case 1: r = q; g = value; b = p; break;
  case 2: r = p; g = value; b = t; break;
  case 3: r = p; g = q; b = value; break;
  case 4: r = t; g = p; b = value; break;
  default: r = value; g = p; b = q; break;
  }
  return 0xFF000000u |
         (static_cast<std::uint32_t>(r * 255.0f + 0.5f) << 16) |
         (static_cast<std::uint32_t>(g * 255.0f + 0.5f) << 8) |
         static_cast<std::uint32_t>(b * 255.0f + 0.5f);
}

static void colorToHsv(std::uint32_t color, float &hue, float &saturation,
                       float &value) {
  const float r = ((color >> 16) & 0xFF) / 255.0f;
  const float g = ((color >> 8) & 0xFF) / 255.0f;
  const float b = (color & 0xFF) / 255.0f;
  const float maximum = (std::max)(r, (std::max)(g, b));
  const float minimum = (std::min)(r, (std::min)(g, b));
  const float delta = maximum - minimum;
  value = maximum;
  saturation = maximum > 0.0f ? delta / maximum : 0.0f;
  hue = 0.0f;
  if (delta > 0.0001f) {
    if (maximum == r)
      hue = std::fmod((g - b) / delta, 6.0f);
    else if (maximum == g)
      hue = (b - r) / delta + 2.0f;
    else
      hue = (r - g) / delta + 4.0f;
    hue /= 6.0f;
    if (hue < 0.0f)
      hue += 1.0f;
  }
}

void cancelInlineEditors() {
  s_activeNumericId = -1;
  s_numericBuffer.clear();
  s_numericCommit = false;
  s_numericCancel = false;
  s_activeHexId = -1;
  s_hexBuffer.clear();
  s_hexCommit = false;
  s_hexCancel = false;
}

bool handleEditorMessage(UINT msg, WPARAM wParam, LPARAM) {
  if (s_activeNumericId < 0 && s_activeHexId < 0)
    return false;
  if (msg == WM_KILLFOCUS) {
    if (s_activeNumericId >= 0) s_numericCancel = true;
    if (s_activeHexId >= 0) s_hexCancel = true;
    return true;
  }
  if (msg == WM_KEYDOWN) {
    if (wParam == VK_ESCAPE) {
      if (s_activeNumericId >= 0) s_numericCancel = true;
      if (s_activeHexId >= 0) s_hexCancel = true;
      return true;
    }
    if (wParam == VK_RETURN) {
      if (s_activeNumericId >= 0) s_numericCommit = true;
      if (s_activeHexId >= 0) s_hexCommit = true;
      return true;
    }
    if (wParam == 'V' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
      if (OpenClipboard(nullptr)) {
        if (HANDLE data = GetClipboardData(CF_TEXT)) {
          if (const char *text = static_cast<const char *>(GlobalLock(data))) {
            for (const char *p = text; *p; ++p) {
              if (s_activeHexId >= 0 && s_hexBuffer.size() < 7) {
                const char c = static_cast<char>(std::toupper(
                    static_cast<unsigned char>(*p)));
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
                    (c == '#' && s_hexBuffer.empty()))
                  s_hexBuffer.push_back(c);
              } else if (s_activeNumericId >= 0 &&
                         s_numericBuffer.size() < 20) {
                char c = *p == ',' ? '.' : *p;
                if ((c >= '0' && c <= '9') || c == '.' || c == '-')
                  s_numericBuffer.push_back(c);
              }
            }
            GlobalUnlock(data);
          }
        }
        CloseClipboard();
      }
      return true;
    }
  }
  if (msg == WM_CHAR) {
    const char c = static_cast<char>(wParam);
    if (s_activeHexId >= 0) {
      if (c == 8) {
        if (!s_hexBuffer.empty()) s_hexBuffer.pop_back();
      } else if (c == 13) {
        s_hexCommit = true;
      } else if (s_hexBuffer.size() < 7) {
        const char upper = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c)));
        if ((upper >= '0' && upper <= '9') ||
            (upper >= 'A' && upper <= 'F') ||
            (upper == '#' && s_hexBuffer.empty()))
          s_hexBuffer.push_back(upper);
      }
    } else {
      if (c == 8) {
        if (!s_numericBuffer.empty()) s_numericBuffer.pop_back();
      } else if (c == 13) {
        s_numericCommit = true;
      } else if (s_numericBuffer.size() < 20 &&
                 ((c >= '0' && c <= '9') || c == '.' || c == ',' ||
                  c == '-')) {
        s_numericBuffer.push_back(c == ',' ? '.' : c);
      }
    }
    return true;
  }
  return false;
}

bool drawNumericInput(int id, float x, float y, float w, float h, float &value,
                      float minValue, float maxValue, int decimals,
                      const char *suffix, float mx, float my, bool clickEvent,
                      float alpha) {
  const bool hovered = mx >= x && mx <= x + w && my >= y && my <= y + h;
  bool changed = false;
  if (clickEvent && hovered) {
    s_activeNumericId = id;
    char valueText[32]{};
    std::snprintf(valueText, sizeof(valueText), "%.*f",
                  std::clamp(decimals, 0, 4), value);
    s_numericBuffer = valueText;
    s_numericCommit = false;
    s_numericCancel = false;
  } else if (clickEvent && s_activeNumericId == id) {
    s_numericCommit = true;
  }

  const bool active = s_activeNumericId == id;
  if (active && s_numericCancel) {
    cancelInlineEditors();
  } else if (active && s_numericCommit) {
    char *end = nullptr;
    const float entered = std::strtof(s_numericBuffer.c_str(), &end);
    if (end != s_numericBuffer.c_str() && end && *end == '\0' &&
        std::isfinite(entered)) {
      const float clamped = std::clamp(entered, minValue, maxValue);
      changed = clamped != value;
      value = clamped;
    }
    cancelInlineEditors();
  }

  const bool focused = s_activeNumericId == id;
  glDisable(GL_TEXTURE_2D);
  drawTextInput(x, y, w, h, focused, hovered, alpha);
  glEnable(GL_TEXTURE_2D);
  std::string display;
  if (focused) {
    display = s_numericBuffer;
    if ((GetTickCount64() / 500) % 2 == 0)
      display += '|';
  } else {
    char valueText[48]{};
    std::snprintf(valueText, sizeof(valueText), "%.*f%s",
                  std::clamp(decimals, 0, 4), value, suffix ? suffix : "");
    display = valueText;
  }
  ClickGUIState::g_guiFont.drawString(
      x + 7.0f, y + h * 0.5f - 6.0f, display.c_str(),
      applyAlpha(focused ? ClickGUITheme::textPrimary()
                         : ClickGUITheme::accent(),
                 alpha),
      0.38f);
  return changed;
}

float colorPickerHeight(bool includeRainbow) {
  return includeRainbow ? 230.0f : 196.0f;
}

bool drawColorPicker(int id, float x, float y, float width,
                     std::uint32_t &color, float mx, float my, bool lClick,
                     bool clickEvent, float alpha, bool *rainbow) {
  PickerState &state = s_pickerStates[id];
  const std::uint32_t opaqueColor = 0xFF000000u | (color & 0x00FFFFFFu);
  if (!state.initialized ||
      (!state.draggingSv && !state.draggingHue && s_activeHexId != id &&
       opaqueColor != state.synchronizedColor)) {
    colorToHsv(opaqueColor, state.hue, state.saturation, state.value);
    state.synchronizedColor = opaqueColor;
    state.initialized = true;
  }

  const float pickerH = colorPickerHeight(rainbow != nullptr);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(x - 8.0f, y - 8.0f, width + 16.0f, pickerH + 16.0f,
                isHovered(mx, my, x - 8.0f, y - 8.0f, width + 16.0f,
                          pickerH + 16.0f),
                alpha);

  const float svX = x;
  const float svY = y;
  const float svW = (std::max)(120.0f, width);
  const float svH = 104.0f;
  const std::uint32_t hueColor = hsvToColor(state.hue, 1.0f, 1.0f);
  const float hueR = ((hueColor >> 16) & 0xFF) / 255.0f;
  const float hueG = ((hueColor >> 8) & 0xFF) / 255.0f;
  const float hueB = (hueColor & 0xFF) / 255.0f;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glShadeModel(GL_SMOOTH);
  glBegin(GL_QUADS);
  glColor4f(1, 1, 1, alpha); glVertex2f(svX, svY);
  glColor4f(hueR, hueG, hueB, alpha); glVertex2f(svX + svW, svY);
  glColor4f(hueR, hueG, hueB, alpha); glVertex2f(svX + svW, svY + svH);
  glColor4f(1, 1, 1, alpha); glVertex2f(svX, svY + svH);
  glEnd();
  glBegin(GL_QUADS);
  glColor4f(0, 0, 0, 0); glVertex2f(svX, svY);
  glColor4f(0, 0, 0, 0); glVertex2f(svX + svW, svY);
  glColor4f(0, 0, 0, alpha); glVertex2f(svX + svW, svY + svH);
  glColor4f(0, 0, 0, alpha); glVertex2f(svX, svY + svH);
  glEnd();

  const float cursorX = svX + state.saturation * svW;
  const float cursorY = svY + (1.0f - state.value) * svH;
  glShadeModel(GL_FLAT);
  glColor4f(1, 1, 1, alpha);
  glLineWidth(1.5f);
  glBegin(GL_LINE_LOOP);
  for (int index = 0; index < 18; ++index) {
    const float angle = index * 6.2831853f / 18.0f;
    glVertex2f(cursorX + std::cos(angle) * 5.0f,
               cursorY + std::sin(angle) * 5.0f);
  }
  glEnd();
  glLineWidth(1.0f);

  if (isHovered(mx, my, svX, svY, svW, svH) && lClick &&
      !state.draggingHue)
    state.draggingSv = true;
  bool changed = false;
  if (state.draggingSv) {
    if (lClick) {
      state.saturation = std::clamp((mx - svX) / svW, 0.0f, 1.0f);
      state.value = std::clamp(1.0f - (my - svY) / svH, 0.0f, 1.0f);
      changed = true;
    } else {
      state.draggingSv = false;
    }
  }

  const float hueX = x;
  const float hueY = svY + svH + 8.0f;
  const float hueH = 12.0f;
  static const float stops[7][3] = {
      {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1},
      {0, 0, 1}, {1, 0, 1}, {1, 0, 0}};
  glShadeModel(GL_SMOOTH);
  for (int index = 0; index < 6; ++index) {
    const float left = hueX + svW * index / 6.0f;
    const float right = hueX + svW * (index + 1) / 6.0f;
    glBegin(GL_QUADS);
    glColor4f(stops[index][0], stops[index][1], stops[index][2], alpha);
    glVertex2f(left, hueY); glVertex2f(left, hueY + hueH);
    glColor4f(stops[index + 1][0], stops[index + 1][1],
              stops[index + 1][2], alpha);
    glVertex2f(right, hueY + hueH); glVertex2f(right, hueY);
    glEnd();
  }
  const float hueCursor = hueX + state.hue * svW;
  glShadeModel(GL_FLAT);
  glColor4f(1, 1, 1, alpha);
  glBegin(GL_LINES);
  glVertex2f(hueCursor, hueY - 2.0f);
  glVertex2f(hueCursor, hueY + hueH + 2.0f);
  glEnd();
  glEnable(GL_TEXTURE_2D);
  if (isHovered(mx, my, hueX, hueY - 4.0f, svW, hueH + 8.0f) && lClick &&
      !state.draggingSv)
    state.draggingHue = true;
  if (state.draggingHue) {
    if (lClick) {
      state.hue = std::clamp((mx - hueX) / svW, 0.0f, 0.9999f);
      changed = true;
    } else {
      state.draggingHue = false;
    }
  }

  std::uint32_t selected = hsvToColor(state.hue, state.saturation, state.value);
  const float controlsY = hueY + hueH + 9.0f;
  glDisable(GL_TEXTURE_2D);
  RenderUtils::drawRoundedRect(x, controlsY, 28.0f, 26.0f, 5.0f, selected,
                               alpha);
  glEnable(GL_TEXTURE_2D);
  const float hexX = x + 36.0f;
  const float hexW = 90.0f;
  const bool hexHovered = isHovered(mx, my, hexX, controlsY, hexW, 26.0f);
  if (clickEvent && hexHovered) {
    s_activeHexId = id;
    char buffer[12]{};
    std::snprintf(buffer, sizeof(buffer), "#%06X", selected & 0xFFFFFFu);
    s_hexBuffer = buffer;
    s_hexCommit = s_hexCancel = false;
    s_activeNumericId = -1;
  } else if (clickEvent && s_activeHexId == id) {
    s_hexCommit = true;
  }
  if (s_activeHexId == id && s_hexCancel) {
    s_activeHexId = -1;
    s_hexBuffer.clear();
    s_hexCancel = false;
  } else if (s_activeHexId == id && s_hexCommit) {
    std::string digits = s_hexBuffer;
    if (!digits.empty() && digits.front() == '#') digits.erase(digits.begin());
    if (digits.size() == 6) {
      char *end = nullptr;
      const unsigned long parsed = std::strtoul(digits.c_str(), &end, 16);
      if (end && *end == '\0') {
        selected = 0xFF000000u | static_cast<std::uint32_t>(parsed);
        colorToHsv(selected, state.hue, state.saturation, state.value);
        changed = true;
      }
    }
    s_activeHexId = -1;
    s_hexBuffer.clear();
    s_hexCommit = false;
  }
  glDisable(GL_TEXTURE_2D);
  drawTextInput(hexX, controlsY, hexW, 26.0f, s_activeHexId == id,
                hexHovered, alpha);
  glEnable(GL_TEXTURE_2D);
  char selectedHex[12]{};
  std::snprintf(selectedHex, sizeof(selectedHex), "#%06X",
                selected & 0xFFFFFFu);
  std::string hexDisplay = s_activeHexId == id ? s_hexBuffer : selectedHex;
  if (s_activeHexId == id && (GetTickCount64() / 500) % 2 == 0)
    hexDisplay += '|';
  ClickGUIState::g_guiFont.drawString(
      hexX + 8.0f, controlsY + 7.0f, hexDisplay.c_str(),
      applyAlpha(ClickGUITheme::textPrimary(), alpha), 0.38f);

  static const std::uint32_t presets[8] = {
      0xFF3D6EF5, 0xFF19B0FF, 0xFF2EE6B8, 0xFF43E08B,
      0xFF9B6BF5, 0xFFFA3EC0, 0xFFFF5436, 0xFFFFA319};
  const float presetStart = x + 136.0f;
  const float presetGap = 5.0f;
  const float presetSize = std::clamp(
      (svW - (presetStart - x) - presetGap * 7.0f) / 8.0f, 16.0f, 26.0f);
  float presetX = presetStart;
  glDisable(GL_TEXTURE_2D);
  for (int index = 0; index < 8; ++index) {
    const bool presetHovered =
        isHovered(mx, my, presetX, controlsY, presetSize, 26.0f);
    if ((selected & 0xFFFFFFu) == (presets[index] & 0xFFFFFFu) ||
        presetHovered)
      RenderUtils::drawRoundedOutline(
          presetX - 2.0f, controlsY - 2.0f, presetSize + 4.0f, 30.0f, 5.0f,
          1.0f, 0xFFFFFFFF, (presetHovered ? 0.5f : 0.9f) * alpha);
    RenderUtils::drawRoundedRect(presetX, controlsY, presetSize, 26.0f, 4.0f,
                                 presets[index], alpha);
    if (clickEvent && presetHovered) {
      selected = presets[index];
      colorToHsv(selected, state.hue, state.saturation, state.value);
      changed = true;
    }
    presetX += presetSize + presetGap;
  }
  glEnable(GL_TEXTURE_2D);

  if (rainbow) {
    const float rainbowY = controlsY + 36.0f;
    const bool rainbowHovered =
        isHovered(mx, my, x, rainbowY, 130.0f, 26.0f);
    glDisable(GL_TEXTURE_2D);
    drawSwitch(100000 + id, x, rainbowY + 4.0f, *rainbow,
               rainbowHovered, alpha);
    glEnable(GL_TEXTURE_2D);
    ClickGUIState::g_guiFont.drawString(
        x + 44.0f, rainbowY + 7.0f, "Rainbow",
        applyAlpha(ClickGUITheme::textPrimary(), alpha), 0.40f);
    if (clickEvent && rainbowHovered)
      *rainbow = !*rainbow;
  }

  if (changed) {
    selected = hsvToColor(state.hue, state.saturation, state.value);
    color = selected;
    state.synchronizedColor = selected;
  }
  return changed;
}

void setMouseGrabbed(bool grabbed) {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  jclass mouseCls = lc->GetClass("org.lwjgl.input.Mouse");
  if (!mouseCls)
    return;
  jmethodID m_setGrabbed =
      env->GetStaticMethodID(mouseCls, "setGrabbed", "(Z)V");
  if (m_setGrabbed) {
    env->CallStaticVoidMethod(mouseCls, m_setGrabbed, grabbed);
  }
}

bool isIngame() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return false;
  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return false;
  }
  jobject mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj)
    return false;
  jfieldID f_screen = env->GetFieldID(mcCls, "currentScreen",
                                      "Lnet/minecraft/client/gui/GuiScreen;");
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = env->GetFieldID(mcCls, "field_71462_r",
                               "Lnet/minecraft/client/gui/GuiScreen;");
  }
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = env->GetFieldID(mcCls, "m", "Laxu;");
  }
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return false;
  }
  jobject screen = env->GetObjectField(mcObj, f_screen);
  bool ingame = (screen == nullptr);
  if (screen)
    env->DeleteLocalRef(screen);
  env->DeleteLocalRef(mcObj);
  return ingame;
}

void drawSwitch(int id, float x, float y, bool enabled, bool hovered,
                float alpha) {
  using namespace ClickGUITheme;
  const bool glass = (style() == Style::LiquidGlass);

  float w = 34.0f;
  float h = 18.0f;

  static std::unordered_map<int, float> anims;
  if (anims.find(id) == anims.end())
    anims[id] = enabled ? 1.0f : 0.0f;
  anims[id] += ((enabled ? 1.0f : 0.0f) - anims[id]) * 0.22f;
  float t = anims[id];

  if (glass) {
    // background pill (bp)
    DWORD off = 0x33FFFFFF; // glassy off state
    DWORD on  = accent();
    DWORD bg  = RenderUtils::lerpColor(off, on, t);
    if (hovered)
      bg = RenderUtils::lerpColor(bg, 0xFFFFFFFF, 0.12f);
    Render::LiquidGlass::drawRect(x, y, w, h, h / 2.0f, alpha, bg);
    
    float pad = 2.5f;
    float knobD = h - 2 * pad;
    float knobX = x + pad + t * (w - h);
    
    RenderUtils::drawCircle(knobX + knobD / 2.0f, y + h / 2.0f,
                            knobD / 2.0f + 1.5f, 0x55000000,
                            0.8f * alpha); // knob shadow
    
    RenderUtils::drawCircle(knobX + knobD / 2.0f, y + h / 2.0f,
                            knobD / 2.0f, 0xFFFFFFFF, alpha);
                            
    RenderUtils::drawCircle(knobX + knobD / 2.0f - 1, y + h / 2.0f - 1,
                            (knobD / 2.0f) * 0.55f, 0x66FFFFFF,
                            alpha); 
    return;
  }

  w = 44.0f;
  h = 25.0f;
  const float r = h * 0.5f;
  if (t > 0.01f)
    RenderUtils::drawGlow(x, y, w, h, r, accent(), 0.22f * t * alpha);
  DWORD track = RenderUtils::lerpColor(surface2(), accent(), t);
  RenderUtils::drawRoundedRect(x, y, w, h, r, track, colorA(track) * alpha);
  DWORD bd = RenderUtils::lerpColor(hairlineStrong(), 0xFF5E86F7, t);
  RenderUtils::drawRoundedOutline(x, y, w, h, r, 1.0f, bd,
                                  colorA(bd) * (0.5f + 0.3f * (1.0f - t)) * alpha);
  const float knobR = 9.5f;
  float kcx = x + 2.0f + t * 21.0f + knobR;
  float kcy = y + h * 0.5f;
  RenderUtils::drawCircle(kcx, kcy + 1.5f, knobR, 0x55000000, 0.6f * alpha);
  RenderUtils::drawCircle(kcx, kcy, knobR, 0xFFFFFFFF, alpha);
}

bool drawSlider(int id, float x, float y, float w, float h, float &val, float minVal, float maxVal, float mx, float my, bool lClick, float alpha) {
  bool interacting = false;
  float knobR = h / 2.0f;

  // Capture the slider on mouse-down and keep it captured until release. The
  // old implementation required the pointer to remain inside an ~16px strip,
  // which made a small vertical wobble interrupt dragging.
  const float hitPadX = knobR + 3.0f;
  const float hitPadY = knobR + 5.0f;
  const bool hovered = mx >= x - hitPadX && mx <= x + w + hitPadX &&
                       my >= y - hitPadY && my <= y + h + hitPadY;
  if (!lClick)
    s_activeSliderId = -1;
  else if (s_activeSliderId < 0 && hovered)
    s_activeSliderId = id;

  if (lClick && s_activeSliderId == id) {
    if (s_activeNumericId == id)
      cancelInlineEditors();
    interacting = true;
    float pct = (mx - x) / w;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    val = minVal + pct * (maxVal - minVal);
  }
  
  float pct = (val - minVal) / (maxVal - minVal);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 1.0f) pct = 1.0f;
  
  bool glass = (Config::getClickGuiTheme() == "LiquidGlass");
  if (glass) {
    Render::LiquidGlass::drawRect(x, y + h/2.0f - 2.0f, w, 4.0f, 2.0f, alpha, 0x33FFFFFF);
    Render::LiquidGlass::drawRect(x, y + h/2.0f - 2.0f, w * pct, 4.0f, 2.0f, alpha, ClickGUITheme::accent());
    float kx = x + w * pct;
    RenderUtils::drawCircle(kx, y + h/2.0f, knobR + 1.0f, 0x55000000, alpha * 0.8f);
    RenderUtils::drawCircle(kx, y + h/2.0f, knobR, 0xFFFFFFFF, alpha);
  } else {
    using namespace ClickGUITheme;
    RenderUtils::drawRoundedRect(x, y + h/2.0f - 1.5f, w, 3.0f, 1.5f,
                                 0xFF202025, alpha * 0.7f);
    if (w * pct > 0.5f) {
      RenderUtils::drawRoundedRect(x, y + h/2.0f - 1.5f, w * pct, 3.0f, 1.5f,
                                   accent(), alpha);
    }
    float kx = x + w * pct;
    RenderUtils::drawCircle(kx, y + h/2.0f + 0.5f, 6.0f, 0x44000000, 0.5f * alpha);
    RenderUtils::drawCircle(kx, y + h/2.0f, 6.0f, 0xFFFFFFFF, alpha);
  }
  
  return interacting;
}

void drawThemePanel(float x, float y, float w, float h, float alpha) {
  using namespace ClickGUITheme;
  const float r = panelRadius();

  if (style() == Style::LiquidGlass) {
    Render::LiquidGlass::drawRect(x, y, w, h, r, alpha, panelBg(), true);
    return;
  }

  for (int i = 6; i >= 1; --i) {
    float sp = i * 2.0f;
    RenderUtils::drawRoundedRect(x - sp, y - sp, w + 2.0f * sp, h + 2.0f * sp,
                                 r + sp, 0x000000,
                                 0.05f * (1.0f - i / 7.0f) * alpha);
  }
  DWORD bd = hairlineStrong();
  DWORD bg = panelBg();
  RenderUtils::drawRoundedRect(x - 1, y - 1, w + 2, h + 2, r,
                                bd, colorA(bd) * alpha);
  RenderUtils::drawRoundedRect(x, y, w, h, r, bg, colorA(bg) * alpha);

  float t = (float)GetTickCount64() / 1000.0f;
  float ph1 = (sinf(t / 2.6f) + 1.0f) * 0.5f;
  float ph2 = (sinf(t / 3.4f + 2.0f) + 1.0f) * 0.5f;
  float ph3 = (sinf(t / 4.1f + 4.0f) + 1.0f) * 0.5f;
  RenderUtils::drawRadialGlow(x + w * 0.85f, y + h * 0.82f, h * 1.2f,
                              RenderUtils::lerpColor(0xFF8A5BE8, 0xFFB44DE0, ph1),
                              0.40f * alpha);
  RenderUtils::drawRadialGlow(x + w * 0.12f, y + h * 0.18f, h * 0.8f,
                              RenderUtils::lerpColor(0xFF4D6FE0, 0xFF5B8AF0, ph2),
                              0.28f * alpha);
  RenderUtils::drawRadialGlow(x + w * 0.45f, y + h * 1.02f, h * 0.95f,
                              RenderUtils::lerpColor(0xFFD8559E, 0xFF8A5BE8, ph3),
                              0.20f * alpha);
}

void drawThemeSidebar(float x, float y, float w, float h, float alpha) {
  using namespace ClickGUITheme;
  const float r = panelRadius();
  DWORD bg = sidebarBg();
  RenderUtils::drawRoundedRect(x, y, w, h, r, bg, colorA(bg) * alpha);
  RenderUtils::drawRect(x + w - 12, y, 12, h, bg, colorA(bg) * alpha);
  DWORD sep = (style() == Style::LiquidGlass) ? 0xFFFFFFFF : border();
  float sepA = (style() == Style::LiquidGlass) ? 0.13f : 1.0f;
  RenderUtils::drawRect(x + w, y + 16, 1, h - 32, sep,
                        sepA * (sep == border() ? colorA(sep) : 1.0f) * alpha);
}

void drawThemeCard(float x, float y, float w, float h, bool hovered,
                   float alpha, bool active) {
  using namespace ClickGUITheme;
  const float r = cardRadius();

  static std::unordered_map<int, float> s_hov;
  int key = (int)(x * 2.0f) * 131071 + (int)(y * 2.0f);
  float &hv = s_hov[key];
  hv += ((hovered ? 1.0f : 0.0f) - hv) * 0.18f;
  float act = active ? 1.0f : 0.0f;
  float strip = hv > act ? hv : act;

  DWORD fill = RenderUtils::lerpColor(cardBg(), cardHover(), hv);

  if (style() == Style::LiquidGlass) {
    Render::LiquidGlass::drawRect(x, y, w, h, r, alpha, fill);
    if (strip > 0.01f)
      RenderUtils::drawRoundedRect(x + 5, y + 6, 3, h - 12, 1.5f, accent(),
                                    strip * alpha);
    return;
  }

  RenderUtils::drawRoundedRect(x, y, w, h, r, fill, colorA(fill) * alpha);
  DWORD bd = RenderUtils::lerpColor(hairline(), hairlineStrong(), strip);
  RenderUtils::drawRoundedOutline(x, y, w, h, r, 1.0f, bd, colorA(bd) * alpha);
  if (strip > 0.01f) {
    RenderUtils::drawGlow(x + 3.0f, y + 9.0f, 3.0f, h - 18.0f, 1.5f, accent(),
                          0.18f * strip * alpha);
    RenderUtils::drawRoundedRect(x + 3.0f, y + 9.0f, 3.0f, h - 18.0f, 1.5f,
                                 accent(), strip * alpha);
  }
}

void drawThemeButton(float x, float y, float w, float h, bool hovered,
                     bool pressed, float alpha) {
  using namespace ClickGUITheme;
  const float r = buttonRadius();
  if (style() == Style::LiquidGlass) {
    if (hovered) {
      RenderUtils::drawRoundedRect(x - 2, y - 2, w + 4, h + 4, r + 2,
                                    accent(), 0.25f * alpha);
    }
    RenderUtils::drawRoundedRect(x, y, w, h, r, 0xFF0A0A12, 0.55f * alpha);
    DWORD fill = pressed ? cardHover() : (hovered ? accent() : cardBg());
    Render::LiquidGlass::drawRect(x, y, w, h, r, alpha, fill);
    return;
  }
  DWORD fill = (hovered || pressed) ? surface2() : surface1();
  RenderUtils::drawRoundedRect(x, y, w, h, r, fill, colorA(fill) * alpha);
  DWORD bd = (hovered || pressed) ? hairlineStrong() : hairline();
  RenderUtils::drawRoundedOutline(x, y, w, h, r, 1.0f, bd, colorA(bd) * alpha);
}

void drawSectionLabel(float x, float y, const std::string &text, float alpha) {
  std::string up = text;
  for (auto &c : up)
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  ClickGUIState::g_guiFont.drawString(
      x, y, up, applyAlpha(ClickGUITheme::textMuted(), alpha), 0.38f);
}

void drawChevron(float ccx, float ccy, float s, bool open, uint32_t col,
                 float alpha) {
  float r = ((col >> 16) & 0xFF) / 255.0f;
  float g = ((col >> 8) & 0xFF) / 255.0f;
  float b = (col & 0xFF) / 255.0f;
  float a = (((col >> 24) & 0xFF) / 255.0f) * alpha;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glLineWidth(1.6f);
  glColor4f(r, g, b, a);
  glBegin(GL_LINE_STRIP);
  if (open) {  // pointing up
    glVertex2f(ccx - s, ccy + s * 0.5f);
    glVertex2f(ccx, ccy - s * 0.5f);
    glVertex2f(ccx + s, ccy + s * 0.5f);
  } else {     // pointing down
    glVertex2f(ccx - s, ccy - s * 0.5f);
    glVertex2f(ccx, ccy + s * 0.5f);
    glVertex2f(ccx + s, ccy - s * 0.5f);
  }
  glEnd();
  glLineWidth(1.0f);
  glEnable(GL_TEXTURE_2D);
}

void drawTextInput(float x, float y, float w, float h, bool focused,
                   bool hovered, float alpha) {
  using namespace ClickGUITheme;
  DWORD ins = inset();
  RenderUtils::drawRoundedRect(x, y, w, h, controlRadius(), ins,
                               colorA(ins) * alpha);
  DWORD bd = focused ? ((accent() & 0x00FFFFFF) | 0x80000000)
                     : (hovered ? hairlineStrong() : hairline());
  RenderUtils::drawRoundedOutline(x, y, w, h, controlRadius(), 1.0f, bd,
                                  colorA(bd) * alpha);
}

void drawThemeTabIndicator(float x, float y, float w, float h, float alpha) {
  using namespace ClickGUITheme;
  if (style() == Style::LiquidGlass) {
    RenderUtils::drawRoundedRect(x - 3, y - 3, w + 6, h + 6, h / 2.0f + 3.0f,
                                  accent(), 0.18f * alpha);
    Render::LiquidGlass::drawRect(x, y, w, h, h / 2.0f, 0.85f * alpha, cardHover());
    RenderUtils::drawRoundedRect(x + 1, y + 1, w - 2, 1.0f, 0.5f,
                                  0xFFFFFFFF, 0.30f * alpha);
    RenderUtils::drawRoundedRect(x + 6, y + 8, 3, h - 16, 1.5f,
                                  accent(), alpha);
    return;
  }
  const float pr = pillRadius();
  RenderUtils::drawGlow(x, y, w, h, pr, accent(), 0.16f * alpha);
  DWORD soft = accentSoft();
  RenderUtils::drawRoundedRect(x, y, w, h, pr, soft, colorA(soft) * alpha);
  DWORD bdc = accentBorder();
  RenderUtils::drawRoundedOutline(x, y, w, h, pr, 1.0f, bdc, colorA(bdc) * alpha);
  RenderUtils::drawRoundedRect(x + 6.0f, y + 8.0f, 3.0f, h - 16.0f, 1.5f,
                               accent(), alpha);
  RenderUtils::drawRoundedRect(x + 4.0f, y + 1.0f, w - 8.0f, 1.0f, 0.5f,
                               0xFFFFFFFF, 0.15f * alpha);
}

void drawThemeBackground(float screenW, float screenH, float alpha) {
  using namespace ClickGUITheme;
  if (style() == Style::LiquidGlass)
    return;

  float t = (float)GetTickCount64() / 1000.0f;
  float ph1 = (sinf(t / 3.0f) + 1.0f) * 0.5f;
  float ph2 = (sinf(t / 3.8f + 2.0f) + 1.0f) * 0.5f;
  float ph3 = (sinf(t / 4.6f + 4.0f) + 1.0f) * 0.5f;

  RenderUtils::drawRadialGlow(screenW * 0.92f, screenH * 0.46f, screenH * 1.35f,
                              RenderUtils::lerpColor(0xFF8A5BE8, 0xFFB44DE0, ph1),
                              0.32f * alpha);
  RenderUtils::drawRadialGlow(screenW * 0.05f, screenH * 0.30f, screenH * 1.15f,
                              RenderUtils::lerpColor(0xFF4D6FE0, 0xFF5B8AF0, ph2),
                              0.24f * alpha);
  RenderUtils::drawRadialGlow(screenW * 0.78f, screenH * 1.05f, screenH * 1.05f,
                              RenderUtils::lerpColor(0xFFD8559E, 0xFF8A5BE8, ph3),
                              0.18f * alpha);
  RenderUtils::drawRadialGlow(screenW * 1.02f, screenH * 0.12f, screenH * 0.85f,
                              RenderUtils::lerpColor(0xFFB44DE0, 0xFF8A5BE8, ph1),
                              0.16f * alpha);
}

} // namespace ClickGUIHelpers
} // namespace Render
