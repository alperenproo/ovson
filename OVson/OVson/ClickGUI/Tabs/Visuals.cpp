#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/StatsOverlay.h"
#include "../../Render/NotificationManager.h"
#include "../../Render/BetterTab.h"
#include "../../Config/Config.h"
#include "../ClickGUI.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <gl/GL.h>
#include <map>
#include <string>
#include <vector>

namespace Render {
namespace Tabs {

void renderVisuals(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  lClick = ctx.lClick;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  drawSectionLabel(cx, cy, "Overlays", alpha);
  cy += 40;
  auto drawSettingsCard = [&](const char *title, const char *desc, bool &val,
                              int id, float &cy_ref) {
    bool hover = isHovered(mx, my, mainX + 190, cy_ref - 10, g_w - 210, 62);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy_ref - 10, g_w - 210, 62, hover, alpha, val);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy_ref, title, applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy_ref + 18, desc, applyAlpha(0xFFA0A0A5, alpha),
                         0.45f);

    float swX = mainX + g_w - 65;
    drawSwitch(id, swX, cy_ref + 5, val, hover, alpha);

    if (clickEvent && hover) {
      val = !val;
    }
    cy_ref += 72;
  };

  bool ovEnabled = StatsOverlay::isEnabled();
  bool oldOv = ovEnabled;
  drawSettingsCard("Stats Overlay",
                   "Display player skill metrics in a clean table", ovEnabled,
                   1, cy);
  if (ovEnabled != oldOv)
    StatsOverlay::setEnabled(ovEnabled);

  bool notifEnabled = Config::isNotificationsEnabled();
  bool oldNotif = notifEnabled;
  drawSettingsCard("Refined Notifications",
                   "Enable silky smooth toast alerts", notifEnabled, 2, cy);
  if (notifEnabled != oldNotif) {
    Config::setNotificationsEnabled(notifEnabled);
  }

  bool techEnabled = Config::isTechEnabled();
  bool oldTech = techEnabled;
  drawSettingsCard("Tech Overlay", "Show technical JNI and system metrics",
                   techEnabled, 9, cy);
  if (techEnabled != oldTech) {
    Config::setTechEnabled(techEnabled);
  }

  bool blurEnabled = Config::isMotionBlurEnabled();
  {
    const float panelX = mainX + 190;
    const float panelW = g_w - 210;
    const float cardY = cy - 10;
    const float topH = 62.0f;
    const float cardH = blurEnabled ? 104.0f : topH;
    bool cardHover = isHovered(mx, my, panelX, cardY, panelW, cardH);
    bool toggleHover = isHovered(mx, my, panelX, cardY, panelW, topH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(panelX, cardY, panelW, cardH, cardHover, alpha, blurEnabled);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Motion Blur", applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18,
                         "Adds a cinematic trail to camera movement",
                         applyAlpha(0xFFA0A0A5, alpha), 0.45f);

    float swX = mainX + g_w - 65;
    glDisable(GL_TEXTURE_2D);
    drawSwitch(4, swX, cy + 5, blurEnabled, toggleHover, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && toggleHover) {
      blurEnabled = !blurEnabled;
      Config::setMotionBlurEnabled(blurEnabled);
    }

    if (blurEnabled) {
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRect(panelX + 16, cardY + topH, panelW - 32, 1.0f,
                            0xFFFFFFFF, 0.06f * alpha);
      glEnable(GL_TEXTURE_2D);
      float rowY = cardY + topH + 12.0f;
      g_guiFont.drawString(cx, rowY, "Blur Intensity",
                           applyAlpha(0xFFA0A0A5, alpha), 0.42f);
      float val = Config::getMotionBlurAmount();
      glDisable(GL_TEXTURE_2D);
      bool ch = drawSlider(80, cx + 110, rowY + 6, panelW - 240, 6.0f, val, 0.0f,
                           1.0f, mx, my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      if (ch) Config::setMotionBlurAmount(val);
      char vb[16];
      snprintf(vb, sizeof(vb), "%d%%", (int)(val * 100.0f + 0.5f));
      g_guiFont.drawString(panelX + panelW - 44, rowY, vb,
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    }
    cy += cardH + 10.0f;
  }

  bool nameTagsEnabled = Config::isNameTagsEnabled();
  {
    const float panelX = mainX + 190;
    const float panelW = g_w - 210;
    const float cardY = cy - 10;
    const float topH = 62.0f;
    const float lx = cx;                        // content left
    const float rx = panelX + panelW - 16.0f;   // content right

    auto slots = Config::getNameTagStats();
    auto labelFor = [](const std::string &k) -> const char * {
      if (k == "star") return "Star";
      if (k == "fkdr") return "FKDR";
      if (k == "fk")   return "Final Kills";
      if (k == "wins") return "Wins";
      if (k == "wlr")  return "WLR";
      if (k == "ws")   return "Winstreak";
      return "?";
    };

    const float heightRowY  = cardY + topH + 9.0f;
    const float heightRowH  = 30.0f;
    const float statsLabelY = heightRowY + heightRowH + 6.0f;
    const float chipsTop    = statsLabelY + 22.0f;
    const float chipH = 30.0f, gap = 9.0f, padX = 13.0f, numW = 14.0f;
    const float tscale = 0.45f;

    struct Rc { float x, y, w; };
    std::vector<Rc> rc(slots.size());
    {
      float curX = 0, curY = 0;
      float maxRowW = rx - lx;
      for (size_t r = 0; r < slots.size(); ++r) {
        bool en = slots[r].second;
        float lw = g_guiFont.getStringWidth(labelFor(slots[r].first)) *
                   (tscale / 0.5f);
        float w = padX * 2.0f + (en ? numW : 0.0f) + lw;
        if (curX + w > maxRowW && curX > 0) { curX = 0; curY += chipH + gap; }
        rc[r] = {curX, curY, w};
        curX += w + gap;
      }
    }
    float chipsBottom = rc.empty() ? chipsTop + chipH : chipsTop + rc.back().y + chipH;
    float cardH = nameTagsEnabled ? (chipsBottom + 12.0f - cardY) : topH;

    bool cardHover   = isHovered(mx, my, panelX, cardY, panelW, cardH);
    bool toggleHover = isHovered(mx, my, panelX, cardY, panelW, topH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(panelX, cardY, panelW, cardH, cardHover, alpha,
                  nameTagsEnabled);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cardY + 10, "NameTags",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cardY + 28,
                         "Renders the Bedwars stats above every nearby player",
                         applyAlpha(0xFFA0A0A5, alpha), 0.45f);
    float swX = mainX + g_w - 65;
    glDisable(GL_TEXTURE_2D);
    drawSwitch(7, swX, cardY + 15, nameTagsEnabled, toggleHover, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && toggleHover) {
      nameTagsEnabled = !nameTagsEnabled;
      Config::setNameTagsEnabled(nameTagsEnabled);
    }

    if (nameTagsEnabled) {
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRect(panelX + 16, cardY + topH, panelW - 32, 1.0f,
                            0xFFFFFFFF, 0.06f * alpha);
      glEnable(GL_TEXTURE_2D);

      float h = Config::getNameTagHeight();
      g_guiFont.drawString(lx, heightRowY + heightRowH * 0.5f - 6.0f,
                           "Label height", applyAlpha(0xFFFFFFFF, alpha), 0.45f);
      char hbuf[24];
      snprintf(hbuf, sizeof(hbuf), "%.1f m", h);
      float hw = g_guiFont.getStringWidth(hbuf) * (0.42f / 0.5f);
      g_guiFont.drawString(rx - hw, heightRowY + heightRowH * 0.5f - 6.0f, hbuf,
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);
      float hv = h;
      glDisable(GL_TEXTURE_2D);
      bool ch = drawSlider(2500, lx + 92.0f, heightRowY + heightRowH * 0.5f,
                           (rx - 52.0f) - (lx + 92.0f), 6.0f, hv, 0.5f, 4.0f, mx,
                           my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      if (ch) Config::setNameTagHeight(hv);

      drawSectionLabel(lx, statsLabelY, "Stats", alpha);
      g_guiFont.drawString(lx + 58, statsLabelY + 2,
                           "click to toggle  -  drag to reorder",
                           applyAlpha(0xFF6E6E78, alpha), 0.38f);

      static int  s_chipDrag  = -1;
      static int  s_chipPress = -1;
      static bool s_chipMoved = false;
      auto chipAt = [&](float px, float py) -> int {
        for (size_t r = 0; r < rc.size(); ++r) {
          float absX = lx + rc[r].x;
          float absY = chipsTop + rc[r].y;
          if (px >= absX && px <= absX + rc[r].w && py >= absY && py <= absY + chipH)
            return (int)r;
        }
        return -1;
      };

      static std::map<std::string, float> s_ax, s_ay;
      int enRank = 0;
      for (size_t r = 0; r < slots.size(); ++r) {
        const std::string &k = slots[r].first;
        float txr = rc[r].x, tyr = rc[r].y;
        if (!s_ax.count(k)) { s_ax[k] = txr; s_ay[k] = tyr; }
        s_ax[k] += (txr - s_ax[k]) * 0.30f;
        s_ay[k] += (tyr - s_ay[k]) * 0.30f;
        float x = lx + s_ax[k], y = chipsTop + s_ay[k], w = rc[r].w;

        bool en = slots[r].second;
        bool dragging = ((int)r == s_chipDrag);
        float absRCX = lx + rc[r].x;
        float absRCY = chipsTop + rc[r].y;
        bool hov = isHovered(mx, my, absRCX, absRCY, w, chipH);

        glDisable(GL_TEXTURE_2D);
        if (en) {
          RenderUtils::drawGlow(x, y, w, chipH, 9.0f, ClickGUITheme::accent(),
                                (dragging ? 0.5f : 0.30f) * alpha);
          RenderUtils::drawRoundedRect(x, y, w, chipH, 9.0f,
                                       ClickGUITheme::accent(), 0.92f * alpha);
          if (dragging || hov)
            RenderUtils::drawRoundedOutline(x, y, w, chipH, 9.0f, 1.5f,
                                            0xFFFFFFFF,
                                            (dragging ? 0.85f : 0.35f) * alpha);
        } else {
          DWORD cb = hov ? 0x16FFFFFF : 0x0CFFFFFF;
          RenderUtils::drawRoundedRect(x, y, w, chipH, 9.0f, cb,
                                       (((cb >> 24) & 0xFF) / 255.0f) * alpha);
          RenderUtils::drawRoundedOutline(x, y, w, chipH, 9.0f, 1.0f, 0x22FFFFFF,
                                          0.6f * alpha);
        }
        glEnable(GL_TEXTURE_2D);

        float tx = x + padX;
        if (en) {
          char nb[4];
          snprintf(nb, sizeof(nb), "%d", ++enRank);
          g_guiFont.drawString(tx, y + chipH * 0.5f - 6.0f, nb,
                               applyAlpha(0xB0FFFFFF, alpha), tscale);
          tx += numW;
        }
        g_guiFont.drawString(tx, y + chipH * 0.5f - 6.0f, labelFor(k),
                             applyAlpha(en ? 0xFFFFFFFF : 0xFFA0A0A8, alpha),
                             tscale);
      }

      if (clickEvent) {
        int hit = chipAt(mx, my);
        if (hit >= 0) { s_chipDrag = hit; s_chipPress = hit; s_chipMoved = false; }
      }
      if (s_chipDrag >= 0 && lClick) {
        int tgt = chipAt(mx, my);
        if (tgt >= 0 && tgt != s_chipDrag) {
          auto mv = slots[s_chipDrag];
          slots.erase(slots.begin() + s_chipDrag);
          slots.insert(slots.begin() + tgt, mv);
          Config::setNameTagStats(slots);
          s_chipDrag = tgt;
          s_chipMoved = true;
        }
      }
      if (!lClick) {
        if (s_chipDrag >= 0 && !s_chipMoved && s_chipPress >= 0 &&
            s_chipPress < (int)slots.size()) {
          slots[s_chipPress].second = !slots[s_chipPress].second;
          Config::setNameTagStats(slots);
        }
        s_chipDrag = -1;
        s_chipPress = -1;
        s_chipMoved = false;
      }
    }

    cy += cardH + 10.0f;
  }

  cy += 20;
  drawSectionLabel(cx, cy, "Table Customization", alpha);
  cy += 30;

  static float s_dropdownPaneY = 0.0f;
  s_dropdownPaneY = cy;

  {
    float totalW = g_w - 240.0f;
    float leftW  = 195.0f;
    float pGap   = 16.0f;
    float rightW = totalW - leftW - pGap;
    float leftX  = cx;
    float rightX = cx + leftW + pGap;
    float paneY  = cy;

    float cPad   = 12.0f;
    float idH    = 28.0f;   // inner dropdown height
    float cardH  = 62.0f;
    float cGap   = 10.0f;
    float leftH  = 3 * cardH + 2 * cGap;

    struct ColToggle {
      std::string name;
      bool enabled;
      std::function<void(bool)> setter;
    };
    std::vector<ColToggle> toggles;
    if (s_columnTargetMode == 0) {
      toggles = {
          {"Star", Config::isOvShowStar(),
           [](bool b) { Config::setOvShowStar(b); }},
          {"FK", Config::isOvShowFk(), [](bool b) { Config::setOvShowFk(b); }},
          {"FKDR", Config::isOvShowFkdr(),
           [](bool b) { Config::setOvShowFkdr(b); }},
          {"Wins", Config::isOvShowWins(),
           [](bool b) { Config::setOvShowWins(b); }},
          {"WLR", Config::isOvShowWlr(),
           [](bool b) { Config::setOvShowWlr(b); }},
          {"WS", Config::isOvShowWs(), [](bool b) { Config::setOvShowWs(b); }},
          {"Kills", Config::isOvShowKills(),
           [](bool b) { Config::setOvShowKills(b); }},
          {"KDR", Config::isOvShowKdr(),
           [](bool b) { Config::setOvShowKdr(b); }},
          {"Beds", Config::isOvShowBeds(),
           [](bool b) { Config::setOvShowBeds(b); }},
          {"BLR", Config::isOvShowBlr(),
           [](bool b) { Config::setOvShowBlr(b); }},
          {"Ping", Config::isOvShowPing(),
           [](bool b) { Config::setOvShowPing(b); }},
          {"Tags", Config::isOvShowTags(),
           [](bool b) { Config::setOvShowTags(b); }}};
    } else {
      toggles = {{"Star", Config::isProShowStar(),
                  [](bool b) { Config::setProShowStar(b); }},
                 {"FK", Config::isProShowFk(),
                  [](bool b) { Config::setProShowFk(b); }},
                 {"FKDR", Config::isProShowFkdr(),
                  [](bool b) { Config::setProShowFkdr(b); }},
                 {"Wins", Config::isProShowWins(),
                  [](bool b) { Config::setProShowWins(b); }},
                 {"WLR", Config::isProShowWlr(),
                  [](bool b) { Config::setProShowWlr(b); }},
                 {"WS", Config::isProShowWs(),
                  [](bool b) { Config::setProShowWs(b); }},
                 {"Kills", Config::isProShowKills(),
                  [](bool b) { Config::setProShowKills(b); }},
                 {"KDR", Config::isProShowKdr(),
                  [](bool b) { Config::setProShowKdr(b); }},
                 {"Beds", Config::isProShowBeds(),
                  [](bool b) { Config::setProShowBeds(b); }},
                 {"BLR", Config::isProShowBlr(),
                  [](bool b) { Config::setProShowBlr(b); }},
                 {"Ping", Config::isProShowPing(),
                  [](bool b) { Config::setProShowPing(b); }},
                 {"Tags", Config::isProShowTags(),
                  [](bool b) { Config::setProShowTags(b); }},
                 {"HP", Config::isProShowHp(),
                  [](bool b) { Config::setProShowHp(b); }}};
    }

    float pPadX  = 10.0f;
    float pGapX  = 6.0f;
    float pGapY  = 6.0f;
    float pillW  = (rightW - 2 * pPadX - pGapX) / 2;
    float pillH  = 28.0f;
    int   pRows  = ((int)toggles.size() + 1) / 2;
    float pillY0 = paneY + 40.0f;
    float rightH = 40.0f + pRows * (pillH + pGapY) + 8.0f;
    float maxH   = (leftH > rightH) ? leftH : rightH;

    float totalLeftH = 3 * cardH + 2 * cGap;
    drawThemeCard(leftX, paneY, leftW, totalLeftH, false, alpha);

    {
      float cY = paneY;
      g_guiFont.drawString(leftX + cPad, cY + 8, "SORT COLUMN",
                           applyAlpha(0xFF9AA0B0, alpha), 0.35f);
      float dX = leftX + cPad, dY = cY + 28, dW = leftW - 2 * cPad;
      bool hD = isHovered(mx, my, dX, dY, dW, idH);
      drawThemeCard(dX, dY, dW, idH, hD, alpha);
      std::string curSort = Config::getSortMode();
      g_guiFont.drawString(dX + 10, dY + 7, curSort.c_str(),
                           applyAlpha(0xFFFFFFFF, alpha), 0.4f);
      drawChevron(dX + dW - 12, dY + idH * 0.5f, 3.5f,
                  s_isSortColumnDropdownOpen, 0xFFA0A0A5, alpha);
      if (clickEvent && hD) {
        s_isSortColumnDropdownOpen = !s_isSortColumnDropdownOpen;
        if (s_isSortColumnDropdownOpen) {
          s_isSortOrderDropdownOpen = false;
          s_isTabDisplayDropdownOpen = false;
        }
      }
    }

    {
      float cY = paneY + cardH + cGap;
      g_guiFont.drawString(leftX + cPad, cY + 8, "SORT ORDER",
                           applyAlpha(0xFF9AA0B0, alpha), 0.35f);
      float dX = leftX + cPad, dY = cY + 28, dW = leftW - 2 * cPad;
      bool hD = isHovered(mx, my, dX, dY, dW, idH);
      drawThemeCard(dX, dY, dW, idH, hD, alpha);
      bool isDesc = Config::isTabSortDescending();
      const char *curOrd = isDesc ? "Descending" : "Ascending";
      g_guiFont.drawString(dX + 10, dY + 7, curOrd,
                           applyAlpha(0xFFFFFFFF, alpha), 0.4f);
      drawChevron(dX + dW - 12, dY + idH * 0.5f, 3.5f,
                  s_isSortOrderDropdownOpen, 0xFFA0A0A5, alpha);
      if (clickEvent && hD) {
        s_isSortOrderDropdownOpen = !s_isSortOrderDropdownOpen;
        if (s_isSortOrderDropdownOpen) {
          s_isSortColumnDropdownOpen = false;
          s_isTabDisplayDropdownOpen = false;
        }
      }
    }

    {
      float cY = paneY + 2 * (cardH + cGap);
      g_guiFont.drawString(leftX + cPad, cY + 8, "TAB DISPLAY",
                           applyAlpha(0xFF9AA0B0, alpha), 0.35f);
      float dX = leftX + cPad, dY = cY + 28, dW = leftW - 2 * cPad;
      bool hD = isHovered(mx, my, dX, dY, dW, idH);
      drawThemeCard(dX, dY, dW, idH, hD, alpha);
      std::string curDisp = Config::getTabDisplayMode();
      g_guiFont.drawString(dX + 10, dY + 7, curDisp.c_str(),
                           applyAlpha(0xFFFFFFFF, alpha), 0.4f);
      drawChevron(dX + dW - 12, dY + idH * 0.5f, 3.5f,
                  s_isTabDisplayDropdownOpen, 0xFFA0A0A5, alpha);
      if (clickEvent && hD) {
        s_isTabDisplayDropdownOpen = !s_isTabDisplayDropdownOpen;
        if (s_isTabDisplayDropdownOpen) {
          s_isSortColumnDropdownOpen = false;
          s_isSortOrderDropdownOpen = false;
        }
      }
    }

    {
      drawThemeCard(rightX, paneY, rightW, maxH, false, alpha);

      g_guiFont.drawString(rightX + 14, paneY + 12, "Visible Columns",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);

      float segW  = 128.0f;
      float segH  = 22.0f;
      float segX  = rightX + rightW - segW - 14;
      float segY  = paneY + 10;
      float segBW = segW / 2.0f;


      bool hovOvr = isHovered(mx, my, segX, segY, segBW, segH);
      bool hovTab = isHovered(mx, my, segX + segBW, segY, segBW, segH);

      drawThemeButton(segX, segY, segBW, segH, hovOvr, s_columnTargetMode == 0, alpha);
      drawThemeButton(segX + segBW, segY, segBW, segH, hovTab, s_columnTargetMode == 1, alpha);

      g_guiFont.drawString(
          segX + 10, segY + 5, "Overlay",
          applyAlpha(s_columnTargetMode == 0 ? 0xFFFFFFFF : 0xFF9AA0B0, alpha),
          0.35f);
      g_guiFont.drawString(
          segX + segBW + 6, segY + 5, "Tab List",
          applyAlpha(s_columnTargetMode == 1 ? 0xFFFFFFFF : 0xFF9AA0B0, alpha),
          0.35f);

      if (clickEvent && hovOvr && s_columnTargetMode != 0)
        s_columnTargetMode = 0;
      if (clickEvent && hovTab && s_columnTargetMode != 1)
        s_columnTargetMode = 1;

      for (size_t i = 0; i < toggles.size(); ++i) {
        int col = (int)(i % 2);
        int row = (int)(i / 2);
        float px = rightX + pPadX + col * (pillW + pGapX);
        float py = pillY0 + row * (pillH + pGapY);
        bool hov = isHovered(mx, my, px, py, pillW, pillH);
        bool on  = toggles[i].enabled;

        drawThemeButton(px, py, pillW, pillH, hov, on, alpha);

        float dcx = px + pillW - 14;
        float dcy = py + pillH * 0.5f;
        glDisable(GL_TEXTURE_2D);
        if (on) {
          RenderUtils::drawCircle(dcx, dcy, 4.5f, ClickGUITheme::accent(),
                                  0.25f * alpha);
          RenderUtils::drawCircle(dcx, dcy, 3.0f, ClickGUITheme::accent(), alpha);
        } else {
          RenderUtils::drawCircle(dcx, dcy, 3.0f, 0xFF646A78,
                                  0.6f * alpha);
        }
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(px + 10, py + pillH * 0.5f - 5,
                             toggles[i].name.c_str(),
                             applyAlpha(on ? 0xFFFFFFFF : 0xFF9AA0B0,
                                        alpha),
                             0.4f);
        if (clickEvent && hov)
          toggles[i].setter(!toggles[i].enabled);
      }
    }

    cy = paneY + maxH + 20;

    cy = paneY + maxH + 20;
  }

  g_guiFont.drawString(cx, cy, "Tab List & Chat Features",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 35;

  {
    bool hTab = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 85);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 85, hTab, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Tab List Overlay",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18,
                         "Show stats next to names in player TAB list",
                         applyAlpha(0xFFA0A0A5, alpha), 0.45f);

    bool tabEnabled = Config::isTabEnabled();
    glDisable(GL_TEXTURE_2D);
    float tabSwX = mainX + g_w - 65;
    drawSwitch(30, tabSwX, cy + 5, tabEnabled, hTab && (my < cy + 30), alpha);
    glEnable(GL_TEXTURE_2D);

    bool hPro = hTab && (my >= cy + 30);
    bool proEnabled = Config::isBetterTabModeEnabled();
    float proAlpha = alpha * (tabEnabled ? 1.0f : 0.4f);

    g_guiFont.drawString(cx + 10, cy + 45, "BetterTab",
                         applyAlpha(0xFFFFFFFF, proAlpha), 0.42f);
    glDisable(GL_TEXTURE_2D);
    drawSwitch(33, tabSwX, cy + 42, proEnabled, hPro && tabEnabled, proAlpha);
    glEnable(GL_TEXTURE_2D);

    bool resizeClicked = false;
    if (tabEnabled && proEnabled) {
        float btnW = 70.0f;
        float btnH = 22.0f;
        float btnX = tabSwX - btnW - 8.0f;
        float btnY = cy + 38.0f;
        
        bool hResizeBtn = isHovered(mx, my, btnX, btnY, btnW, btnH);
        glDisable(GL_TEXTURE_2D);
        uint32_t btnBg = hResizeBtn ? ClickGUITheme::accent() : 0xFF2A2A35;
        float btnAlpha = alpha * (hResizeBtn ? 0.9f : 0.7f);
        RenderUtils::drawRoundedRect(btnX, btnY, btnW, btnH, 6.0f, btnBg, btnAlpha);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(btnX + 12, btnY + 4, "\xE2\x87\xB1 Resize",
                             applyAlpha(0xFFFFFFFF, alpha), 0.40f);
        
        if (clickEvent && hResizeBtn) {
            BetterTab::setResizeMode(true);
            ClickGUI::setOpen(false);
            resizeClicked = true;
        }
    }

    if (clickEvent && hTab && !resizeClicked) {
      if (my < cy + 30) {
        Config::setTabEnabled(!tabEnabled);
        NotificationManager::getInstance()->add(
            "Tab", !tabEnabled ? "Tab List Enabled" : "Tab List Disabled",
            !tabEnabled ? NotificationType::Success
                        : NotificationType::Warning);
      } else if (tabEnabled) {
        Config::setBetterTabModeEnabled(!proEnabled);
        NotificationManager::getInstance()->add(
            "Tab",
            !proEnabled ? "Better Mode Enabled" : "Better Mode Disabled",
            !proEnabled ? NotificationType::Success
                        : NotificationType::Warning);
      }
    }
    
    cy += 95;

    bool hChatStats = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 62);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 62, hChatStats, alpha);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "Pre-Game Chat Stats",
                         applyAlpha(0xFFFFFFFF, alpha * s_contentAlpha));
    g_guiFont.drawString(
        cx, cy + 18, "Auto-fetch stats when players speak in pre-game lobby",
        applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

    bool chatStatsEnabled = Config::isPreGameChatStatsEnabled();
    glDisable(GL_TEXTURE_2D);
    float csSwX = mainX + g_w - 65;
    drawSwitch(31, csSwX, cy + 5, chatStatsEnabled, hChatStats, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hChatStats) {
      Config::setPreGameChatStatsEnabled(!chatStatsEnabled);
      NotificationManager::getInstance()->add(
          "Chat",
          !chatStatsEnabled ? "Pre-Game Stats Enabled"
                            : "Pre-Game Stats Disabled",
          !chatStatsEnabled ? NotificationType::Success
                            : NotificationType::Warning);
    }
    cy += 72;
  }

  {
    bool hReport = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 82);

    bool hovAnyChannel = false;
    float tempChX = cx + 80;
    for (int i = 0; i < 3; ++i) {
      if (isHovered(mx, my, tempChX, cy + 37, 55, 25))
        hovAnyChannel = true;
      tempChX += 62;
    }

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 82, hReport, alpha);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "Team Stats Report",
                         applyAlpha(0xFFFFFFFF, alpha * s_contentAlpha));
    g_guiFont.drawString(
        cx, cy + 18, "Auto-report team averages to chat (.teamreport)",
        applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

    bool reportEnabled = Config::isTeamReportEnabled();
    glDisable(GL_TEXTURE_2D);
    float repSwX = mainX + g_w - 65;
    drawSwitch(32, repSwX, cy + 5, reportEnabled, hReport && !hovAnyChannel,
               alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hReport && !hovAnyChannel) {
      Config::setTeamReportEnabled(!reportEnabled);
      NotificationManager::getInstance()->add(
          "Team Report",
          !reportEnabled ? "Team Report Enabled" : "Team Report Disabled",
          !reportEnabled ? NotificationType::Success
                         : NotificationType::Warning);
    }

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(
        cx, cy + 42,
        "Channel:", applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

    const char *channels[] = {"/pc", "/ac", "/shout"};
    std::string curChannel = Config::getTeamReportChannel();
    float chX = cx + 80;
    for (int i = 0; i < 3; ++i) {
      bool hov = isHovered(mx, my, chX, cy + 37, 55, 25);
      bool sel = (curChannel == channels[i]);
      glDisable(GL_TEXTURE_2D);
      drawThemeButton(chX, cy + 37, 55, 25, hov, sel, alpha * s_contentAlpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(
          chX + 8, cy + 44, channels[i],
          applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha * s_contentAlpha),
          0.4f);
      if (clickEvent && hov) {
        Config::setTeamReportChannel(channels[i]);
        NotificationManager::getInstance()->add(
            "Team Report", std::string("Channel set to ") + channels[i],
            NotificationType::Info);
      }
      chX += 62;
    }
    cy += 92;
  }
  cy += 20;

  {
    float leftX = cx;
    float leftW = 195.0f;
    float cPad = 12.0f;
    float idH = 28.0f;
    float cardH = 62.0f;
    float cGap = 10.0f;
    float paneY = s_dropdownPaneY;

    s_sortColumnDropdownAnim +=
        (s_isSortColumnDropdownOpen ? 1.0f - s_sortColumnDropdownAnim
                                    : 0.0f - s_sortColumnDropdownAnim) *
        0.15f;
    if (s_sortColumnDropdownAnim > 0.01f) {
      const char *sM[] = {"Team", "Star", "FK", "FKDR",
                          "Wins", "WLR",  "WS"};
      std::string cs = Config::getSortMode();
      float mX = leftX + cPad, mW = leftW - 2 * cPad;
      float mY = paneY + 28 + idH + 2;
      float mA = alpha * s_sortColumnDropdownAnim;
      for (int i = 0; i < 7; ++i) {
        float iY = mY + i * idH;
        bool hI  = isHovered(mx, my, mX, iY, mW, idH);
        bool sel = (cs == sM[i]);
        if (ClickGUITheme::style() != ClickGUITheme::Style::LiquidGlass) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, 0xFF0D0F13, mA);
          glEnable(GL_TEXTURE_2D);
        }
        drawThemeCard(mX, iY, mW, idH, hI, mA);
        if (sel) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, ClickGUITheme::accent(),
                                       0.08f * mA);
          glEnable(GL_TEXTURE_2D);
        }
        g_guiFont.drawString(
            mX + 10, iY + 7, sM[i],
            applyAlpha(
                sel ? ClickGUITheme::accent() : (hI ? 0xFFFFFFFF : 0xFF9AA0B0), mA),
            0.4f);
        if (clickEvent && hI && s_sortColumnDropdownAnim > 0.8f) {
          Config::setSortMode(sM[i]);
          s_isSortColumnDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Sort", "Sorting by " + std::string(sM[i]),
              NotificationType::Info);
        }
      }
    }

    s_sortOrderDropdownAnim +=
        (s_isSortOrderDropdownOpen ? 1.0f - s_sortOrderDropdownAnim
                                   : 0.0f - s_sortOrderDropdownAnim) *
        0.15f;
    if (s_sortOrderDropdownAnim > 0.01f) {
      const char *oM[] = {"Ascending", "Descending"};
      bool isD = Config::isTabSortDescending();
      float mX = leftX + cPad, mW = leftW - 2 * cPad;
      float mY = paneY + (cardH + cGap) + 28 + idH + 2;
      float mA = alpha * s_sortOrderDropdownAnim;
      for (int i = 0; i < 2; ++i) {
        float iY = mY + i * idH;
        bool hI  = isHovered(mx, my, mX, iY, mW, idH);
        bool sel = (i == 1) == isD;
        if (ClickGUITheme::style() != ClickGUITheme::Style::LiquidGlass) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, 0xFF0D0F13, mA);
          glEnable(GL_TEXTURE_2D);
        }
        drawThemeCard(mX, iY, mW, idH, hI, mA);
        if (sel) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, ClickGUITheme::accent(),
                                       0.08f * mA);
          glEnable(GL_TEXTURE_2D);
        }
        g_guiFont.drawString(
            mX + 10, iY + 7, oM[i],
            applyAlpha(
                sel ? ClickGUITheme::accent() : (hI ? 0xFFFFFFFF : 0xFF9AA0B0), mA),
            0.4f);
        if (clickEvent && hI && s_sortOrderDropdownAnim > 0.8f) {
          Config::setTabSortDescending(i == 1);
          s_isSortOrderDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Sort", std::string("Order: ") + oM[i],
              NotificationType::Info);
        }
      }
    }

    s_tabDisplayDropdownAnim +=
        (s_isTabDisplayDropdownOpen ? 1.0f - s_tabDisplayDropdownAnim
                                    : 0.0f - s_tabDisplayDropdownAnim) *
        0.15f;
    if (s_tabDisplayDropdownAnim > 0.01f) {
      const char *dM[] = {"fk", "fkdr", "wins", "wlr", "ws"};
      std::string cd = Config::getTabDisplayMode();
      float mX = leftX + cPad, mW = leftW - 2 * cPad;
      float mY = paneY + 2 * (cardH + cGap) + 28 + idH + 2;
      float mA = alpha * s_tabDisplayDropdownAnim;
      for (int i = 0; i < 5; ++i) {
        float iY = mY + i * idH;
        bool hI  = isHovered(mx, my, mX, iY, mW, idH);
        bool sel = (cd == dM[i]);
        if (ClickGUITheme::style() != ClickGUITheme::Style::LiquidGlass) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, 0xFF0D0F13, mA);
          glEnable(GL_TEXTURE_2D);
        }
        drawThemeCard(mX, iY, mW, idH, hI, mA);
        if (sel) {
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(mX, iY, mW, idH, 6.0f, ClickGUITheme::accent(),
                                       0.08f * mA);
          glEnable(GL_TEXTURE_2D);
        }
        g_guiFont.drawString(
            mX + 10, iY + 7, dM[i],
            applyAlpha(
                sel ? ClickGUITheme::accent() : (hI ? 0xFFFFFFFF : 0xFF9AA0B0), mA),
            0.4f);
        if (clickEvent && hI && s_tabDisplayDropdownAnim > 0.8f) {
          Config::setTabDisplayMode(dM[i]);
          s_isTabDisplayDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Tab", "Display: " + std::string(dM[i]),
              NotificationType::Info);
        }
      }
    }
  }
}

} // namespace Tabs
} // namespace Render
