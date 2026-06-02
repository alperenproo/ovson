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

  g_guiFont.drawString(cx, cy, "Stat Color Ranges",
                       applyAlpha(0xFFFFFFFF, alpha));
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
      s_cpEditRangeIdx = ri;
      s_colorPickerOpen = true;
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
      uint8_t mr = (r.color >> 16) & 0xFF;
      uint8_t mg = (r.color >> 8) & 0xFF;
      uint8_t mb = r.color & 0xFF;
      float rf = mr / 255.0f, gf = mg / 255.0f, bf = mb / 255.0f;
      float cmax = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
      float cmin = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
      float delta = cmax - cmin;
      s_cpVal = cmax;
      s_cpSat = (cmax > 0) ? delta / cmax : 0;
      if (delta < 0.001f)
        s_cpHue = 0;
      else if (cmax == rf)
        s_cpHue = fmodf((gf - bf) / delta, 6.0f) / 6.0f;
      else if (cmax == gf)
        s_cpHue = ((bf - rf) / delta + 2.0f) / 6.0f;
      else
        s_cpHue = ((rf - gf) / delta + 4.0f) / 6.0f;
      if (s_cpHue < 0)
        s_cpHue += 1.0f;
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
    s_colorPickerOpen = !s_colorPickerOpen;
    s_cpEditingField = 0;
    if (!s_colorPickerOpen)
      s_cpEditRangeIdx = -1;
  }

  float rstX = cx + addBtnW + 15;
  float rstW = 140.0f;
  bool hRst = isHovered(mx, my, rstX, cy, rstW, 30);
  glDisable(GL_TEXTURE_2D);
  if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
    RenderUtils::drawRoundedRect(rstX, cy, rstW, 30, 5.0f, 0xFF0A0A12, 0.55f * alpha);
    Render::LiquidGlass::drawRect(rstX, cy, rstW, 30, 5.0f, alpha, hRst ? 0xFF991111 : THEME_CARD);
  } else {
    RenderUtils::drawRoundedRect(rstX, cy, rstW, 30, 5.0f, hRst ? 0xFF991111 : THEME_CARD, alpha);
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
    float popH = 230;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(popX, popY, popW, popH, false, alpha);

    float svX = popX + 12;
    float svY = popY + 12;
    float svSize = 140.0f;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);

    {
      float hr = 1, hg = 1, hb = 1;
      float h6 = s_cpHue * 6.0f;
      int hi = (int)h6 % 6;
      float f = h6 - (int)h6;
      switch (hi) {
      case 0: hr = 1;     hg = f;     hb = 0;     break;
      case 1: hr = 1 - f; hg = 1;     hb = 0;     break;
      case 2: hr = 0;     hg = 1;     hb = f;     break;
      case 3: hr = 0;     hg = 1 - f; hb = 1;     break;
      case 4: hr = f;     hg = 0;     hb = 1;     break;
      case 5: hr = 1;     hg = 0;     hb = 1 - f; break;
      }

      glBegin(GL_QUADS);
      glColor4f(1.0f, 1.0f, 1.0f, alpha); glVertex2f(svX, svY);
      glColor4f(hr, hg, hb, alpha);       glVertex2f(svX + svSize, svY);
      glColor4f(hr, hg, hb, alpha);       glVertex2f(svX + svSize, svY + svSize);
      glColor4f(1.0f, 1.0f, 1.0f, alpha); glVertex2f(svX, svY + svSize);
      glEnd();
    }

    {
      glBegin(GL_QUADS);
      glColor4f(0.0f, 0.0f, 0.0f, 0.0f);  glVertex2f(svX, svY);
      glColor4f(0.0f, 0.0f, 0.0f, 0.0f);  glVertex2f(svX + svSize, svY);
      glColor4f(0.0f, 0.0f, 0.0f, alpha); glVertex2f(svX + svSize, svY + svSize);
      glColor4f(0.0f, 0.0f, 0.0f, alpha); glVertex2f(svX, svY + svSize);
      glEnd();
    }

    float cursorX = svX + s_cpSat * svSize;
    float cursorY = svY + (1.0f - s_cpVal) * svSize;
    glShadeModel(GL_FLAT);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    for (int a = 0; a < 16; ++a) {
      float angle = a * 6.2831853f / 16.0f;
      glVertex2f(cursorX + cosf(angle) * 4, cursorY + sinf(angle) * 4);
    }
    glEnd();
    glLineWidth(1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    if (isHovered(mx, my, svX, svY, svSize, svSize)) {
      if (lClick) s_cpDraggingSV = true;
    }
    if (s_cpDraggingSV) {
      if (lClick) {
        s_cpSat = (mx - svX) / svSize;
        s_cpVal = 1.0f - (my - svY) / svSize;
        if (s_cpSat < 0) s_cpSat = 0;
        if (s_cpSat > 1) s_cpSat = 1;
        if (s_cpVal < 0) s_cpVal = 0;
        if (s_cpVal > 1) s_cpVal = 1;
      } else {
        s_cpDraggingSV = false;
      }
    }

    float hueX = svX + svSize + 15;
    float hueY = svY;
    float hueW = 20.0f;
    float hueH = svSize;
    int hueSteps = 24;
    float stepH = hueH / hueSteps;
    for (int i = 0; i < hueSteps; ++i) {
      float h1 = (float)i / hueSteps;
      float h2 = (float)(i + 1) / hueSteps;
      float r1, g1, b1, r2, g2, b2;
      auto hsvRgb = [](float h, float &r, float &g, float &b) {
        float h6 = h * 6.0f;
        int hi = (int)h6 % 6;
        float f = h6 - (int)h6;
        switch (hi) {
        case 0: r = 1;     g = f;     b = 0;     break;
        case 1: r = 1 - f; g = 1;     b = 0;     break;
        case 2: r = 0;     g = 1;     b = f;     break;
        case 3: r = 0;     g = 1 - f; b = 1;     break;
        case 4: r = f;     g = 0;     b = 1;     break;
        case 5: r = 1;     g = 0;     b = 1 - f; break;
        }
      };
      hsvRgb(h1, r1, g1, b1);
      hsvRgb(h2, r2, g2, b2);
      glBegin(GL_QUADS);
      glColor4f(r1, g1, b1, alpha); glVertex2f(hueX, hueY + i * stepH);
      glColor4f(r1, g1, b1, alpha); glVertex2f(hueX + hueW, hueY + i * stepH);
      glColor4f(r2, g2, b2, alpha); glVertex2f(hueX + hueW, hueY + (i + 1) * stepH);
      glColor4f(r2, g2, b2, alpha); glVertex2f(hueX, hueY + (i + 1) * stepH);
      glEnd();
    }

    float hueCurY = hueY + s_cpHue * hueH;
    glColor4f(1, 1, 1, alpha);
    glBegin(GL_LINES);
    glVertex2f(hueX - 2, hueCurY);
    glVertex2f(hueX + hueW + 2, hueCurY);
    glEnd();

    if (isHovered(mx, my, hueX - 4, hueY, hueW + 8, hueH) &&
        !s_cpDraggingSV) {
      if (lClick) s_cpDraggingHue = true;
    }
    if (s_cpDraggingHue) {
      if (lClick) {
        s_cpHue = (my - hueY) / hueH;
        if (s_cpHue < 0)     s_cpHue = 0;
        if (s_cpHue > 0.999f) s_cpHue = 0.999f;
      } else {
        s_cpDraggingHue = false;
      }
    }

    glEnable(GL_TEXTURE_2D);

    float rpX = hueX + hueW + 20;
    float rpY = popY + 12;
    auto hsvToRgb32 = [](float h, float s, float v) -> uint32_t {
      float h6 = h * 6.0f;
      int hi = (int)h6 % 6;
      float f = h6 - (int)h6;
      float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
      float r, g, b;
      switch (hi) {
      case 0:  r = v; g = t; b = p; break;
      case 1:  r = q; g = v; b = p; break;
      case 2:  r = p; g = v; b = t; break;
      case 3:  r = p; g = q; b = v; break;
      case 4:  r = t; g = p; b = v; break;
      default: r = v; g = p; b = q; break;
      }
      return 0xFF000000 | ((uint8_t)(r * 255) << 16) |
             ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
    };

    uint32_t previewColor = hsvToRgb32(s_cpHue, s_cpSat, s_cpVal);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(rpX, rpY, 60, 30, 4.0f, previewColor, alpha);
    glEnable(GL_TEXTURE_2D);
    char rgbBuf[32];
    snprintf(rgbBuf, sizeof(rgbBuf), "R:%d G:%d B:%d",
             (previewColor >> 16) & 0xFF, (previewColor >> 8) & 0xFF,
             previewColor & 0xFF);
    g_guiFont.drawString(rpX, rpY + 38, rgbBuf, applyAlpha(0xFFA0A0A5, alpha),
                         0.38f);
    char hexBuf2[12];
    snprintf(hexBuf2, sizeof(hexBuf2), "#%02X%02X%02X",
             (previewColor >> 16) & 0xFF, (previewColor >> 8) & 0xFF,
             previewColor & 0xFF);
    g_guiFont.drawString(rpX + 70, rpY + 8, hexBuf2,
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    rpY += 58;
    g_guiFont.drawString(rpX, rpY,
                         "MC Colors:", applyAlpha(0xFF808085, alpha), 0.38f);
    rpY += 18;
    struct McPreset {
      const char *name;
      uint32_t color;
    };
    McPreset mcPresets[] = {
        {"0", 0xFF000000}, {"1", 0xFF0000AA}, {"2", 0xFF00AA00},
        {"3", 0xFF00AAAA}, {"4", 0xFFAA0000}, {"5", 0xFFAA00AA},
        {"6", 0xFFFFAA00}, {"7", 0xFFAAAAAA}, {"8", 0xFF555555},
        {"9", 0xFF5555FF}, {"a", 0xFF55FF55}, {"b", 0xFF55FFFF},
        {"c", 0xFFFF5555}, {"d", 0xFFFF55FF}, {"e", 0xFFFFFF55},
        {"f", 0xFFFFFFFF},
    };
    float mcX = rpX;
    for (int i = 0; i < 16; ++i) {
      bool hMc = isHovered(mx, my, mcX, rpY, 14, 14);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(mcX, rpY, 14, 14, 2.0f, mcPresets[i].color,
                                   alpha);
      if (hMc)
        RenderUtils::drawRoundedRect(mcX - 1, rpY - 1, 16, 16, 2.0f,
                                     0xFFFFFFFF, 0.5f * alpha);
      glEnable(GL_TEXTURE_2D);
      if (clickEvent && hMc) {
        uint8_t mr = (mcPresets[i].color >> 16) & 0xFF;
        uint8_t mg = (mcPresets[i].color >> 8) & 0xFF;
        uint8_t mb = mcPresets[i].color & 0xFF;
        float rf = mr / 255.0f, gf = mg / 255.0f, bf = mb / 255.0f;
        float cmax =
            (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
        float cmin =
            (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
        float delta = cmax - cmin;
        s_cpVal = cmax;
        s_cpSat = (cmax > 0) ? delta / cmax : 0;
        if (delta < 0.001f)
          s_cpHue = 0;
        else if (cmax == rf)
          s_cpHue = fmodf((gf - bf) / delta, 6.0f) / 6.0f;
        else if (cmax == gf)
          s_cpHue = ((bf - rf) / delta + 2.0f) / 6.0f;
        else
          s_cpHue = ((rf - gf) / delta + 4.0f) / 6.0f;
        if (s_cpHue < 0)
          s_cpHue += 1.0f;
      }
      mcX += 17;
      if (i == 7) {
        mcX = rpX;
        rpY += 17;
      }
    }

    rpY += 22;

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
        Config::save();
        NotificationManager::getInstance()->add(
            "Colors",
            s_cpEditRangeIdx >= 0 ? "Range updated!" : "Range added!",
            NotificationType::Success);
        s_cpEditRangeIdx = -1;
        if (s_cpEditRangeIdx >= 0)
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
