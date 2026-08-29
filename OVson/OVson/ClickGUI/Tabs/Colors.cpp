#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../LiquidGlass.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Config/StatColors.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gl/GL.h>
#include <string>

namespace Render {
namespace Tabs {

void renderColors(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  lClick = ctx.lClick;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;
  static std::uint32_t statPickerColor = 0xFF3D6EF5u;

  {
    drawSectionLabel(cx, cy, "Accent Color", alpha);
    cy += 26.0f;
    std::uint32_t accentColor = Config::getThemeColor();
    bool rainbow = Config::isChromaEnabled();
    const bool colorChanged =
        drawColorPicker(9000, cx, cy, (std::max)(180.0f, g_w - 230.0f),
                        accentColor, mx, my, lClick, clickEvent, alpha,
                        &rainbow);
    if (colorChanged) {
      Config::setThemeColor(accentColor);
      // Chroma animates from the selected accent's HSV state. Force the
      // existing animation path to resync after the shared picker changes it.
      s_accentInit = false;
    }
    if (rainbow != Config::isChromaEnabled())
      Config::setChromaEnabled(rainbow);
    cy += colorPickerHeight(true) + 12.0f;
    if (rainbow) {
      g_guiFont.drawString(cx, cy + 2.0f, "Rainbow speed",
                           applyAlpha(0xFFA0A0A5, alpha), 0.4f);
      float speed = Config::getChromaSpeed();
      bool speedChanged = drawSlider(901, cx + 110.0f, cy, 150.0f, 14.0f,
                                     speed, 10.0f, 180.0f, mx, my, lClick,
                                     alpha);
      speedChanged =
          drawNumericInput(901, cx + 270.0f, cy - 5.0f, 62.0f, 25.0f,
                           speed, 10.0f, 180.0f, 0, "/s", mx, my,
                           clickEvent, alpha) || speedChanged;
      if (speedChanged)
        Config::setChromaSpeed(speed);
      cy += 34.0f;
    }
  }


  drawSectionLabel(cx, cy, "Stat Color Ranges", alpha);
  cy += 35;

  const int statCount = (int)StatColors::StatType::COUNT;
  float btnW = 55.0f;
  float btnH = 26.0f;
  float btnX = cx;
  for (int i = 0; i < statCount; ++i) {
    if ((StatColors::StatType)i == StatColors::StatType::Star) {
      if (s_colorSelectedStat == i)
        s_colorSelectedStat = 1;
      continue;
    }
    const char *sName = StatColors::getStatName((StatColors::StatType)i);
    bool sel = (s_colorSelectedStat == i);
    bool hov = isHovered(mx, my, btnX, cy, btnW, btnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(btnX, cy, btnW, btnH, hov, sel, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(btnX + 5, cy + 4, sName,
                         applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha),
                         0.4f);
    if (clickEvent && hov) {
      s_colorSelectedStat = i;
      s_colorPickerOpen = false;
      s_cpEditRangeIdx = -1;
    }
    btnX += btnW + 6;
    if (btnX + btnW > mainX + g_w - 30) {
      btnX = cx;
      cy += btnH + 6;
    }
  }
  cy += btnH + 20;

  auto &cfg =
      StatColors::getConfig((StatColors::StatType)s_colorSelectedStat);
  g_guiFont.drawString(cx, cy,
                       (std::string(cfg.name) + " Color Ranges:").c_str(),
                       applyAlpha(0xFFA0A0A5, alpha));
  cy += 25;

  for (int ri = 0; ri < (int)cfg.ranges.size(); ++ri) {
    const auto &r = cfg.ranges[ri];
    float rowY = cy;
    float rowW = g_w - 230;

    bool hRow = isHovered(mx, my, cx, rowY, rowW, 28);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cx, rowY, rowW, 28, hRow, alpha);
    RenderUtils::drawRoundedRect(cx + 4, rowY + 4, 20, 20, 3.0f, r.color,
                                 alpha);
    glEnable(GL_TEXTURE_2D);

    char rangeBuf[64];
    bool isRatio = (s_colorSelectedStat == (int)StatColors::StatType::FKDR ||
                    s_colorSelectedStat == (int)StatColors::StatType::KDR ||
                    s_colorSelectedStat == (int)StatColors::StatType::WLR ||
                    s_colorSelectedStat == (int)StatColors::StatType::BLR);

    if (r.maxVal >= 1e300) {
      if (isRatio)
        snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - INF", r.minVal);
      else
        snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - INF", r.minVal);
    } else {
      if (isRatio)
        snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - %.2f", r.minVal,
                 r.maxVal);
      else
        snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - %.0f", r.minVal,
                 r.maxVal);
    }
    g_guiFont.drawString(cx + 30, rowY + 5, rangeBuf,
                         applyAlpha(0xFFFFFFFF, alpha), 0.4f);

    char hexBuf[12];
    snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", (r.color >> 16) & 0xFF,
             (r.color >> 8) & 0xFF, r.color & 0xFF);
    g_guiFont.drawString(cx + 160, rowY + 5, hexBuf,
                         applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    const char *mcName = StatColors::rgbToMcColor(r.color);
    g_guiFont.drawString(cx + 240, rowY + 5, mcName,
                         applyAlpha(0xFF808085, alpha), 0.35f);

    float editX = cx + rowW - 65;
    bool hEdit = isHovered(mx, my, editX, rowY + 2, 32, 24);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(editX, rowY + 2, 32, 24, hEdit, s_cpEditRangeIdx == ri, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(editX + 4, rowY + 3, "Edit",
                         applyAlpha(0xFFFFFFFF, alpha), 0.35f);
    if (clickEvent && hEdit) {
      ClickGUIHelpers::cancelInlineEditors();
      s_cpEditRangeIdx = ri;
      s_colorPickerOpen = true;
      statPickerColor = r.color;
      bool isRatio2 =
          (s_colorSelectedStat == (int)StatColors::StatType::FKDR ||
           s_colorSelectedStat == (int)StatColors::StatType::KDR ||
           s_colorSelectedStat == (int)StatColors::StatType::WLR ||
           s_colorSelectedStat == (int)StatColors::StatType::BLR);

      if (isRatio2)
        snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.2f", r.minVal);
      else
        snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.0f", r.minVal);

      s_cpMinLen = (int)strlen(s_cpMinBuf);
      if (r.maxVal >= 1e300) {
        s_cpMaxBuf[0] = 0;
        s_cpMaxLen = 0;
      } else {
        if (isRatio2)
          snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.2f", r.maxVal);
        else
          snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.0f", r.maxVal);
        s_cpMaxLen = (int)strlen(s_cpMaxBuf);
      }
    }

    float delX = cx + rowW - 28;
    bool hDel = isHovered(mx, my, delX, rowY + 2, 24, 24);
    glDisable(GL_TEXTURE_2D);
    if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
      RenderUtils::drawRoundedRect(delX, rowY + 2, 24, 24, 3.0f, 0xFF0A0A12, 0.55f * alpha);
      Render::LiquidGlass::drawRect(delX, rowY + 2, 24, 24, 3.0f, alpha, hDel ? 0xFF991111 : 0xFF505055);
    } else {
      RenderUtils::drawRoundedRect(delX, rowY + 2, 24, 24, 3.0f, hDel ? 0xFF991111 : 0xFF505055, alpha);
    }
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(delX + 7, rowY + 3, "X",
                         applyAlpha(hDel ? 0xFFFFFFFF : 0xFFFF5555, alpha),
                         0.4f);
    if (clickEvent && hDel) {
      StatColors::removeRange((StatColors::StatType)s_colorSelectedStat, ri);
      Config::save();
      NotificationManager::getInstance()->add("Colors", "Range removed",
                                              NotificationType::Info);
      if (s_cpEditRangeIdx == ri) {
        s_cpEditRangeIdx = -1;
        s_colorPickerOpen = false;
      }
      break;
    }

    cy += 32;
  }

  cy += 15;

  float addBtnW = 160.0f;
  bool hAdd = isHovered(mx, my, cx, cy, addBtnW, 30);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, addBtnW, 30, hAdd, s_colorPickerOpen, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx + 10, cy + 6,
                       s_colorPickerOpen ? "- Close Picker" : "+ Add Range",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  if (clickEvent && hAdd) {
    ClickGUIHelpers::cancelInlineEditors();
    const bool openingNew = !s_colorPickerOpen && s_cpEditRangeIdx < 0;
    s_colorPickerOpen = !s_colorPickerOpen;
    s_cpEditingField = 0;
    if (openingNew) {
      statPickerColor = Config::getThemeColor();
      std::snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "0");
      std::snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "100");
      s_cpMinLen = 1;
      s_cpMaxLen = 3;
    }
    if (!s_colorPickerOpen)
      s_cpEditRangeIdx = -1;
  }

  float rstX = cx + addBtnW + 15;
  float rstW = 140.0f;
  bool hRst = isHovered(mx, my, rstX, cy, rstW, 30);
  glDisable(GL_TEXTURE_2D);
  DWORD rstCol = hRst ? 0xFF991111 : THEME_CARD;
  float rstAlpha = (((rstCol >> 24) & 0xFF) / 255.0f) * alpha;
  if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
    RenderUtils::drawRoundedRect(rstX, cy, rstW, 30, 5.0f, 0xFF0A0A12, 0.55f * alpha);
    Render::LiquidGlass::drawRect(rstX, cy, rstW, 30, 5.0f, alpha, rstCol);
  } else {
    RenderUtils::drawRoundedRect(rstX, cy, rstW, 30, 5.0f, rstCol, rstAlpha);
  }
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(rstX + 8, cy + 6, "Reset Defaults",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  if (clickEvent && hRst) {
    StatColors::resetToDefaults((StatColors::StatType)s_colorSelectedStat);
    Config::save();
    NotificationManager::getInstance()->add("Colors", "Reset to defaults",
                                            NotificationType::Success);
  }
  cy += 40;

  if (s_colorPickerOpen) {
    float popX = mainX + 185;
    float popY = cy;
    float popW = g_w - 205;
    float popH = 310;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(popX, popY, popW, popH, false, alpha);
    glEnable(GL_TEXTURE_2D);
    drawColorPicker(9100 + s_colorSelectedStat, popX + 12.0f,
                    popY + 12.0f, popW - 24.0f, statPickerColor, mx, my,
                    lClick, clickEvent, alpha);
    std::uint32_t previewColor = statPickerColor;
    float rpX = popX + 12.0f;
    float rpY = popY + 12.0f + colorPickerHeight(false) + 12.0f;


    bool showCursor = (GetTickCount64() / 500) % 2 == 0;

    g_guiFont.drawString(rpX, rpY, "Min:", applyAlpha(0xFFA0A0A5, alpha),
                         0.38f);
    float minBoxX = rpX + 30;
    bool hMinBox = isHovered(mx, my, minBoxX, rpY - 3, 55, 20);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(minBoxX, rpY - 3, 55, 20, s_cpEditingField == 1, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(minBoxX + 4, rpY - 1, s_cpMinBuf,
                         applyAlpha(0xFFFFFFFF, alpha), 0.38f);
    if (s_cpEditingField == 1 && showCursor) {
      float tw = (g_guiFont.getStringWidth(s_cpMinBuf) / 0.5f) * 0.38f;
      glDisable(GL_TEXTURE_2D);
      glColor4f(1, 1, 1, alpha);
      glBegin(GL_LINES);
      glVertex2f(minBoxX + 4 + tw, rpY - 1);
      glVertex2f(minBoxX + 4 + tw, rpY + 13);
      glEnd();
      glEnable(GL_TEXTURE_2D);
    }
    if (clickEvent && hMinBox)
      s_cpEditingField = 1;

    g_guiFont.drawString(rpX + 95, rpY, "Max:", applyAlpha(0xFFA0A0A5, alpha),
                         0.38f);
    float maxBoxX = rpX + 125;
    bool hMaxBox = isHovered(mx, my, maxBoxX, rpY - 3, 55, 20);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(maxBoxX, rpY - 3, 55, 20, s_cpEditingField == 2, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(maxBoxX + 4, rpY - 1, s_cpMaxBuf,
                         applyAlpha(0xFFFFFFFF, alpha), 0.38f);
    if (s_cpEditingField == 2 && showCursor) {
      float tw = (g_guiFont.getStringWidth(s_cpMaxBuf) / 0.5f) * 0.38f;
      glDisable(GL_TEXTURE_2D);
      glColor4f(1, 1, 1, alpha);
      glBegin(GL_LINES);
      glVertex2f(maxBoxX + 4 + tw, rpY - 1);
      glVertex2f(maxBoxX + 4 + tw, rpY + 13);
      glEnd();
      glEnable(GL_TEXTURE_2D);
    }
    if (clickEvent && hMaxBox)
      s_cpEditingField = 2;

    if (clickEvent && !hMinBox && !hMaxBox)
      s_cpEditingField = 0;

    rpY += 28;

    float addW2 = 120.0f;
    bool hAdd2 = isHovered(mx, my, rpX, rpY, addW2, 26);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(rpX, rpY, addW2, 26, hAdd2, false, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(rpX + 12, rpY + 4,
                         s_cpEditRangeIdx >= 0 ? "Save Changes" : "Add Range",
                         applyAlpha(0xFFFFFFFF, alpha), 0.4f);

    if (clickEvent && hAdd2) {
      double minV = atof(s_cpMinBuf);
      double maxV = atof(s_cpMaxBuf);
      if (maxV <= 0 || strlen(s_cpMaxBuf) == 0)
        maxV = 1e308;

      bool success = false;
      if (s_cpEditRangeIdx >= 0) {
        success = StatColors::updateRange(
            (StatColors::StatType)s_colorSelectedStat, s_cpEditRangeIdx, minV,
            maxV, previewColor);
      } else {
        success =
            StatColors::addRange((StatColors::StatType)s_colorSelectedStat,
                                 minV, maxV, previewColor);
      }

      if (success) {
        const bool wasEditing = s_cpEditRangeIdx >= 0;
        Config::save();
        NotificationManager::getInstance()->add(
            "Colors",
            s_cpEditRangeIdx >= 0 ? "Range updated!" : "Range added!",
            NotificationType::Success);
        s_cpEditRangeIdx = -1;
        if (wasEditing)
          s_colorPickerOpen = false;
      } else {
        NotificationManager::getInstance()->add(
            "Colors", "Overlap! Check existing ranges.",
            NotificationType::Error);
      }
    }

    cy += popH + 10;
  }

  cy += 30;
}

} // namespace Tabs
} // namespace Render
