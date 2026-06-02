#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/StatsOverlay.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <gl/GL.h>
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

  g_guiFont.drawString(cx, cy, "Overlays", applyAlpha(0xFFFFFFFF, alpha));
  cy += 40;
  auto drawSettingsCard = [&](const char *title, const char *desc, bool &val,
                              int id, float &cy_ref) {
    bool hover = isHovered(mx, my, mainX + 190, cy_ref - 10, g_w - 210, 62);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy_ref - 10, g_w - 210, 62, hover, alpha);
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
  drawSettingsCard("Refined Notifications",
                   "Enable silky smooth toast alerts", notifEnabled, 2, cy);
  Config::setNotificationsEnabled(notifEnabled);

  bool techEnabled = Config::isTechEnabled();
  drawSettingsCard("Tech Overlay", "Show technical JNI and system metrics",
                   techEnabled, 9, cy);
  Config::setTechEnabled(techEnabled);

  bool blurEnabled = Config::isMotionBlurEnabled();
  drawSettingsCard("Motion Blur", "Adds a cinematic trail to camera movement",
                   blurEnabled, 4, cy);
  Config::setMotionBlurEnabled(blurEnabled);
  cy += 10;

  if (blurEnabled) {
    g_guiFont.drawString(cx, cy, "Blur Intensity",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 20;
    float sliderW = 200.0f;
    float sliderH = 10.0f;
    float sliderVal = Config::getMotionBlurAmount();

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRect(cx, cy, sliderW, sliderH, 0xFF2A2A2E, alpha);
    RenderUtils::drawRect(cx, cy, sliderW * sliderVal, sliderH,
                          Config::getThemeColor(), alpha);
    glEnable(GL_TEXTURE_2D);

    bool hSlider = isHovered(mx, my, cx, cy - 5, sliderW, sliderH + 10);
    if (hSlider && lClick) {
      float newVal = (mx - cx) / sliderW;
      if (newVal < 0) newVal = 0;
      if (newVal > 1) newVal = 1;
      Config::setMotionBlurAmount(newVal);
    }
    cy += 30;
  }

  bool nameTagsEnabled = Config::isNameTagsEnabled();
  drawSettingsCard("NameTags",
                   "Renders the Bedwars stats above every nearby player",
                   nameTagsEnabled, 7, cy);
  Config::setNameTagsEnabled(nameTagsEnabled);
  cy += 10;

  if (nameTagsEnabled) {
    const float panelX = mainX + 190;
    const float panelW = g_w - 210;
    {
      float h = Config::getNameTagHeight();
      const float cardH = 34.0f;
      float rowY = cy - 6;
      bool rowHover = isHovered(mx, my, panelX, rowY, panelW, cardH);

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(panelX, rowY, panelW, cardH, rowHover, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx, cy, "Label height",
                           applyAlpha(0xFFFFFFFF, alpha));

      char hbuf[24];
      sprintf_s(hbuf, "%.1f m", h);

      const float btnW = 22, btnH = 22;
      const float btnY = cy - 1;
      const float upX = panelX + panelW - 12 - btnW;
      const float dnX = upX - 6 - btnW;
      const float valueX = dnX - 50;

      g_guiFont.drawString(valueX, cy, hbuf,
                           applyAlpha(0xFFE6E6EA, alpha));

      bool hUp = isHovered(mx, my, upX, btnY, btnW, btnH);
      bool hDn = isHovered(mx, my, dnX, btnY, btnW, btnH);
      glDisable(GL_TEXTURE_2D);
      drawThemeButton(upX, btnY, btnW, btnH, hUp, false, alpha);
      drawThemeButton(dnX, btnY, btnW, btnH, hDn, false, alpha);
      const float cxU = upX + btnW / 2.0f;
      const float cyU = btnY + btnH / 2.0f;
      const float cxD = dnX + btnW / 2.0f;
      const float cyD = btnY + btnH / 2.0f;
      const float armLen = 9.0f, armThick = 2.0f;
      DWORD sym = 0xFFFFFFFF;
      RenderUtils::drawRect(cxU - armLen / 2, cyU - armThick / 2,
                            armLen, armThick, sym, alpha);
      RenderUtils::drawRect(cxU - armThick / 2, cyU - armLen / 2,
                            armThick, armLen, sym, alpha);
      RenderUtils::drawRect(cxD - armLen / 2, cyD - armThick / 2,
                            armLen, armThick, sym, alpha);
      glEnable(GL_TEXTURE_2D);

      if (hUp && clickEvent && h < 4.0f)
        Config::setNameTagHeight(h + 0.1f);
      if (hDn && clickEvent && h > 0.5f)
        Config::setNameTagHeight(h - 0.1f);

      cy += 30;
    }

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

    const float rowH = 32.0f;
    const float rowGap = 4.0f;
    const float handleW = 26.0f;

    static int   s_dragIdx     = -1;
    static float s_dragMouseDY = 0.0f;
    static bool  s_dragMoved   = false;

    static std::vector<float> s_rowAnimY(6, 0.0f);
    if (s_rowAnimY.size() != slots.size())
      s_rowAnimY.assign(slots.size(), 0.0f);
    for (auto &v : s_rowAnimY) v *= 0.75f;

    if (clickEvent) {
      for (size_t r = 0; r < slots.size(); ++r) {
        float ry = cy + r * (rowH + rowGap);
        if (isHovered(mx, my, panelX, ry, handleW, rowH)) {
          s_dragIdx = (int)r;
          s_dragMouseDY = my - ry;
          s_dragMoved = false;
          break;
        }
      }
    }

    if (s_dragIdx >= 0 && lClick) {
      int over = (int)((my - cy) / (rowH + rowGap));
      if (over < 0) over = 0;
      if (over >= (int)slots.size()) over = (int)slots.size() - 1;
      if (over != s_dragIdx) {
        int from = s_dragIdx, to = over;
        if (from < to) {
          for (int k = from + 1; k <= to; ++k)
            s_rowAnimY[k] = -(rowH + rowGap);
        } else {
          for (int k = to; k < from; ++k)
            s_rowAnimY[k] = +(rowH + rowGap);
        }
        auto moved = slots[from];
        slots.erase(slots.begin() + from);
        slots.insert(slots.begin() + to, moved);
        Config::setNameTagStats(slots);
        s_dragIdx = to;
        s_dragMoved = true;
      }
    }
    if (!lClick) s_dragIdx = -1;

    for (size_t r = 0; r < slots.size(); ++r) {
      float ry = cy + r * (rowH + rowGap) + s_rowAnimY[r];
      bool isDragging = ((int)r == s_dragIdx);
      bool rowHover = isHovered(mx, my, panelX, ry, panelW, rowH);
      bool en = slots[r].second;

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(panelX, ry, panelW, rowH, rowHover || isDragging, alpha);
      if (en) {
        if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
          RenderUtils::drawRoundedRect(panelX + 5, ry + 6, 3, rowH - 12, 1.5f,
                                       THEME_NAVY, alpha);
        } else {
          RenderUtils::drawRoundedRect(panelX, ry, 3, rowH, 1.5f,
                                       THEME_NAVY, alpha);
        }
      }
      DWORD gripCol = (rowHover || isDragging) ? 0xFFE0E0E5 : 0xFF6F6F75;
      float gx = panelX + 10, gy = ry + rowH / 2;
      for (int dy = -1; dy <= 1; ++dy) {
        RenderUtils::drawCircle(gx,     gy + dy * 5, 1.4f, gripCol, alpha);
        RenderUtils::drawCircle(gx + 5, gy + dy * 5, 1.4f, gripCol, alpha);
      }
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(panelX + handleW + 14, ry + 9,
                           labelFor(slots[r].first),
                           applyAlpha(0xFFFFFFFF, alpha));

      float swX = panelX + panelW - 50;
      float swY = ry + 7;
      drawSwitch(2000 + (int)r, swX, swY, en, rowHover, alpha);

      bool clickOnSwitch =
          isHovered(mx, my, swX, swY, 34.0f, 18.0f);
      bool clickOnRowBody =
          rowHover && !isHovered(mx, my, panelX, ry, handleW, rowH);
      if (clickEvent && (clickOnSwitch || clickOnRowBody) &&
          !s_dragMoved && s_dragIdx < 0) {
        slots[r].second = !en;
        Config::setNameTagStats(slots);
      }
    }
    if (!lClick) s_dragMoved = false;

    cy += slots.size() * (rowH + rowGap) + 10;
  }

  cy += 20;
  g_guiFont.drawString(cx, cy, "Table Customization",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 35;

  g_guiFont.drawString(cx, cy, "Sort By:", applyAlpha(0xFFA0A0A5, alpha));
  const char *sModes[] = {"Team", "Star", "FK", "FKDR", "Wins", "WLR", "WS"};
  std::string curSort = Config::getSortMode();

  float bx = cx + 80;
  for (int i = 0; i < 7; ++i) {
    bool hov = isHovered(mx, my, bx, cy - 5, 50, 25);
    bool sel = (curSort == sModes[i]);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(bx, cy - 5, 50, 25, hov, sel, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(bx + 5, cy - 2, sModes[i],
                         applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha),
                         0.45f);
    if (clickEvent && hov) {
      Config::setSortMode(sModes[i]);
      NotificationManager::getInstance()->add(
          "Sort", "Sorting by " + std::string(sModes[i]),
          NotificationType::Info);
    }
    bx += 58;
  }

  cy += 40;
  g_guiFont.drawString(cx, cy,
                       "Tab List Display:", applyAlpha(0xFFA0A0A5, alpha));
  const char *dModes[] = {"fk", "fkdr", "wins", "wlr", "ws"};
  std::string curMode = Config::getTabDisplayMode();

  float dbx = cx + 170;
  for (int i = 0; i < 5; ++i) {
    bool hov = isHovered(mx, my, dbx, cy - 5, 45, 25);
    bool sel = (curMode == dModes[i]);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(dbx, cy - 5, 45, 25, hov, sel, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(
        dbx + (strlen(dModes[i]) > 3 ? 2 : 10), cy - 2, dModes[i],
        applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha), 0.4f);
    if (clickEvent && hov) {
      Config::setTabDisplayMode(dModes[i]);
      NotificationManager::getInstance()->add(
          "Tab", "Display set to " + std::string(dModes[i]),
          NotificationType::Info);
    }
    dbx += 50;
  }
  cy += 45;

  {
    bool isDesc = Config::isTabSortDescending();
    std::string currentOrder = isDesc ? "Descending" : "Ascending";
    const char *orders[] = {"Ascending", "Descending"};

    g_guiFont.drawString(cx, cy + 8,
                         "Sort Order:", applyAlpha(0xFFA0A0A5, alpha));

    float dropX = cx + 100;
    float dropW = 160.0f;
    float dropH = 32.0f;
    bool hovDrop = isHovered(mx, my, dropX, cy, dropW, dropH);

    s_sortOrderDropdownAnim +=
        (s_isSortOrderDropdownOpen ? 1.0f - s_sortOrderDropdownAnim
                                   : 0.0f - s_sortOrderDropdownAnim) *
        0.15f;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(dropX, cy, dropW, dropH, hovDrop, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(dropX + 10, cy + 6, currentOrder,
                         applyAlpha(0xFFFFFFFF, alpha), 0.45f);
    g_guiFont.drawString(dropX + dropW - 18, cy + 10,
                         s_isSortOrderDropdownOpen ? "-" : "+",
                         applyAlpha(0xFFA0A0A5, alpha), 0.45f);

    if (clickEvent && hovDrop) {
      s_isSortOrderDropdownOpen = !s_isSortOrderDropdownOpen;
    }

    if (s_sortOrderDropdownAnim > 0.01f) {
      float listY = cy + dropH + 2;
      for (int i = 0; i < 2; ++i) {
        float itemY = listY + (i * dropH);
        bool hItem = isHovered(mx, my, dropX, itemY, dropW, dropH);

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(dropX, itemY, dropW, dropH, hItem, alpha * s_sortOrderDropdownAnim);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(
            dropX + 15, itemY + 12, orders[i],
            applyAlpha(currentOrder == orders[i] ? 0xFFFFFFFF : 0xFFA0A0A5,
                       alpha * s_sortOrderDropdownAnim),
            0.45f);

        if (clickEvent && hItem && (s_sortOrderDropdownAnim > 0.8f)) {
          Config::setTabSortDescending(i == 1);
          s_isSortOrderDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Sort", std::string("Order set to: ") + orders[i],
              NotificationType::Info);
        }
      }
      cy += (2 * dropH) * s_sortOrderDropdownAnim;
    }
  }
  cy += 50;
  g_guiFont.drawString(cx, cy,
                       "Visible Columns:", applyAlpha(0xFFA0A0A5, alpha));
  cy += 30;

  {
    const char *targets[] = {"GUI Overlay", "Better Tab"};
    g_guiFont.drawString(cx, cy + 8,
                         "Target:", applyAlpha(0xFFA0A0A5, alpha));

    float dropX = cx + 60;
    float dropW = 140.0f;
    float dropH = 30.0f;
    bool hovDrop = isHovered(mx, my, dropX, cy, dropW, dropH);

    s_columnTargetDropdownAnim +=
        (s_isColumnTargetDropdownOpen ? 1.0f - s_columnTargetDropdownAnim
                                      : 0.0f - s_columnTargetDropdownAnim) *
        0.15f;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(dropX, cy, dropW, dropH, hovDrop, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(dropX + 10, cy + 6, targets[s_columnTargetMode],
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);

    if (clickEvent && hovDrop)
      s_isColumnTargetDropdownOpen = !s_isColumnTargetDropdownOpen;

    if (s_columnTargetDropdownAnim > 0.01f) {
      float listY = cy + dropH + 2;
      for (int i = 0; i < 2; ++i) {
        float itemY = listY + (i * dropH);
        bool hItem = isHovered(mx, my, dropX, itemY, dropW, dropH);
        glDisable(GL_TEXTURE_2D);
        drawThemeCard(dropX, itemY, dropW, dropH, hItem, alpha * s_columnTargetDropdownAnim);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(
            dropX + 10, itemY + 10, targets[i],
            applyAlpha(s_columnTargetMode == i ? 0xFFFFFFFF : 0xFFA0A0A5,
                       alpha * s_columnTargetDropdownAnim),
            0.42f);
        if (clickEvent && hItem && s_columnTargetDropdownAnim > 0.8f) {
          s_columnTargetMode = i;
          s_isColumnTargetDropdownOpen = false;
        }
      }
      cy += (2 * dropH) * s_columnTargetDropdownAnim;
    }
  }
  cy += 45;

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

  float tx = cx;
  for (size_t i = 0; i < toggles.size(); ++i) {
    float cardW = 125.0f;
    float cardH = 36.0f;
    bool hov = isHovered(mx, my, tx, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(tx, cy, cardW, cardH, hov, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(
        tx + 12, cy + cardH / 2.0f - 9.0f, toggles[i].name,
        applyAlpha(toggles[i].enabled ? 0xFFFFFFFF : 0xFF808085, alpha),
        0.45f);

    glDisable(GL_TEXTURE_2D);
    drawSwitch(100 + (int)i, tx + cardW - 45, cy + (cardH - 18.0f) / 2.0f,
               toggles[i].enabled, hov, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hov) {
      toggles[i].setter(!toggles[i].enabled);
    }

    if (i % 3 == 2) {
      tx = cx;
      cy += cardH + 10;
    } else {
      tx += cardW + 12;
    }
  }
  cy += 60;

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

    if (clickEvent && hTab) {
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
}

} // namespace Tabs
} // namespace Render
