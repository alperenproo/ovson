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

  g_guiFont.drawString(cx, cy, "Tagging Services",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 40;

  bool tagsEnabled = Config::isTagsEnabled();
  bool hMasterCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hMasterCard, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy, "Enable Tags", applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18, "Master switch for all tagging services",
                       applyAlpha(0xFFA0A0A5, alpha));
  glDisable(GL_TEXTURE_2D);
  drawSwitch(10, mainX + g_w - 65, cy, tagsEnabled, hMasterCard, alpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hMasterCard) {
    Config::setTagsEnabled(!tagsEnabled);
    NotificationManager::getInstance()->add(
        "Tags", tagsEnabled ? "Tags Disabled" : "Tags Enabled",
        !tagsEnabled ? NotificationType::Success : NotificationType::Warning);
  }
  cy += 80;

  if (tagsEnabled) {
    g_guiFont.drawString(cx, cy, "Active Service",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 30;

    std::string currentService = Config::getActiveTagService();
    const char *services[] = {"Urchin", "Seraph", "Both", "Khadow"};
    constexpr int kServiceCount =
        (int)(sizeof(services) / sizeof(services[0]));

    float dropW = 220.0f;
    float dropH = 35.0f;
    bool hovDrop = isHovered(mx, my, cx, cy, dropW, dropH);

    s_tagsDropdownAnim += (s_isTagsDropdownOpen ? 1.0f - s_tagsDropdownAnim
                                                : 0.0f - s_tagsDropdownAnim) *
                          0.15f;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cx, cy, dropW, dropH, hovDrop, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx + 10, cy + 6, currentService,
                         applyAlpha(0xFFFFFFFF, alpha));
    drawChevron(cx + dropW - 16, cy + dropH * 0.5f, 4.0f,
                s_isTagsDropdownOpen, 0xFFA0A0A5, alpha);

    if (clickEvent && hovDrop) {
      s_isTagsDropdownOpen = !s_isTagsDropdownOpen;
    }

    if (s_tagsDropdownAnim > 0.01f) {
      float listY = cy + dropH + 2;
      for (int i = 0; i < kServiceCount; ++i) {
        float itemY = listY + (i * dropH);
        bool hItem = isHovered(mx, my, cx, itemY, dropW, dropH);

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(cx, itemY, dropW, dropH, hItem, alpha * s_tagsDropdownAnim);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(cx + 15, itemY + 8, services[i],
                             applyAlpha(currentService == services[i]
                                            ? 0xFFFFFFFF
                                            : 0xFFA0A0A5,
                                        alpha * s_tagsDropdownAnim));

        if (clickEvent && hItem && (s_tagsDropdownAnim > 0.8f)) {
          Config::setActiveTagService(services[i]);
          s_isTagsDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Tags", "Active service set to: " + std::string(services[i]),
              NotificationType::Info);
        }
      }
      cy += (kServiceCount * dropH) * s_tagsDropdownAnim;
    }
    cy += 50;

    g_guiFont.drawString(cx, cy, "Urchin API Key",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 20;
    glDisable(GL_TEXTURE_2D);
    drawTextInput(cx, cy, 350, 35, s_typingUrchinKey,
                  isHovered(mx, my, cx, cy, 350, 35), alpha);
    glEnable(GL_TEXTURE_2D);

    std::string dispUrchinKey =
        s_typingUrchinKey
            ? s_urchinKeyInput
            : (Config::getUrchinApiKey().empty() ? "None (Rate-limited)"
                                                 : "********************");
    if (s_typingUrchinKey && (GetTickCount64() / 500) % 2 == 0)
      dispUrchinKey += "|";
    g_guiFont.drawString(cx + 10, cy + 8, dispUrchinKey,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && isHovered(mx, my, cx, cy, 350, 35)) {
      s_typingUrchinKey = true;
      s_typingSeraphKey = s_typingSearch = s_typingApiKey = s_typingAutoGG =
          false;
      s_urchinKeyInput = Config::getUrchinApiKey();
    } else if (clickEvent && s_typingUrchinKey) {
      Config::setUrchinApiKey(s_urchinKeyInput);
      s_typingUrchinKey = false;
    }
    cy += 55;

    g_guiFont.drawString(cx, cy, "Seraph API Key",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 20;
    glDisable(GL_TEXTURE_2D);
    drawTextInput(cx, cy, 350, 35, s_typingSeraphKey,
                  isHovered(mx, my, cx, cy, 350, 35), alpha);
    glEnable(GL_TEXTURE_2D);

    std::string dispSeraphKey =
        s_typingSeraphKey
            ? s_seraphKeyInput
            : (Config::getSeraphApiKey().empty() ? "None"
                                                 : "********************");
    if (s_typingSeraphKey && (GetTickCount64() / 500) % 2 == 0)
      dispSeraphKey += "|";
    g_guiFont.drawString(cx + 10, cy + 8, dispSeraphKey,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && isHovered(mx, my, cx, cy, 350, 35)) {
      s_typingSeraphKey = true;
      s_typingUrchinKey = s_typingSearch = s_typingApiKey = s_typingAutoGG =
          false;
      NotificationManager::getInstance()->add("Input", "Seraph Key focused",
                                              NotificationType::Info);
    } else if (clickEvent) {
      if (s_typingSeraphKey) {
        Config::setSeraphApiKey(s_seraphKeyInput);
        NotificationManager::getInstance()->add("Seraph", "API Key Saved",
                                                NotificationType::Success);
      }
    }
    cy += 45;

  g_guiFont.drawString(cx, cy, "Muted Tag Chat Alerts", applyAlpha(0xFFFFFFFF, alpha));
  cy += 35;

  bool hMuteCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 85);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 85, hMuteCard, alpha);
  glEnable(GL_TEXTURE_2D);

  bool muteEnabled = Config::isMuteTagAlertsEnabled();
  g_guiFont.drawString(cx, cy, "Mute Tag Warnings", applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18, "Disable chat warnings/notifications for specified players",
                       applyAlpha(0xFFA0A0A5, alpha), 0.45f);

  glDisable(GL_TEXTURE_2D);
  drawSwitch(13, mainX + g_w - 65, cy, muteEnabled, hMuteCard && (my < cy + 30), alpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hMuteCard && (my < cy + 30)) {
    Config::setMuteTagAlertsEnabled(!muteEnabled);
  }

  bool muteSelfEnabled = Config::isMuteSelfTagAlertsEnabled();
  float muteSelfAlpha = alpha * (muteEnabled ? 1.0f : 0.4f);

  g_guiFont.drawString(cx + 10, cy + 45, "Mute Self Warnings", applyAlpha(0xFFFFFFFF, muteSelfAlpha), 0.42f);

  glDisable(GL_TEXTURE_2D);
  drawSwitch(144, mainX + g_w - 65, cy + 42, muteSelfEnabled, hMuteCard && (my >= cy + 30) && muteEnabled, muteSelfAlpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hMuteCard && (my >= cy + 30) && muteEnabled) {
    Config::setMuteSelfTagAlertsEnabled(!muteSelfEnabled);
  }
  cy += 90;

  float activeMuteAlpha = alpha * (muteEnabled ? 1.0f : 0.4f);
  g_guiFont.drawString(cx, cy, "Add Player to Mute List", applyAlpha(0xFFFFFFFF, activeMuteAlpha));
  cy += 20;

  glDisable(GL_TEXTURE_2D);
  drawTextInput(cx, cy, 250, 35, s_typingMuteTagPlayer,
                muteEnabled && isHovered(mx, my, cx, cy, 250, 35), activeMuteAlpha);
  glEnable(GL_TEXTURE_2D);

  std::string dispMuteInput = s_typingMuteTagPlayer ? s_muteTagPlayerInput : "Enter player name...";
  if (s_typingMuteTagPlayer && (GetTickCount64() / 500) % 2 == 0)
    dispMuteInput += "|";
  g_guiFont.drawString(cx + 10, cy + 8, dispMuteInput,
                       applyAlpha(s_typingMuteTagPlayer ? 0xFFFFFFFF : 0xFF808085, activeMuteAlpha));

  if (clickEvent && muteEnabled && isHovered(mx, my, cx, cy, 250, 35)) {
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
  cy += 50;

  const auto &mutedPlayers = Config::getMutedTagPlayers();
  if (!mutedPlayers.empty()) {
    g_guiFont.drawString(cx, cy, "Muted Players:", applyAlpha(0xFFA0A0A5, activeMuteAlpha));
    cy += 20;
    for (const auto &p : mutedPlayers) {
      bool hDel = muteEnabled && isHovered(mx, my, cx + 180, cy - 2, 20, 20);
      g_guiFont.drawString(cx, cy, p, applyAlpha(0xFFFFFFFF, activeMuteAlpha));
      
      float rx = cx + 180;
      float ry = cy - 2;
      float rw = 18.0f;
      float rh = 18.0f;
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
        break; // break loop because vector modified
      }
      cy += 25;
    }
    cy += 15;
  } else {
    cy += 10;
  }
  }

  g_guiFont.drawString(cx, cy, "Players in Current Game",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 35;

  std::lock_guard<std::mutex> stLock(OVson::g_statsMutex);
  if (OVson::g_playerStatsMap.empty()) {
    g_guiFont.drawString(cx, cy, "No players detected in this session.",
                         applyAlpha(0xFF808085, alpha));
    cy += 30;
  } else {
    g_guiFont.drawString(cx, cy, "Player", applyAlpha(0xFFA0A0A5, alpha));
    g_guiFont.drawString(cx + 140, cy, "FK", applyAlpha(0xFFA0A0A5, alpha));
    g_guiFont.drawString(cx + 200, cy, "FKDR", applyAlpha(0xFFA0A0A5, alpha));
    g_guiFont.drawString(cx + 280, cy, "Urchin",
                         applyAlpha(0xFFA0A0A5, alpha));
    g_guiFont.drawString(cx + 420, cy, "Seraph",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 25;

    for (const auto &pair : OVson::g_playerStatsMap) {
      const std::string &name = pair.first;
      const auto &stats = pair.second;

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
      g_guiFont.drawString(cx, cy, name, applyAlpha(nameCol, alpha));

      g_guiFont.drawString(cx + 140, cy,
                           std::to_string(stats.bedwarsFinalKills),
                           applyAlpha(0xFFCCCCCC, alpha));
      double fkdr =
          (stats.bedwarsFinalDeaths == 0)
              ? stats.bedwarsFinalKills
              : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
      char fBuf[16];
      sprintf_s(fBuf, "%.2f", fkdr);
      g_guiFont.drawString(cx + 200, cy, fBuf, applyAlpha(0xFFCCCCCC, alpha));

      if (Config::isTagsEnabled()) {
        std::string activeS = Config::getActiveTagService();

        if (activeS == "Urchin" || activeS == "Both") {
          auto uTagsRes = Urchin::getPlayerTags(name);
          if (uTagsRes && !uTagsRes->tags.empty()) {
            std::string tS;
            for (auto &t : uTagsRes->tags) {
              if (!tS.empty()) tS += ", ";
              tS += t.type;
            }
            if (tS.length() > 25) tS = tS.substr(0, 22) + "...";
            g_guiFont.drawString(cx + 280, cy, tS,
                                 applyAlpha(0xFFE0E0E0, alpha));
          } else
            g_guiFont.drawString(cx + 280, cy, "-",
                                 applyAlpha(0xFF505055, alpha));
        } else
          g_guiFont.drawString(cx + 280, cy, "Disabled",
                               applyAlpha(0xFF505055, alpha));

        if (activeS == "Seraph" || activeS == "Both") {
          auto sTagsRes = Seraph::getPlayerTags(name, stats.uuid);
          if (sTagsRes && !sTagsRes->tags.empty()) {
            std::string tS;
            for (auto &t : sTagsRes->tags) {
              if (!tS.empty()) tS += ", ";
              tS += t.type;
            }
            if (tS.length() > 25) tS = tS.substr(0, 22) + "...";
            uint32_t sCol = 0xFFFF5555;
            if (tS.find("Confirmed") != std::string::npos)
              sCol = 0xFFFF55FF;
            g_guiFont.drawString(cx + 420, cy, tS, applyAlpha(sCol, alpha));
          } else
            g_guiFont.drawString(cx + 420, cy, "-",
                                 applyAlpha(0xFF505055, alpha));
        } else
          g_guiFont.drawString(cx + 420, cy, "Disabled",
                               applyAlpha(0xFF505055, alpha));
      } else {
        g_guiFont.drawString(cx + 280, cy, "Disabled",
                             applyAlpha(0xFF505055, alpha));
      }
      cy += 35;
    }
  }
}

} // namespace Tabs
} // namespace Render
