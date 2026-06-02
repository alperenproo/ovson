#include "Helpers.h"
#include "Theme.h"
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

  DWORD bgBase = RenderUtils::lerpColor(0xFF2D2D31, accent(), t);
  if (hovered)
    bgBase = RenderUtils::lerpColor(bgBase, 0xFFFFFFFF, 0.15f);
  RenderUtils::drawRoundedRect(x, y, w, h, h / 2.0f, bgBase, alpha);
  RenderUtils::drawCircle(x + 9.0f + t * 16.0f, y + h / 2.0f,
                          (h - 4.0f) / 2.0f, 0xFFFFFFFF, alpha);
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
    RenderUtils::drawRoundedRect(x, y + h/2.0f - 2.0f, w, 4.0f, 2.0f, 0xFF35353A, alpha);
    RenderUtils::drawRoundedRect(x, y + h/2.0f - 2.0f, w * pct, 4.0f, 2.0f, ClickGUITheme::accent(), alpha);
    float kx = x + w * pct;
    RenderUtils::drawCircle(kx, y + h/2.0f, knobR, ClickGUITheme::accent(), alpha);
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

  DWORD bd = border();
  DWORD bg = panelBg();
  RenderUtils::drawRoundedRect(x - 1, y - 1, w + 2, h + 2, r,
                                bd, colorA(bd) * alpha);
  RenderUtils::drawRoundedRect(x, y, w, h, r, bg, colorA(bg) * alpha);
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
                   float alpha) {
  using namespace ClickGUITheme;
  const float r = cardRadius();
  DWORD fill = hovered ? cardHover() : cardBg();

  if (style() == Style::LiquidGlass) {
    Render::LiquidGlass::drawRect(x, y, w, h, r, alpha, fill);
    
    if (hovered) {
      RenderUtils::drawRoundedRect(x + 5, y + 6, 3, h - 12, 1.5f,
                                    accent(), alpha);
    }
    return;
  }

  RenderUtils::drawRoundedRect(x, y, w, h, r, fill, colorA(fill) * alpha);
  if (hovered)
    RenderUtils::drawRect(x, y + 4, 3, h - 8, accent(), alpha);
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
  DWORD base = pressed ? cardHover() : (hovered ? accent() : cardBg());
  RenderUtils::drawRoundedRect(x, y, w, h, r, base, colorA(base) * alpha);
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
  RenderUtils::drawRoundedRect(x, y, w, h, h / 2.0f, 0xFF2C2C35, alpha);
  RenderUtils::drawRoundedRect(x + 4, y + 6, 3, h - 12, 1.5f, accent(), alpha);
}

void drawThemeBackground(float screenW, float screenH, float alpha) {
  return;
}

} // namespace ClickGUIHelpers
} // namespace Render
