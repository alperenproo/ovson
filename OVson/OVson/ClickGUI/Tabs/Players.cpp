#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Config/StatColors.h"
#include "../../Utils/BedwarsPrestiges.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <gl/GL.h>
#include <sstream>
#include <string>

namespace Render {
namespace Tabs {

void renderPlayers(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Player Search",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 24;

  float searchW = g_w - 200;
  float searchH = 36.0f;
  float searchX = mainX + 186;
  bool hSearch = isHovered(mx, my, searchX, cy, searchW, searchH);
  glDisable(GL_TEXTURE_2D);
  drawTextInput(searchX, cy, searchW, searchH, s_typingSearch, hSearch, alpha);
  glEnable(GL_TEXTURE_2D);

  std::string dispSearch = s_playerSearch;
  if (s_typingSearch && (GetTickCount64() / 500) % 2 == 0)
    dispSearch += "|";
  if (dispSearch.empty() && !s_typingSearch)
    dispSearch = "Search player...";

  g_guiFont.drawString(
      searchX + 12, cy + 7, dispSearch,
      applyAlpha(s_typingSearch ? 0xFFFFFFFF : 0xFF606065, alpha));

  if (clickEvent && hSearch) {
    s_typingSearch = true;
    s_typingApiKey = s_typingAutoGG = s_typingUrchinKey = false;
  } else if (clickEvent && !hSearch)
    s_typingSearch = false;

  cy += searchH + 10;

  if (s_searching) {
    float loadCardW = g_w - 210;
    float loadCardH = 50.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy, loadCardW, loadCardH, false, alpha);
    glEnable(GL_TEXTURE_2D);
    int dots = 1 + ((GetTickCount64() / 400) % 3);
    std::string loadText = "Fetching stats";
    for (int i = 0; i < dots; i++)
      loadText += ".";
    g_guiFont.drawString(mainX + 210, cy + 16, loadText,
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += loadCardH + 10;
  } else if (s_hasLookup) {
    if (s_skinPendingReady) {
      if (s_lookupSkinTexId) {
        glDeleteTextures(1, &s_lookupSkinTexId);
        s_lookupSkinTexId = 0;
      }
      glGenTextures(1, &s_lookupSkinTexId);
      glBindTexture(GL_TEXTURE_2D, s_lookupSkinTexId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_skinPendingW, s_skinPendingH,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, s_skinPendingData.data());
      glBindTexture(GL_TEXTURE_2D, 0);
      s_skinPendingReady = false;
      s_skinPendingData.clear();
    }

    float cardW = g_w - 210;
    float cardX = mainX + 190;

    auto getRankDisplay = [&]() -> std::string {
      const auto &r = s_lookupResult;
      if (!r.prefix.empty())
        return r.prefix;
      if (r.rank == "ADMIN")
        return "\xC2\xA7" "c[ADMIN]";
      if (r.rank == "MODERATOR")
        return "\xC2\xA7" "2[MOD]";
      if (r.rank == "HELPER")
        return "\xC2\xA7" "9[HELPER]";
      if (r.rank == "YOUTUBER")
        return "\xC2\xA7" "c[\xC2\xA7" "fYOUTUBE\xC2\xA7" "c]";
      if (r.monthlyPackageRank == "SUPERSTAR") {
        std::string plusCol = "\xC2\xA7" "c";
        if (r.rankPlusColor == "GOLD")        plusCol = "\xC2\xA7" "6";
        else if (r.rankPlusColor == "AQUA")   plusCol = "\xC2\xA7" "b";
        else if (r.rankPlusColor == "GREEN")  plusCol = "\xC2\xA7" "a";
        else if (r.rankPlusColor == "LIGHT_PURPLE") plusCol = "\xC2\xA7" "d";
        else if (r.rankPlusColor == "WHITE")  plusCol = "\xC2\xA7" "f";
        else if (r.rankPlusColor == "BLUE")   plusCol = "\xC2\xA7" "9";
        else if (r.rankPlusColor == "DARK_RED")    plusCol = "\xC2\xA7" "4";
        else if (r.rankPlusColor == "DARK_AQUA")   plusCol = "\xC2\xA7" "3";
        else if (r.rankPlusColor == "DARK_GREEN")  plusCol = "\xC2\xA7" "2";
        else if (r.rankPlusColor == "DARK_PURPLE") plusCol = "\xC2\xA7" "5";
        else if (r.rankPlusColor == "YELLOW") plusCol = "\xC2\xA7" "e";
        return "\xC2\xA7" "6[MVP" + plusCol + "++" + "\xC2\xA7" "6]";
      }
      if (r.newPackageRank == "MVP_PLUS") {
        std::string plusCol = "\xC2\xA7" "c";
        if (r.rankPlusColor == "GOLD")        plusCol = "\xC2\xA7" "6";
        else if (r.rankPlusColor == "AQUA")   plusCol = "\xC2\xA7" "b";
        else if (r.rankPlusColor == "GREEN")  plusCol = "\xC2\xA7" "a";
        else if (r.rankPlusColor == "LIGHT_PURPLE") plusCol = "\xC2\xA7" "d";
        else if (r.rankPlusColor == "WHITE")  plusCol = "\xC2\xA7" "f";
        else if (r.rankPlusColor == "BLUE")   plusCol = "\xC2\xA7" "9";
        else if (r.rankPlusColor == "DARK_RED")    plusCol = "\xC2\xA7" "4";
        else if (r.rankPlusColor == "DARK_AQUA")   plusCol = "\xC2\xA7" "3";
        else if (r.rankPlusColor == "DARK_GREEN")  plusCol = "\xC2\xA7" "2";
        else if (r.rankPlusColor == "DARK_PURPLE") plusCol = "\xC2\xA7" "5";
        else if (r.rankPlusColor == "YELLOW") plusCol = "\xC2\xA7" "e";
        return "\xC2\xA7" "b[MVP" + plusCol + "+" + "\xC2\xA7" "b]";
      }
      if (r.newPackageRank == "MVP")      return "\xC2\xA7" "b[MVP]";
      if (r.newPackageRank == "VIP_PLUS") return "\xC2\xA7" "a[VIP\xC2\xA7" "6+\xC2\xA7" "a]";
      if (r.newPackageRank == "VIP")      return "\xC2\xA7" "a[VIP]";
      return "\xC2\xA7" "7";
    };

    float headerH = 48.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, headerH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    float textOffX = 0;
    if (s_lookupSkinTexId) {
      glColor4f(1.0f, 1.0f, 1.0f, alpha);
      glBindTexture(GL_TEXTURE_2D, s_lookupSkinTexId);
      glBegin(GL_QUADS);
      float sw = 32.0f;
      float sh = 32.0f;
      float sx = cardX + 10;
      float sy = cy + (headerH - sh) / 2.0f;
      glTexCoord2f(0.0f, 0.0f); glVertex2f(sx, sy);
      glTexCoord2f(0.0f, 1.0f); glVertex2f(sx, sy + sh);
      glTexCoord2f(1.0f, 1.0f); glVertex2f(sx + sw, sy + sh);
      glTexCoord2f(1.0f, 0.0f); glVertex2f(sx + sw, sy);
      glEnd();
      glBindTexture(GL_TEXTURE_2D, 0);
      textOffX = 42.0f;
    }

    std::string rankStr = getRankDisplay();
    std::string nameWithRank = rankStr + " " + s_lookupName;
    g_guiFont.drawString(cardX + 14 + textOffX, cy + 8, nameWithRank,
                         applyAlpha(0xFFFFFFFF, alpha));

    std::string starFormatted =
        BedwarsStars::GetFormattedLevel(s_lookupResult.bedwarsStar);
    g_guiFont.drawString(cardX + 14 + textOffX, cy + 26, starFormatted,
                         applyAlpha(0xFFFFFFFF, alpha));

    std::string nlText =
        "Level " + std::to_string(s_lookupResult.networkLevel);
    g_guiFont.drawString(cardX + cardW - 120, cy + 8, nlText,
                         applyAlpha(0xFF808085, alpha), 0.45f);
    if (!s_lookupResult.uuid.empty()) {
      std::string shortUuid = s_lookupResult.uuid.substr(0, 8) + "...";
      g_guiFont.drawString(cardX + cardW - 120, cy + 22, shortUuid,
                           applyAlpha(0xFF505055, alpha), 0.4f);
    }

    cy += headerH + 8;

    struct StatEntry {
      const char *label;
      std::string value;
      uint32_t color;
    };

    double fkdr = (s_lookupResult.bedwarsFinalDeaths == 0)
                      ? (double)s_lookupResult.bedwarsFinalKills
                      : (double)s_lookupResult.bedwarsFinalKills /
                            s_lookupResult.bedwarsFinalDeaths;
    double kdr = (s_lookupResult.bedwarsDeaths == 0)
                     ? (double)s_lookupResult.bedwarsKills
                     : (double)s_lookupResult.bedwarsKills /
                           s_lookupResult.bedwarsDeaths;
    double blr = (s_lookupResult.bedwarsBedsLost == 0)
                     ? (double)s_lookupResult.bedwarsBedsBroken
                     : (double)s_lookupResult.bedwarsBedsBroken /
                           s_lookupResult.bedwarsBedsLost;
    double wlr = (s_lookupResult.bedwarsLosses == 0)
                     ? (double)s_lookupResult.bedwarsWins
                     : (double)s_lookupResult.bedwarsWins /
                           s_lookupResult.bedwarsLosses;

    auto fmtK = [](int v) -> std::string {
      if (v >= 1000000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fM", v / 1000000.0);
        return buf;
      }
      if (v >= 10000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fK", v / 1000.0);
        return buf;
      }
      return std::to_string(v);
    };
    auto fmtR = [](double v) -> std::string {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.2f", v);
      return buf;
    };

    StatEntry stats[] = {
        {"Final Kills", fmtK(s_lookupResult.bedwarsFinalKills),
         StatColors::getColor(StatColors::StatType::FinalKills,
                              s_lookupResult.bedwarsFinalKills)},
        {"FKDR", fmtR(fkdr),
         StatColors::getColor(StatColors::StatType::FKDR, fkdr)},
        {"Kills", fmtK(s_lookupResult.bedwarsKills),
         StatColors::getColor(StatColors::StatType::Kills,
                              s_lookupResult.bedwarsKills)},
        {"KDR", fmtR(kdr),
         StatColors::getColor(StatColors::StatType::KDR, kdr)},
        {"Beds Broken", fmtK(s_lookupResult.bedwarsBedsBroken),
         StatColors::getColor(StatColors::StatType::Beds,
                              s_lookupResult.bedwarsBedsBroken)},
        {"BLR", fmtR(blr),
         StatColors::getColor(StatColors::StatType::BLR, blr)},
        {"Wins", fmtK(s_lookupResult.bedwarsWins),
         StatColors::getColor(StatColors::StatType::Wins,
                              s_lookupResult.bedwarsWins)},
        {"WLR", fmtR(wlr),
         StatColors::getColor(StatColors::StatType::WLR, wlr)},
        {"Winstreak", std::to_string(s_lookupResult.winstreak),
         StatColors::getColor(StatColors::StatType::WS,
                              s_lookupResult.winstreak)},
    };
    int statCount = sizeof(stats) / sizeof(stats[0]);

    float colW = (cardW - 8) / 2.0f;
    float statRowH = 36.0f;
    int rowsNeeded = (statCount + 1) / 2;
    float gridH = rowsNeeded * statRowH + 12.0f;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, gridH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    for (int i = 0; i < statCount; i++) {
      int col = i % 2;
      int row = i / 2;
      float sx = cardX + 14 + col * colW;
      float sy = cy + 6 + row * statRowH;

      g_guiFont.drawString(sx, sy, stats[i].label,
                           applyAlpha(0xFF808085, alpha), 0.4f);
      g_guiFont.drawString(sx, sy + 14, stats[i].value,
                           applyAlpha(stats[i].color, alpha));
    }

    {
      float sx0 = cardX + 14;
      g_guiFont.drawString(
          sx0 + g_guiFont.getStringWidth(stats[0].value) + 6, cy + 6 + 14 + 2,
          "/ " + std::to_string(s_lookupResult.bedwarsFinalDeaths) + " FD",
          applyAlpha(0xFF505055, alpha), 0.35f);
      g_guiFont.drawString(
          sx0 + g_guiFont.getStringWidth(stats[2].value) + 6,
          cy + 6 + statRowH + 14 + 2,
          "/ " + std::to_string(s_lookupResult.bedwarsDeaths) + " D",
          applyAlpha(0xFF505055, alpha), 0.35f);
      g_guiFont.drawString(
          sx0 + g_guiFont.getStringWidth(stats[4].value) + 6,
          cy + 6 + 2 * statRowH + 14 + 2,
          "/ " + std::to_string(s_lookupResult.bedwarsBedsLost) + " BL",
          applyAlpha(0xFF505055, alpha), 0.35f);
      g_guiFont.drawString(
          sx0 + g_guiFont.getStringWidth(stats[6].value) + 6,
          cy + 6 + 3 * statRowH + 14 + 2,
          "/ " + std::to_string(s_lookupResult.bedwarsLosses) + " L",
          applyAlpha(0xFF505055, alpha), 0.35f);
    }

    cy += gridH + 8;

    if (Config::isTagsEnabled()) {
      std::string activeS = Config::getActiveTagService();

      bool servesUrchin = (activeS == "Urchin" || activeS == "Both" ||
                           activeS == "Khadow");
      bool servesSeraph = (activeS == "Seraph" || activeS == "Both" ||
                           activeS == "Khadow");
      bool hasUrchin = s_lookupUrchinTags &&
                       !s_lookupUrchinTags->tags.empty() && servesUrchin;
      bool hasSeraph = s_lookupSeraphTags &&
                       !s_lookupSeraphTags->tags.empty() && servesSeraph;

      auto measureWrappedLines = [&](const std::string &text) -> int {
        int lines = 0;
        std::string line;
        std::string word;
        std::stringstream ss(text);
        while (ss >> word) {
          if (g_guiFont.getStringWidth(line + word) > cardW - 40) {
            lines++;
            line = "";
          }
          line += (line.empty() ? "" : " ") + word;
        }
        if (!line.empty())
          lines++;
        return lines;
      };

      auto drawWrapped = [&](const std::string &text, uint32_t color,
                             float &currY) {
        std::string line;
        std::string word;
        std::stringstream ss(text);
        while (ss >> word) {
          if (g_guiFont.getStringWidth(line + word) > cardW - 40) {
            g_guiFont.drawString(cardX + 14, currY, line,
                                 applyAlpha(color, alpha));
            currY += 16.0f;
            line = "";
          }
          line += (line.empty() ? "" : " ") + word;
        }
        if (!line.empty()) {
          g_guiFont.drawString(cardX + 14, currY, line,
                               applyAlpha(color, alpha));
          currY += 16.0f;
        }
      };

      if (hasUrchin) {
        int totalLines = 0;
        for (const auto &t : s_lookupUrchinTags->tags) {
          std::string tstr = t.type;
          if (tstr.empty())
            continue;
          std::string tagText = "[" + t.type + "]";
          if (!t.reason.empty())
            tagText += " - " + t.reason;
          totalLines += measureWrappedLines(tagText);
        }
        float tagCardH = 34.0f + totalLines * 16.0f;

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(cardX, cy, cardW, tagCardH, false, alpha);
        if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
          RenderUtils::drawRoundedRect(cardX + 5, cy + 6, 3, tagCardH - 12, 1.5f,
                                       0xFF55FFFF, alpha);
        } else {
          RenderUtils::drawRect(cardX, cy + 4, 3, tagCardH - 8, 0xFF55FFFF,
                                alpha);
        }
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(cardX + 14, cy + 6, "Urchin Tags",
                             applyAlpha(0xFF55FFFF, alpha), 0.45f);
        float tagY = cy + 24;
        for (const auto &t : s_lookupUrchinTags->tags) {
          std::string tagText = "\xC2\xA7" "e[" + t.type + "]";
          if (!t.reason.empty())
            tagText += " \xC2\xA7" "7- " + t.reason;
          drawWrapped(tagText, 0xFFCCCCCC, tagY);
        }
        cy += tagCardH + 6;
      }

      if (hasSeraph) {
        int totalLines = 0;
        for (const auto &t : s_lookupSeraphTags->tags) {
          std::string tstr = t.type;
          if (tstr.empty())
            continue;
          std::string tagText = "[" + t.type + "]";
          if (!t.reason.empty())
            tagText += " - " + t.reason;
          totalLines += measureWrappedLines(tagText);
        }
        float tagCardH = 34.0f + totalLines * 16.0f;

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(cardX, cy, cardW, tagCardH, false, alpha);
        if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
          RenderUtils::drawRoundedRect(cardX + 5, cy + 6, 3, tagCardH - 12, 1.5f,
                                       0xFFFF5555, alpha);
        } else {
          RenderUtils::drawRect(cardX, cy + 4, 3, tagCardH - 8, 0xFFFF5555,
                                alpha);
        }
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(cardX + 14, cy + 6, "Seraph Blacklist",
                             applyAlpha(0xFFFF5555, alpha), 0.45f);
        float tagY = cy + 24;
        for (const auto &t : s_lookupSeraphTags->tags) {
          std::string tagText = "\xC2\xA7" "c[" + t.type + "]";
          if (!t.reason.empty())
            tagText += " \xC2\xA7" "7- " + t.reason;
          drawWrapped(tagText, 0xFFCCCCCC, tagY);
        }
        cy += tagCardH + 6;
      }
    }
  }
  cy += 20;
}

} // namespace Tabs
} // namespace Render
