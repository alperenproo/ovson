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


  cy += 14.0f;
  drawSectionLabel(cx, cy, "Stat Color Ranges", alpha);
  cy += 32.0f;

  const float contentW = (std::max)(300.0f, g_w - 230.0f);

  const int statCount = (int)StatColors::StatType::COUNT;
  float btnW = 56.0f;
  float btnH = 26.0f;
  float btnX = cx;
  for (int i = 0; i < statCount; ++i) {
    if ((StatColors::StatType)i == StatColors::StatType::Star) {
      if (s_colorSelectedStat == i)
        s_colorSelectedStat = (int)StatColors::StatType::FinalKills;
      continue;
    }
    const char *sName = StatColors::getStatName((StatColors::StatType)i);
    bool sel = (s_colorSelectedStat == i);
    bool hov = isHovered(mx, my, btnX, cy, btnW, btnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(btnX, cy, btnW, btnH, hov, sel, alpha);
    if (sel) {
      RenderUtils::drawRoundedRect(btnX + 6.0f, cy + btnH - 2.5f, btnW - 12.0f, 2.0f, 1.0f,
                                   ClickGUITheme::accent(), alpha);
    }
    glEnable(GL_TEXTURE_2D);
    float tw = g_guiFont.getStringWidth(sName) * (0.38f / 0.5f);
    float tx = btnX + (btnW - tw) * 0.5f;
    float ty = cy + (btnH - 9.0f) * 0.5f;
    g_guiFont.drawString(tx, ty, sName,
                         applyAlpha(sel ? 0xFFFFFFFF : (hov ? 0xFFEEEEEE : 0xFF8A8A92), alpha),
                         0.38f);
    if (clickEvent && hov) {
      s_colorSelectedStat = i;
      s_colorPickerOpen = false;
      s_cpEditRangeIdx = -1;
      s_cpEditingField = 0;
    }
    btnX += btnW + 6.0f;
    if (btnX + btnW > cx + contentW) {
      btnX = cx;
      cy += btnH + 6.0f;
    }
  }
  cy += btnH + 20.0f;

  auto &cfg = StatColors::getConfig((StatColors::StatType)s_colorSelectedStat);
  const bool isRatio = (s_colorSelectedStat == (int)StatColors::StatType::FKDR ||
                        s_colorSelectedStat == (int)StatColors::StatType::KDR ||
                        s_colorSelectedStat == (int)StatColors::StatType::WLR ||
                        s_colorSelectedStat == (int)StatColors::StatType::BLR);

  {
    std::string titleStr = std::string(cfg.name) + " Color Ranges";
    g_guiFont.drawString(cx, cy + 2.0f, titleStr.c_str(), applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    float titleW = g_guiFont.getStringWidth(titleStr.c_str()) * (0.44f / 0.5f);
    std::string countStr = std::to_string(cfg.ranges.size()) + " ranges configured";
    g_guiFont.drawString(cx + titleW + 12.0f, cy + 4.0f, countStr.c_str(),
                         applyAlpha(0xFF7A7A84, alpha), 0.35f);
    cy += 28.0f;
  }

  const float rowH = 36.0f;
  const float rowW = contentW;

  for (int ri = 0; ri < (int)cfg.ranges.size(); ++ri) {
    const auto &r = cfg.ranges[ri];
    float rowY = cy;
    bool isEditingThis = (s_colorPickerOpen && s_cpEditRangeIdx == ri);
    bool hRow = isHovered(mx, my, cx, rowY, rowW, rowH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cx, rowY, rowW, rowH, hRow, alpha, isEditingThis);
    if (isEditingThis) {
      RenderUtils::drawRoundedOutline(cx, rowY, rowW, rowH, 6.0f, 1.5f,
                                      ClickGUITheme::accent(), 0.85f * alpha);
    }

    const float swSize = 22.0f;
    const float swX = cx + 8.0f;
    const float swY = rowY + (rowH - swSize) * 0.5f;
    RenderUtils::drawGlow(swX, swY, swSize, swSize, 5.0f, r.color, 0.25f * alpha);
    RenderUtils::drawRoundedRect(swX, swY, swSize, swSize, 5.0f, r.color, alpha);
    RenderUtils::drawRoundedOutline(swX, swY, swSize, swSize, 5.0f, 1.0f, 0x30FFFFFF, 0.5f * alpha);
    glEnable(GL_TEXTURE_2D);

    char rangeBuf[64];
    if (r.maxVal >= 1e300) {
      if (isRatio)
        snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - INF", r.minVal);
      else
        snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - INF", r.minVal);
    } else {
      if (isRatio)
        snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - %.2f", r.minVal, r.maxVal);
      else
        snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - %.0f", r.minVal, r.maxVal);
    }

    float rangeStrW = g_guiFont.getStringWidth(rangeBuf) * (0.38f / 0.5f);
    float badgeW = rangeStrW + 18.0f;
    float badgeH = 22.0f;
    float badgeX = swX + swSize + 10.0f;
    float badgeY = rowY + (rowH - badgeH) * 0.5f;

    glDisable(GL_TEXTURE_2D);
    DWORD badgeBg = ClickGUITheme::inset();
    RenderUtils::drawRoundedRect(badgeX, badgeY, badgeW, badgeH, 4.0f, badgeBg, 0.85f * alpha);
    RenderUtils::drawRoundedOutline(badgeX, badgeY, badgeW, badgeH, 4.0f, 1.0f,
                                    ClickGUITheme::hairline(), 0.5f * alpha);
    glEnable(GL_TEXTURE_2D);

    float rtx = badgeX + (badgeW - rangeStrW) * 0.5f;
    float rty = badgeY + (badgeH - 9.0f) * 0.5f;
    g_guiFont.drawString(rtx, rty, rangeBuf, applyAlpha(0xFFFFFFFF, alpha), 0.38f);

    char hexBuf[16];
    snprintf(hexBuf, sizeof(hexBuf), "#%06X", r.color & 0xFFFFFF);
    float hexX = badgeX + badgeW + 18.0f;
    float hexY = rowY + (rowH - 9.0f) * 0.5f;
    g_guiFont.drawString(hexX, hexY, hexBuf, applyAlpha(0xFFA5A5B0, alpha), 0.36f);

    const char *mcName = StatColors::rgbToMcColor(r.color);
    float mcX = hexX + 75.0f;
    float mcY = rowY + (rowH - 9.0f) * 0.5f;
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawCircle(mcX + 4.0f, rowY + rowH * 0.5f, 3.5f, r.color, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(mcX + 13.0f, mcY, mcName, applyAlpha(0xFF888892, alpha), 0.35f);

    float delW = 28.0f;
    float delH = 26.0f;
    float delX = cx + rowW - delW - 8.0f;
    float delY = rowY + (rowH - delH) * 0.5f;
    bool hDel = isHovered(mx, my, delX, delY, delW, delH);

    glDisable(GL_TEXTURE_2D);
    DWORD delBg = hDel ? 0xDD8B1A1A : 0x28381216;
    DWORD delBorder = hDel ? 0xFFFF3838 : 0x55FF4444;
    RenderUtils::drawRoundedRect(delX, delY, delW, delH, 5.0f, delBg, alpha);
    RenderUtils::drawRoundedOutline(delX, delY, delW, delH, 5.0f, 1.0f, delBorder, alpha);
    glEnable(GL_TEXTURE_2D);
    float dw = g_guiFont.getStringWidth("X") * (0.36f / 0.5f);
    g_guiFont.drawString(delX + (delW - dw) * 0.5f, delY + (delH - 9.0f) * 0.5f, "X",
                         applyAlpha(hDel ? 0xFFFF2222 : 0xFFFF5555, alpha), 0.36f);

    if (clickEvent && hDel) {
      StatColors::removeRange((StatColors::StatType)s_colorSelectedStat, ri);
      Config::save();
      NotificationManager::getInstance()->add("Colors", "Range removed",
                                              NotificationType::Info);
      if (s_cpEditRangeIdx == ri) {
        s_cpEditRangeIdx = -1;
        s_colorPickerOpen = false;
        s_cpEditingField = 0;
      } else if (s_cpEditRangeIdx > ri) {
        s_cpEditRangeIdx--;
      }
      break;
    }

    float editW = 50.0f;
    float editH = 26.0f;
    float editX = delX - editW - 8.0f;
    float editY = rowY + (rowH - editH) * 0.5f;
    bool hEdit = isHovered(mx, my, editX, editY, editW, editH);

    glDisable(GL_TEXTURE_2D);
    drawThemeButton(editX, editY, editW, editH, hEdit, isEditingThis, alpha);
    glEnable(GL_TEXTURE_2D);
    float ew = g_guiFont.getStringWidth("Edit") * (0.36f / 0.5f);
    g_guiFont.drawString(editX + (editW - ew) * 0.5f, editY + (editH - 9.0f) * 0.5f, "Edit",
                         applyAlpha(isEditingThis ? 0xFFFFFFFF : (hEdit ? 0xFFFFFFFF : 0xFFCCCCCC), alpha),
                         0.36f);

    if (clickEvent && hEdit) {
      ClickGUIHelpers::cancelInlineEditors();
      s_cpEditRangeIdx = ri;
      s_colorPickerOpen = true;
      statPickerColor = r.color;
      s_cpEditingField = 0;

      if (isRatio)
        snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.2f", r.minVal);
      else
        snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.0f", r.minVal);
      s_cpMinLen = (int)strlen(s_cpMinBuf);

      if (r.maxVal >= 1e300) {
        s_cpMaxBuf[0] = 0;
        s_cpMaxLen = 0;
      } else {
        if (isRatio)
          snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.2f", r.maxVal);
        else
          snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.0f", r.maxVal);
        s_cpMaxLen = (int)strlen(s_cpMaxBuf);
      }
    }

    cy += rowH + 6.0f;
  }

  cy += 14.0f;

  const float actBtnH = 32.0f;
  const float addBtnW = 140.0f;
  const bool isAddingNew = (s_colorPickerOpen && s_cpEditRangeIdx < 0);
  bool hAdd = isHovered(mx, my, cx, cy, addBtnW, actBtnH);

  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, addBtnW, actBtnH, hAdd, isAddingNew, alpha);
  glEnable(GL_TEXTURE_2D);

  const char *addLabel = isAddingNew ? "- Close Picker" : "+ Add Range";
  float alw = g_guiFont.getStringWidth(addLabel) * (0.40f / 0.5f);
  g_guiFont.drawString(cx + (addBtnW - alw) * 0.5f, cy + (actBtnH - 9.0f) * 0.5f, addLabel,
                       applyAlpha(0xFFFFFFFF, alpha), 0.40f);

  if (clickEvent && hAdd) {
    ClickGUIHelpers::cancelInlineEditors();
    if (s_colorPickerOpen && s_cpEditRangeIdx < 0) {
      s_colorPickerOpen = false;
      s_cpEditingField = 0;
    } else {
      s_colorPickerOpen = true;
      s_cpEditRangeIdx = -1;
      s_cpEditingField = 0;
      statPickerColor = Config::getThemeColor();
      double nextMin = 0.0;
      if (!cfg.ranges.empty()) {
        for (const auto &rg : cfg.ranges) {
          if (rg.maxVal < 1e300 && rg.maxVal > nextMin)
            nextMin = rg.maxVal;
        }
      }
      if (isRatio) {
        std::snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.2f", nextMin);
        std::snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.2f", nextMin + 1.0);
      } else {
        std::snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.0f", nextMin);
        std::snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.0f", nextMin + (nextMin == 0.0 ? 1000.0 : nextMin));
      }
      s_cpMinLen = (int)strlen(s_cpMinBuf);
      s_cpMaxLen = (int)strlen(s_cpMaxBuf);
    }
  }

  const float rstBtnW = 140.0f;
  const float rstX = cx + addBtnW + 12.0f;
  bool hRst = isHovered(mx, my, rstX, cy, rstBtnW, actBtnH);

  glDisable(GL_TEXTURE_2D);
  DWORD rstCol = hRst ? 0xFF991111 : ClickGUITheme::cardBg();
  float rstAlpha = (((rstCol >> 24) & 0xFF) / 255.0f) * alpha;
  if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
    RenderUtils::drawRoundedRect(rstX, cy, rstBtnW, actBtnH, 5.0f, 0xFF0A0A12, 0.55f * alpha);
    Render::LiquidGlass::drawRect(rstX, cy, rstBtnW, actBtnH, 5.0f, alpha, rstCol);
  } else {
    RenderUtils::drawRoundedRect(rstX, cy, rstBtnW, actBtnH, 5.0f, rstCol, rstAlpha);
    RenderUtils::drawRoundedOutline(rstX, cy, rstBtnW, actBtnH, 5.0f, 1.0f,
                                    hRst ? 0xFFFF4444 : ClickGUITheme::hairline(), alpha);
  }
  glEnable(GL_TEXTURE_2D);

  float rw = g_guiFont.getStringWidth("Reset Defaults") * (0.40f / 0.5f);
  g_guiFont.drawString(rstX + (rstBtnW - rw) * 0.5f, cy + (actBtnH - 9.0f) * 0.5f, "Reset Defaults",
                       applyAlpha(hRst ? 0xFFFF8888 : 0xFFCCCCCC, alpha), 0.40f);

  if (clickEvent && hRst) {
    StatColors::resetToDefaults((StatColors::StatType)s_colorSelectedStat);
    Config::save();
    NotificationManager::getInstance()->add("Colors", "Reset to defaults",
                                            NotificationType::Success);
  }

  cy += actBtnH + 16.0f;

  if (s_colorPickerOpen) {
    const float popX = cx;
    const float popW = contentW;
    const float popY = cy;
    const float popH = 300.0f;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(popX, popY, popW, popH, false, alpha);
    RenderUtils::drawRoundedOutline(popX, popY, popW, popH, 8.0f, 1.0f,
                                    ClickGUITheme::hairlineStrong(), 0.6f * alpha);

    const char *cardTitle = (s_cpEditRangeIdx >= 0) ? "EDIT COLOR RANGE" : "ADD NEW COLOR RANGE";
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(popX + 16.0f, popY + 12.0f, cardTitle, applyAlpha(0xFFFFFFFF, alpha), 0.40f);



    float closeX = popX + popW - 32.0f;
    float closeY = popY + 9.0f;
    bool hClose = isHovered(mx, my, closeX, closeY, 22.0f, 22.0f);
    glDisable(GL_TEXTURE_2D);
    DWORD closeBg = hClose ? 0xDD8B1A1A : 0x28381216;
    DWORD closeBorder = hClose ? 0xFFFF3838 : 0x55FF4444;
    RenderUtils::drawRoundedRect(closeX, closeY, 22.0f, 22.0f, 4.0f, closeBg, alpha);
    RenderUtils::drawRoundedOutline(closeX, closeY, 22.0f, 22.0f, 4.0f, 1.0f, closeBorder, alpha);
    glEnable(GL_TEXTURE_2D);
    float cw = g_guiFont.getStringWidth("X") * (0.36f / 0.5f);
    g_guiFont.drawString(closeX + (22.0f - cw) * 0.5f, closeY + 3.0f, "X",
                         applyAlpha(hClose ? 0xFFFF2222 : 0xFFFF5555, alpha), 0.36f);
    if (clickEvent && hClose) {
      s_colorPickerOpen = false;
      s_cpEditRangeIdx = -1;
      s_cpEditingField = 0;
    }

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRect(popX + 12.0f, popY + 36.0f, popW - 24.0f, 1.0f,
                          ClickGUITheme::hairline(), 0.5f * alpha);
    glEnable(GL_TEXTURE_2D);

    float pickerX = popX + 16.0f;
    float pickerY = popY + 44.0f;
    float pickerW = popW - 32.0f;
    drawColorPicker(9100 + s_colorSelectedStat, pickerX, pickerY, pickerW,
                    statPickerColor, mx, my, lClick, clickEvent, alpha);

    float barY = popY + 258.0f;
    float curX = popX + 16.0f;

    g_guiFont.drawString(curX, barY + 7.0f, "Min:", applyAlpha(0xFFA0A0AA, alpha), 0.38f);
    curX += 30.0f;
    float boxW = 60.0f;
    float boxH = 26.0f;
    bool hMin = isHovered(mx, my, curX, barY + 2.0f, boxW, boxH);
    glDisable(GL_TEXTURE_2D);
    drawTextInput(curX, barY + 2.0f, boxW, boxH, s_cpEditingField == 1, hMin, alpha);
    glEnable(GL_TEXTURE_2D);

    const char *minDisp = (s_cpMinLen > 0) ? s_cpMinBuf : "0";
    DWORD minTextColor = (s_cpMinLen > 0) ? 0xFFFFFFFF : 0xFF707078;
    g_guiFont.drawString(curX + 8.0f, barY + 6.0f, minDisp, applyAlpha(minTextColor, alpha), 0.38f);

    bool showCursor = (GetTickCount64() / 500) % 2 == 0;
    if (s_cpEditingField == 1 && showCursor) {
      float mtw = g_guiFont.getStringWidth(s_cpMinBuf) * (0.38f / 0.5f);
      glDisable(GL_TEXTURE_2D);
      glColor4f(1, 1, 1, alpha);
      glBegin(GL_LINES);
      glVertex2f(curX + 8.0f + mtw, barY + 5.0f);
      glVertex2f(curX + 8.0f + mtw, barY + 21.0f);
      glEnd();
      glEnable(GL_TEXTURE_2D);
    }
    if (clickEvent && hMin) s_cpEditingField = 1;
    curX += boxW + 12.0f;

    g_guiFont.drawString(curX, barY + 7.0f, "->", applyAlpha(0xFF707078, alpha), 0.40f);
    curX += 20.0f;

    g_guiFont.drawString(curX, barY + 7.0f, "Max:", applyAlpha(0xFFA0A0AA, alpha), 0.38f);
    curX += 34.0f;
    bool hMax = isHovered(mx, my, curX, barY + 2.0f, boxW, boxH);
    glDisable(GL_TEXTURE_2D);
    drawTextInput(curX, barY + 2.0f, boxW, boxH, s_cpEditingField == 2, hMax, alpha);
    glEnable(GL_TEXTURE_2D);

    const char *maxDisp = (s_cpMaxLen > 0) ? s_cpMaxBuf : "INF";
    DWORD maxTextColor = (s_cpMaxLen > 0) ? 0xFFFFFFFF : 0xFF707078;
    g_guiFont.drawString(curX + 8.0f, barY + 6.0f, maxDisp, applyAlpha(maxTextColor, alpha), 0.38f);

    if (s_cpEditingField == 2 && showCursor) {
      float xtw = g_guiFont.getStringWidth(s_cpMaxBuf) * (0.38f / 0.5f);
      glDisable(GL_TEXTURE_2D);
      glColor4f(1, 1, 1, alpha);
      glBegin(GL_LINES);
      glVertex2f(curX + 8.0f + xtw, barY + 5.0f);
      glVertex2f(curX + 8.0f + xtw, barY + 21.0f);
      glEnd();
      glEnable(GL_TEXTURE_2D);
    }
    if (clickEvent && hMax) s_cpEditingField = 2;
    curX += boxW + 20.0f;

    if (clickEvent && !hMin && !hMax) {
      s_cpEditingField = 0;
    }

    g_guiFont.drawString(curX, barY + 7.0f, "Preview:", applyAlpha(0xFFA0A0AA, alpha), 0.36f);
    curX += 54.0f;
    float prevW = 32.0f;
    float prevH = 24.0f;
    float prevY = barY + 3.0f;
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawGlow(curX, prevY, prevW, prevH, 4.0f, statPickerColor, 0.25f * alpha);
    RenderUtils::drawRoundedRect(curX, prevY, prevW, prevH, 4.0f, statPickerColor, alpha);
    RenderUtils::drawRoundedOutline(curX, prevY, prevW, prevH, 4.0f, 1.0f, 0x40FFFFFF, 0.6f * alpha);
    glEnable(GL_TEXTURE_2D);

    float saveBtnW = (s_cpEditRangeIdx >= 0) ? 100.0f : 90.0f;
    float saveBtnH = 26.0f;
    float saveBtnX = popX + popW - saveBtnW - 16.0f;
    float saveBtnY = barY + 2.0f;

    float cancelBtnW = 60.0f;
    float cancelBtnH = 26.0f;
    float cancelBtnX = saveBtnX - cancelBtnW - 8.0f;
    float cancelBtnY = barY + 2.0f;

    bool hCancel = isHovered(mx, my, cancelBtnX, cancelBtnY, cancelBtnW, cancelBtnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(cancelBtnX, cancelBtnY, cancelBtnW, cancelBtnH, hCancel, false, alpha);
    glEnable(GL_TEXTURE_2D);
    float cwCancel = g_guiFont.getStringWidth("Cancel") * (0.36f / 0.5f);
    g_guiFont.drawString(cancelBtnX + (cancelBtnW - cwCancel) * 0.5f, cancelBtnY + 5.0f, "Cancel",
                         applyAlpha(hCancel ? 0xFFFFFFFF : 0xFFCCCCCC, alpha), 0.36f);
    if (clickEvent && hCancel) {
      s_colorPickerOpen = false;
      s_cpEditRangeIdx = -1;
      s_cpEditingField = 0;
    }

    const char *saveLabel = (s_cpEditRangeIdx >= 0) ? "Save Range" : "Add Range";
    bool hSave = isHovered(mx, my, saveBtnX, saveBtnY, saveBtnW, saveBtnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(saveBtnX, saveBtnY, saveBtnW, saveBtnH, hSave, true, alpha);
    glEnable(GL_TEXTURE_2D);
    float swSave = g_guiFont.getStringWidth(saveLabel) * (0.38f / 0.5f);
    g_guiFont.drawString(saveBtnX + (saveBtnW - swSave) * 0.5f, saveBtnY + 5.0f, saveLabel,
                         applyAlpha(0xFFFFFFFF, alpha), 0.38f);

    if (clickEvent && hSave) {
      double minV = atof(s_cpMinBuf);
      double maxV = atof(s_cpMaxBuf);
      if (maxV <= 0 || strlen(s_cpMaxBuf) == 0)
        maxV = 1e308;

      bool success = false;
      if (s_cpEditRangeIdx >= 0) {
        success = StatColors::updateRange(
            (StatColors::StatType)s_colorSelectedStat, s_cpEditRangeIdx, minV,
            maxV, statPickerColor);
      } else {
        success =
            StatColors::addRange((StatColors::StatType)s_colorSelectedStat,
                                 minV, maxV, statPickerColor);
      }

      if (success) {
        const bool wasEditing = s_cpEditRangeIdx >= 0;
        Config::save();
        NotificationManager::getInstance()->add(
            "Colors",
            wasEditing ? "Range updated!" : "Range added!",
            NotificationType::Success);
        s_cpEditRangeIdx = -1;
        s_colorPickerOpen = false;
        s_cpEditingField = 0;
      } else {
        NotificationManager::getInstance()->add(
            "Colors", "Overlap! Check existing ranges.",
            NotificationType::Error);
      }
    }

    cy += popH + 16.0f;
  }

  cy += 30.0f;
}

} // namespace Tabs
} // namespace Render
