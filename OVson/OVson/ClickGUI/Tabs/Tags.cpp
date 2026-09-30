#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Logic/StatsTracker.h"
#include "../../Logic/StatsTracker.internal.h"
#include "../../Services/SeraphService.h"
#include "../../Services/UrchinService.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <gl/GL.h>
#include <mutex>
#include <string>

namespace Render {
namespace Tabs {

void renderTags(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  const float cardX = mainX + 190.0f;
  const float cardW = g_w - 210.0f;
  const float swX   = mainX + g_w - 65.0f;

  if (s_moduleSearch.empty()) {
    drawSectionLabel(cx, cy, "Tagging Services", alpha);
    cy += 30.0f;
  }

  if (shouldShowInSearch("Enable Tags", "Master switch for all tagging services")) {
    float cardH = 64.0f;
    bool tagsEnabled = Config::isTagsEnabled();
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawSwitch(10, swX, cy + 19.0f, tagsEnabled, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Enable Tags", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Master switch for all tagging services", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (clickEvent && hCard) {
      Config::setTagsEnabled(!tagsEnabled);
      NotificationManager::getInstance()->add(
          "Tags", tagsEnabled ? "Tags Disabled" : "Tags Enabled",
          !tagsEnabled ? NotificationType::Success : NotificationType::Warning);
    }

    cy += cardH + 12.0f;
  }

  bool tagsEnabled = Config::isTagsEnabled();

  if (tagsEnabled && shouldShowInSearch("Active Service", "Urchin Seraph Both Khadow tag service selector")) {
    std::string currentService = Config::getActiveTagService();
    const char *services[] = {"Urchin", "Seraph", "Both", "Khadow"};
    constexpr int kServiceCount = (int)(sizeof(services) / sizeof(services[0]));

    float dropW = 180.0f;
    float dropH = 32.0f;
    float dropX = cardX + cardW - dropW - 16.0f;

    s_tagsDropdownAnim += (s_isTagsDropdownOpen ? 1.0f - s_tagsDropdownAnim
                                                : 0.0f - s_tagsDropdownAnim) *
                          0.15f;

    float dropdownContentH = 0.0f;
    if (s_tagsDropdownAnim > 0.01f)
      dropdownContentH = (kServiceCount * (dropH + 2.0f) + 8.0f) * s_tagsDropdownAnim;

    float cardH = 64.0f + dropdownContentH;
    float dropY = cy + 16.0f;
    bool hovDrop = isHovered(mx, my, dropX, dropY, dropW, dropH);
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawThemeCard(dropX, dropY, dropW, dropH, hovDrop, 0.9f * alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Active Service", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Tag lookup provider to use for player scanning", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    g_guiFont.drawString(dropX + 12.0f, dropY + 7.0f, currentService,
                         applyAlpha(0xFFFFFFFF, alpha), 0.40f);
    drawChevron(dropX + dropW - 16.0f, dropY + dropH * 0.5f, 4.0f,
                s_isTagsDropdownOpen, 0xFFA0A0A5, alpha);

    if (clickEvent && hovDrop)
      s_isTagsDropdownOpen = !s_isTagsDropdownOpen;

    if (s_tagsDropdownAnim > 0.01f) {
      float listY = dropY + dropH + 4.0f;
      for (int i = 0; i < kServiceCount; ++i) {
        float itemY = listY + (i * (dropH + 2.0f));
        bool hItem = isHovered(mx, my, dropX, itemY, dropW, dropH);

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(dropX, itemY, dropW, dropH, hItem, 0.95f * alpha * s_tagsDropdownAnim);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(
            dropX + 14.0f, itemY + 7.0f, services[i],
            applyAlpha(currentService == services[i] ? 0xFFFFFFFF : 0xFFA0A0A5,
                       alpha * s_tagsDropdownAnim), 0.40f);

        if (clickEvent && hItem && (s_tagsDropdownAnim > 0.8f)) {
          Config::setActiveTagService(services[i]);
          s_isTagsDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Tags", "Active service set to: " + std::string(services[i]),
              NotificationType::Info);
        }
      }
    }

    cy += cardH + 12.0f;
  }

  if (tagsEnabled) {
    if (s_moduleSearch.empty()) {
      cy += 6.0f;
      drawSectionLabel(cx, cy, "Muted Tag Alerts", alpha);
      cy += 30.0f;
    }

    if (shouldShowInSearch("Mute Tag Warnings", "Disable chat warnings notifications mute self teammate")) {
      bool muteEnabled = Config::isMuteTagAlertsEnabled();
      bool muteSelfEnabled = Config::isMuteSelfTagAlertsEnabled();
      bool muteTeamEnabled = Config::isMuteTeamTagAlertsEnabled();
      float cardH = muteEnabled ? 150.0f : 64.0f;
      bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
      bool hTopRow = isHovered(mx, my, cardX, cy, cardW, 50.0f);

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
      drawSwitch(13, swX, cy + 19.0f, muteEnabled, hTopRow, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx, cy + 12.0f, "Mute Tag Warnings", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Disable chat warnings/notifications for specified players", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

      if (muteEnabled) {
        float muteChildAlpha = alpha;

        float selfY = cy + 58.0f;
        bool hSelfRow = isHovered(mx, my, cardX + 16.0f, selfY, cardW - 32.0f, 36.0f);
        g_guiFont.drawString(cx + 10.0f, selfY + 2.0f, "Mute Self Warnings",
                             applyAlpha(0xFFFFFFFF, muteChildAlpha), 0.42f);
        g_guiFont.drawString(cx + 10.0f, selfY + 18.0f, "Never warn about your own tags",
                             applyAlpha(0xFFA0A0A5, muteChildAlpha), 0.36f);
        glDisable(GL_TEXTURE_2D);
        drawSwitch(144, swX, selfY + 5.0f, muteSelfEnabled, hSelfRow, muteChildAlpha);
        glEnable(GL_TEXTURE_2D);

        if (clickEvent && hSelfRow) {
          Config::setMuteSelfTagAlertsEnabled(!muteSelfEnabled);
        }

        float teamY = cy + 100.0f;
        bool hTeamRow = isHovered(mx, my, cardX + 16.0f, teamY, cardW - 32.0f, 36.0f);
        g_guiFont.drawString(cx + 10.0f, teamY + 2.0f, "Mute Teammate Warnings",
                             applyAlpha(0xFFFFFFFF, muteChildAlpha), 0.42f);
        g_guiFont.drawString(cx + 10.0f, teamY + 18.0f, "Never warn about anyone on your own team",
                             applyAlpha(0xFFA0A0A5, muteChildAlpha), 0.36f);
        glDisable(GL_TEXTURE_2D);
        drawSwitch(145, swX, teamY + 5.0f, muteTeamEnabled, hTeamRow, muteChildAlpha);
        glEnable(GL_TEXTURE_2D);

        if (clickEvent && hTeamRow) {
          Config::setMuteTeamTagAlertsEnabled(!muteTeamEnabled);
        }
      }

      if (clickEvent && hTopRow && !(muteEnabled && (isHovered(mx, my, cardX + 16.0f, cy + 58.0f, cardW - 32.0f, 36.0f) ||
                                                      isHovered(mx, my, cardX + 16.0f, cy + 100.0f, cardW - 32.0f, 36.0f)))) {
        Config::setMuteTagAlertsEnabled(!muteEnabled);
      }

      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("Mute Player", "Add player to mute list muted players remove")) {
      bool muteEnabled = Config::isMuteTagAlertsEnabled();
      float activeMuteAlpha = alpha * (muteEnabled ? 1.0f : 0.4f);

      float boxX = cardX + 16.0f;
      float boxW = 250.0f;
      float boxH = 32.0f;

      const auto &mutedPlayers = Config::getMutedTagPlayers();
      float listH = mutedPlayers.empty() ? 0.0f : (mutedPlayers.size() * 28.0f + 16.0f);
      float cardH = 80.0f + listH;
      bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
      float boxY = cy + 36.0f;
      bool hBox = muteEnabled && isHovered(mx, my, boxX, boxY, boxW, boxH);

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hCard, activeMuteAlpha);
      drawTextInput(boxX, boxY, boxW, boxH, s_typingMuteTagPlayer, hBox, activeMuteAlpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx, cy + 12.0f, "Add Player to Mute List", applyAlpha(0xFFFFFFFF, activeMuteAlpha), 0.46f);

      std::string dispMuteInput = s_typingMuteTagPlayer ? s_muteTagPlayerInput : "Enter player name...";
      if (s_typingMuteTagPlayer && (GetTickCount64() / 500) % 2 == 0)
        dispMuteInput += "|";
      g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispMuteInput,
                           applyAlpha(s_typingMuteTagPlayer ? 0xFFFFFFFF : 0xFF6C6C78, activeMuteAlpha), 0.42f);

      if (clickEvent && muteEnabled && hBox) {
        s_typingMuteTagPlayer = true;
        s_typingSeraphKey = s_typingUrchinKey = s_typingSearch = s_typingApiKey = s_typingAutoGG = false;
        s_muteTagPlayerInput = "";
      } else if (clickEvent && s_typingMuteTagPlayer) {
        if (!s_muteTagPlayerInput.empty()) {
          Config::addMutedTagPlayer(s_muteTagPlayerInput);
          NotificationManager::getInstance()->add("Tags", "Player added to mute list: " + s_muteTagPlayerInput,
                                                  NotificationType::Success);
          s_muteTagPlayerInput.clear();
        }
        s_typingMuteTagPlayer = false;
      }

      if (!mutedPlayers.empty()) {
        float listStartY = boxY + boxH + 12.0f;
        for (size_t pi = 0; pi < mutedPlayers.size(); ++pi) {
          float iy = listStartY + pi * 28.0f;
          const std::string &p = mutedPlayers[pi];

          g_guiFont.drawString(boxX, iy + 2.0f, p, applyAlpha(0xFFFFFFFF, activeMuteAlpha), 0.42f);

          float rx = boxX + 220.0f;
          float ry = iy;
          float rw = 22.0f;
          float rh = 22.0f;
          bool hDel = muteEnabled && isHovered(mx, my, rx, ry, rw, rh);

          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(rx, ry, rw, rh, 4.0f, hDel ? 0xFFD32F2F : 0xFF2A2A2D, activeMuteAlpha);
          glDisable(GL_TEXTURE_2D);

          glColor4f(1.0f, 1.0f, 1.0f, activeMuteAlpha);
          glLineWidth(1.5f);
          glBegin(GL_LINES);
          glVertex2f(rx + 6.0f, ry + 6.0f);
          glVertex2f(rx + rw - 6.0f, ry + rh - 6.0f);
          glVertex2f(rx + rw - 6.0f, ry + 6.0f);
          glVertex2f(rx + 6.0f, ry + rh - 6.0f);
          glEnd();
          glEnable(GL_TEXTURE_2D);

          if (clickEvent && hDel) {
            Config::removeMutedTagPlayer(p);
            NotificationManager::getInstance()->add("Tags", "Removed " + p + " from mute list",
                                                    NotificationType::Warning);
            break;
          }
        }
      }

      cy += cardH + 12.0f;
    }
  }

  // gotta respect the legacy code
  /*
  if (s_moduleSearch.empty()) {
    cy += 6.0f;
    drawSectionLabel(cx, cy, "Live Tag Results", alpha);
    cy += 30.0f;
  }

  struct RenderTagCache {
    std::optional<Urchin::PlayerTags> urchin;
    std::optional<Seraph::PlayerTags> seraph;
  };
  static std::unordered_map<std::string, RenderTagCache> s_renderCache;
  static ULONGLONG s_lastRefreshTick = 0;

  ULONGLONG nowTick = GetTickCount64();
  bool shouldRefresh = (nowTick - s_lastRefreshTick) > 2000;

  std::lock_guard<std::recursive_mutex> stLock(OVson::g_statsMutex);
  if (OVson::g_playerStatsMap.empty()) {
    float emptyH = 50.0f;
    bool hEmpty = isHovered(mx, my, cardX, cy, cardW, emptyH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, emptyH, false, alpha * 0.6f);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 16.0f, "No players detected in this session.",
                         applyAlpha(0xFF808085, alpha), 0.42f);
    cy += emptyH + 12.0f;
  } else {
    // Table header
    float headerH = 32.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, headerH, false, alpha * 0.5f);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 8.0f, "Player", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    g_guiFont.drawString(cx + 140.0f, cy + 8.0f, "FK", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    g_guiFont.drawString(cx + 200.0f, cy + 8.0f, "FKDR", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    g_guiFont.drawString(cx + 280.0f, cy + 8.0f, "Urchin", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    g_guiFont.drawString(cx + 420.0f, cy + 8.0f, "Seraph", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    cy += headerH + 4.0f;

    if (shouldRefresh && Config::isTagsEnabled()) {
      s_lastRefreshTick = nowTick;
      std::string activeS = Config::getActiveTagService();
      for (const auto &pair : OVson::g_playerStatsMap) {
        auto &rc = s_renderCache[pair.first];
        if (activeS == "Urchin" || activeS == "Both")
          rc.urchin = Urchin::getPlayerTags(pair.first);
        if (activeS == "Seraph" || activeS == "Both")
          rc.seraph = Seraph::getPlayerTags(pair.first, pair.second.uuid);
      }
    }

    for (const auto &pair : OVson::g_playerStatsMap) {
      const std::string &name = pair.first;
      const auto &stats = pair.second;

      float rowH = 36.0f;
      bool hRow = isHovered(mx, my, cardX, cy, cardW, rowH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, rowH, hRow, alpha * 0.7f);
      glEnable(GL_TEXTURE_2D);

      uint32_t nameCol = 0xFFFFFFFF;
      auto itT = OVson::g_playerTeamColor.find(name);
      if (itT != OVson::g_playerTeamColor.end()) {
        if (itT->second == "Red")    nameCol = 0xFFFF5555;
        else if (itT->second == "Blue")   nameCol = 0xFF5555FF;
        else if (itT->second == "Green")  nameCol = 0xFF55FF55;
        else if (itT->second == "Yellow") nameCol = 0xFFFFFF55;
        else if (itT->second == "Pink")   nameCol = 0xFFFF55FF;
        else if (itT->second == "Aqua")   nameCol = 0xFF55FFFF;
      }
      g_guiFont.drawString(cx, cy + 9.0f, name, applyAlpha(nameCol, alpha), 0.42f);

      g_guiFont.drawString(cx + 140.0f, cy + 9.0f,
                           std::to_string(stats.bedwarsFinalKills),
                           applyAlpha(0xFFCCCCCC, alpha), 0.42f);
      double fkdr =
          (stats.bedwarsFinalDeaths == 0)
              ? stats.bedwarsFinalKills
              : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
      char fBuf[16];
      sprintf_s(fBuf, "%.2f", fkdr);
      g_guiFont.drawString(cx + 200.0f, cy + 9.0f, fBuf, applyAlpha(0xFFCCCCCC, alpha), 0.42f);

      if (Config::isTagsEnabled()) {
        std::string activeS = Config::getActiveTagService();
        auto rcIt = s_renderCache.find(name);

        if (activeS == "Urchin" || activeS == "Both") {
          bool hasData = rcIt != s_renderCache.end() && rcIt->second.urchin && !rcIt->second.urchin->tags.empty();
          if (hasData) {
            std::string tS;
            for (auto &t : rcIt->second.urchin->tags) {
              if (!tS.empty()) tS += ", ";
              tS += t.type;
            }
            if (tS.length() > 25) tS = tS.substr(0, 22) + "...";
            g_guiFont.drawString(cx + 280.0f, cy + 9.0f, tS,
                                 applyAlpha(0xFFE0E0E0, alpha), 0.42f);
          } else
            g_guiFont.drawString(cx + 280.0f, cy + 9.0f, "-",
                                 applyAlpha(0xFF505055, alpha), 0.42f);
        } else
          g_guiFont.drawString(cx + 280.0f, cy + 9.0f, "Disabled",
                               applyAlpha(0xFF505055, alpha), 0.42f);

        if (activeS == "Seraph" || activeS == "Both") {
          bool hasData = rcIt != s_renderCache.end() && rcIt->second.seraph && !rcIt->second.seraph->tags.empty();
          if (hasData) {
            std::string tS;
            for (auto &t : rcIt->second.seraph->tags) {
              if (!tS.empty()) tS += ", ";
              tS += t.type;
            }
            if (tS.length() > 25) tS = tS.substr(0, 22) + "...";
            uint32_t sCol = 0xFFFF5555;
            if (tS.find("Confirmed") != std::string::npos)
              sCol = 0xFFFF55FF;
            g_guiFont.drawString(cx + 420.0f, cy + 9.0f, tS, applyAlpha(sCol, alpha), 0.42f);
          } else
            g_guiFont.drawString(cx + 420.0f, cy + 9.0f, "-",
                                 applyAlpha(0xFF505055, alpha), 0.42f);
        } else
          g_guiFont.drawString(cx + 420.0f, cy + 9.0f, "Disabled",
                               applyAlpha(0xFF505055, alpha), 0.42f);
      } else {
        g_guiFont.drawString(cx + 280.0f, cy + 9.0f, "Disabled",
                             applyAlpha(0xFF505055, alpha), 0.42f);
      }
      cy += rowH + 4.0f;
    }
  }
  */
}

} // namespace Tabs
} // namespace Render
