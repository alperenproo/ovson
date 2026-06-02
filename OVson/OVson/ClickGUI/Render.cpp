#include "ClickGUI.h"
#include "State.h"
#include "Theme.h"
#include "Helpers.h"
#include "Tabs/Tabs.h"
#include "../Render/RenderUtils.h"
#include "../Render/RenderHook.h"
#include "LiquidGlass.h"
#include "../Render/NotificationManager.h"
#include "../Config/Config.h"
#include "../Utils/GlGuard.h"
#include "../Utils/SensitivityFix.h"
#include "../Utils/Timer.h"
#include <Windows.h>
#include <cmath>
#include <gl/GL.h>

namespace Render {

using namespace ClickGUIState;
using ClickGUITheme::Style;
using ClickGUITheme::style;
using ClickGUITheme::accent;
using ClickGUITheme::panelRadius;
using ClickGUITheme::textPrimary;
using ClickGUITheme::textSecondary;
using ClickGUITheme::textMuted;
using ClickGUITheme::easeOut;

void ClickGUI::render(HDC hdc) {
  HWND hwnd = WindowFromDC(hdc);
  updateInput(hwnd);

  if (s_open) {
    static int focusTick = 0;
    if (focusTick++ % 20 == 0) {
      FocusFix::setIngameFocus(false);
      setMouseGrabbed(false);
    }
  }

  float alphaDiff = s_targetAlpha - s_animAlpha;
  s_animAlpha += alphaDiff * 0.18f;
  s_openingScale = 0.94f + 0.06f * easeOut(s_animAlpha);

  if (s_animAlpha <= 0.001f && !s_open)
    return;

  if (!g_guiFont.isInitialized()) {
    g_guiFont.init(hdc);
  }

  GlGuard::GlMatrixGuard _gMv(GL_MODELVIEW);
  GlGuard::GlAttribGuard _gAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  RECT cr;
  GetClientRect(hwnd, &cr);
  float sw = (float)cr.right;
  float sh = (float)cr.bottom;

  if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
    Render::LiquidGlass::updateTime(RenderHook::getDelta());
    Render::LiquidGlass::beginFrame((int)sw, (int)sh);
  }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, sw, sh, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd, &pt);
  float mx = (float)pt.x;
  float my = (float)pt.y;
  bool lClick = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  bool clickEvent = lClick && !s_lastLButton;
  s_lastLButton = lClick;

  DWORD dim = (style() == Style::LiquidGlass) ? 0x00000000 : 0xB0000000;
  if (dim != 0) {
    RenderUtils::drawRect(0, 0, sw, sh, applyAlpha(dim, s_animAlpha));
  }

  drawThemeBackground(sw, sh, s_animAlpha);

  GlGuard::GlMatrixGuard _gMvInner(GL_MODELVIEW);
  float centerX = g_x + g_w / 2;
  float centerY = g_y + g_h / 2;
  glTranslatef(centerX, centerY, 0);
  glScalef(s_openingScale, s_openingScale, 1.0f);
  glTranslatef(-centerX, -centerY, 0);

  if (s_open && s_animAlpha >= 0.95f) {
    if (lClick) {
      if (!s_dragging) {
        if (isHovered(mx, my, g_x, g_y, g_w, 60)) {
          s_dragging = true;
          s_dragOffsetX = mx - g_x;
          s_dragOffsetY = my - g_y;
        }
      } else {
        g_x = mx - s_dragOffsetX;
        g_y = my - s_dragOffsetY;
      }
    } else {
      s_dragging = false;
    }
  }

  if (s_open && s_waitingForKey) {
    for (int k = 1; k < 255; ++k) {
      if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON)
        continue;
      if ((GetAsyncKeyState(k) & 0x8000) != 0) {
        if (k == VK_ESCAPE) {
          s_waitingForKey = false;
        } else {
          Config::setClickGuiKey(k);
          Config::save();
          NotificationManager::getInstance()->add(
              "Settings", "Bind set to " + getKeyName(k),
              NotificationType::Success);
          s_waitingForKey = false;
        }
        break;
      }
    }
  }

  const float mainX = g_x;
  const float mainY = g_y;
  const float sidebarW = 180.0f;  // wider sidebar for the redesign

  drawThemePanel(mainX, mainY, g_w, g_h, s_animAlpha);
  drawThemeSidebar(mainX, mainY, sidebarW, g_h, s_animAlpha);

  RenderUtils::drawRect(mainX + sidebarW, mainY + 60, g_w - sidebarW, 1,
                        applyAlpha(0x18FFFFFF, s_animAlpha));

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(mainX + 26, mainY + 22.0f, "OVSON",
                       applyAlpha(textPrimary(), s_animAlpha));
  g_guiFont.drawString(mainX + 26, mainY + 40.0f, "CLIENT",
                       applyAlpha(accent(), s_animAlpha), 0.45f);

  {
    float closeSize = 28.0f;
    float closeX = mainX + g_w - 16.0f - closeSize;
    float closeY = mainY + 16.0f;
    bool hovClose = isHovered(mx, my, closeX, closeY, closeSize, closeSize);

    glDisable(GL_TEXTURE_2D);
    if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
      Render::LiquidGlass::drawRect(closeX, closeY, closeSize, closeSize, 8.0f, s_animAlpha, hovClose ? 0xFFE03A4F : 0x00000000);
    } else {
      DWORD bg = hovClose ? 0xFFE03A4F : 0x15FFFFFF;
      RenderUtils::drawRoundedRect(closeX, closeY, closeSize, closeSize, 8.0f, applyAlpha(bg, s_animAlpha), -1.0f);
    }
    glEnable(GL_TEXTURE_2D);

    const char *glyph = "X";
    float glyphW = g_guiFont.getStringWidth(glyph);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.5f);
    glDisable(GL_TEXTURE_2D);

    glColor4f(1.0f, 1.0f, 1.0f, s_animAlpha * (hovClose ? 1.0f : 0.8f));
    float pad = 9.0f;
    glBegin(GL_LINES);
    glVertex2f(closeX + pad, closeY + pad);
    glVertex2f(closeX + closeSize - pad, closeY + closeSize - pad);
    glVertex2f(closeX + closeSize - pad, closeY + pad);
    glVertex2f(closeX + pad, closeY + closeSize - pad);
    glEnd();

    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hovClose) {
      toggle();
    }
  }

  const float tabStartY = 96.0f;
  const float tabRowH = 48.0f;
  float targetY = tabStartY + (s_targetTab * tabRowH);
  s_tabIndicatorY += (targetY - s_tabIndicatorY) * 0.22f;

  glDisable(GL_TEXTURE_2D);
  drawThemeTabIndicator(mainX + 12, mainY + s_tabIndicatorY - 10.0f,
                         sidebarW - 24, 42.0f, s_animAlpha);
  glEnable(GL_TEXTURE_2D);

  if (s_activeTab != s_targetTab) {
    s_contentAlpha -= 0.20f;
    if (s_contentAlpha <= 0.0f) {
      s_activeTab = s_targetTab;
      s_contentSlide = 18.0f;
      s_targetScroll = 0.0f;
      s_scrollOffset = 0.0f;
    }
  } else {
    s_contentAlpha += 0.20f;
    if (s_contentAlpha > 1.0f)
      s_contentAlpha = 1.0f;
    s_contentSlide += (0.0f - s_contentSlide) * 0.18f;
  }

  if (s_targetScroll < 0)
    s_targetScroll = 0;
  if (s_targetScroll > s_maxScroll)
    s_targetScroll = s_maxScroll;
  s_scrollOffset += (s_targetScroll - s_scrollOffset) * 0.18f;

  const char *tabs[] = {"Visuals", "Players", "Tags",  "Settings",
                        "Colors",  "Debug",   "Utils", nullptr};
  float ty = mainY + tabStartY;
  for (int i = 0; tabs[i]; ++i) {
    bool hover = isHovered(mx, my, mainX + 12, ty - 10, sidebarW - 24, 42);
    
    if (hover) {
        if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
            if (s_targetTab != i) {
                glDisable(GL_TEXTURE_2D);
                Render::LiquidGlass::drawRect(mainX + 12, ty - 10, sidebarW - 24, 42, 21.0f, 0.8f * s_animAlpha, ClickGUITheme::cardHover());
                glEnable(GL_TEXTURE_2D);
            }
        } else {
            if (s_targetTab != i) {
                glDisable(GL_TEXTURE_2D);
                RenderUtils::drawRoundedRect(mainX + 12, ty - 10, sidebarW - 24, 42, 21.0f,
                                             0xFF2C2C35, s_animAlpha);
                glEnable(GL_TEXTURE_2D);
            }
        }
    }

    DWORD col = (s_targetTab == i) ? textPrimary()
                : hover            ? 0xFFE5E5EA
                                   : textSecondary();
    g_guiFont.drawString(mainX + 36, ty, tabs[i],
                         applyAlpha(col, s_animAlpha));
    if (clickEvent && hover) {
      s_targetTab = i;
      s_isDropdownOpen = false;
    }
    ty += tabRowH;
  }

  float cx = mainX + sidebarW + 30 + s_contentSlide;
  float startCy = mainY + 86;
  float cy = startCy - s_scrollOffset;
  float alpha = s_animAlpha * s_contentAlpha;

  glEnable(GL_SCISSOR_TEST);
  glScissor((int)(mainX + sidebarW), (int)(sh - (mainY + g_h - 10)),
            (int)(g_w - sidebarW), (int)(g_h - 70));

  TabCtx ctx{hwnd, mainX, mainY, cx, startCy, cy,
             mx, my, lClick, clickEvent, alpha};

  switch (s_activeTab) {
  case 0: Tabs::renderVisuals (ctx); break;
  case 1: Tabs::renderPlayers (ctx); break;
  case 2: Tabs::renderTags    (ctx); break;
  case 3: Tabs::renderSettings(ctx); break;
  case 4: Tabs::renderColors  (ctx); break;
  case 5: Tabs::renderDebug   (ctx); break;
  case 6: Tabs::renderUtils   (ctx); break;
  default: break;
  }

  float contentHeight = (cy + s_scrollOffset) - startCy;
  float visibleHeight = g_h - 110.0f;
  s_maxScroll = (contentHeight > visibleHeight)
                    ? (contentHeight - visibleHeight + 40.0f)
                    : 0.0f;

  glDisable(GL_SCISSOR_TEST);
}

} // namespace Render
