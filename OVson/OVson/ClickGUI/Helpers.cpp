#include "Helpers.h"
#include "Theme.h"
#include "State.h"
#include "../Render/RenderUtils.h"
#include "LiquidGlass.h"
#include "../Java.h"
#include <Windows.h>
#include <gl/GL.h>
#include <cmath>
#include <unordered_map>

namespace Render {
namespace ClickGUIHelpers {

static inline float colorA(DWORD c) {
  return ((c >> 24) & 0xFF) / 255.0f;
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
  
  if (lClick && mx >= x - knobR && mx <= x + w + knobR && my >= y - knobR && my <= y + h + knobR) {
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
  // extra purple bloom hugging the top-right edge for a fuller right side
  RenderUtils::drawRadialGlow(screenW * 1.02f, screenH * 0.12f, screenH * 0.85f,
                              RenderUtils::lerpColor(0xFFB44DE0, 0xFF8A5BE8, ph1),
                              0.16f * alpha);
}

} // namespace ClickGUIHelpers
} // namespace Render
