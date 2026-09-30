#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../ClickGUI.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include <Windows.h>
#include <cstdint>
#include <gl/GL.h>
#include <string>

namespace Render { class ClickGUI; }

namespace Render {
namespace Tabs {

void renderSettings(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  lClick = ctx.lClick;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  const float cardX = mainX + 190.0f;
  const float cardW = g_w - 210.0f;
  const float swX   = mainX + g_w - 65.0f;

  if (s_moduleSearch.empty()) {
    drawSectionLabel(cx, cy, "API Credentials & Network", alpha);
    cy += 30.0f;
  }

  if (shouldShowInSearch("Hypixel API Key", "Hypixel API key token credentials stats player lookups")) {
    float cardH = 96.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    float boxX = cardX + 16.0f;
    float boxY = cy + 50.0f;
    float boxW = cardW - 32.0f;
    float boxH = 32.0f;
    bool hBox = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Hypixel API Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Official token for player statistics, winstreaks & party lookups", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    glDisable(GL_TEXTURE_2D);
    drawTextInput(boxX, boxY, boxW, boxH, s_typingApiKey, hBox, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingApiKey)
      s_apiKeyInput = Config::getApiKey();

    std::string dispKey;
    DWORD textCol = 0xFFFFFFFF;
    if (s_typingApiKey) {
      dispKey = s_apiKeyInput;
      if ((GetTickCount64() / 500) % 2 == 0)
        dispKey += "|";
    } else {
      if (s_apiKeyInput.empty()) {
        dispKey = "Click to enter Hypixel API Key...";
        textCol = 0xFF6C6C78;
      } else {
        dispKey = std::string(s_apiKeyInput.length(), '*');
      }
    }
    g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispKey, applyAlpha(textCol, alpha), 0.42f);

    if (clickEvent && hBox) {
      s_typingApiKey = true;
      s_typingSearch = s_typingModuleSearch = s_typingAutoGG = s_typingUrchinKey =
          s_typingSeraphKey = s_typingAuroraApiKey = s_typingPrefix = false;
      NotificationManager::getInstance()->add("Input", "Hypixel API Key focused",
                                              NotificationType::Info);
    } else if (clickEvent && s_typingApiKey) {
      Config::setApiKey(s_apiKeyInput);
      Config::save();
      NotificationManager::getInstance()->add("Settings", "Hypixel API Key Saved",
                                              NotificationType::Success);
      s_typingApiKey = false;
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Aurora API Key", "Aurora API key token credentials tags history lookups")) {
    float cardH = 96.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    float boxX = cardX + 16.0f;
    float boxY = cy + 50.0f;
    float boxW = cardW - 32.0f;
    float boxH = 32.0f;
    bool hBox = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Aurora API Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Aurora database token for UUID caching, player tags & history", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    glDisable(GL_TEXTURE_2D);
    drawTextInput(boxX, boxY, boxW, boxH, s_typingAuroraApiKey, hBox, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingAuroraApiKey)
      s_auroraApiKeyInput = Config::getAuroraApiKey();

    std::string dispAurora;
    DWORD textCol = 0xFFFFFFFF;
    if (s_typingAuroraApiKey) {
      dispAurora = s_auroraApiKeyInput;
      if ((GetTickCount64() / 500) % 2 == 0)
        dispAurora += "|";
    } else {
      if (s_auroraApiKeyInput.empty()) {
        dispAurora = "Click to enter Aurora API Key...";
        textCol = 0xFF6C6C78;
      } else {
        dispAurora = std::string(s_auroraApiKeyInput.length(), '*');
      }
    }
    g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispAurora, applyAlpha(textCol, alpha), 0.42f);

    if (clickEvent && hBox) {
      s_typingAuroraApiKey = true;
      s_typingApiKey = s_typingSearch = s_typingModuleSearch = s_typingAutoGG =
          s_typingUrchinKey = s_typingSeraphKey = s_typingPrefix = false;
      NotificationManager::getInstance()->add("Input", "Aurora Key focused",
                                              NotificationType::Info);
    } else if (clickEvent && s_typingAuroraApiKey) {
      Config::setAuroraApiKey(s_auroraApiKeyInput);
      Config::save();
      NotificationManager::getInstance()->add("Settings", "Aurora Key Saved",
                                              NotificationType::Success);
      s_typingAuroraApiKey = false;
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Urchin API Key", "Urchin API key token tagging service")) {
    float cardH = 96.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    float boxX = cardX + 16.0f;
    float boxY = cy + 50.0f;
    float boxW = cardW - 32.0f;
    float boxH = 32.0f;
    bool hBox = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Urchin API Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Urchin tagging service token for player tag lookups", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    glDisable(GL_TEXTURE_2D);
    drawTextInput(boxX, boxY, boxW, boxH, s_typingUrchinKey, hBox, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingUrchinKey)
      s_urchinKeyInput = Config::getUrchinApiKey();

    std::string dispUrchin;
    DWORD textCol = 0xFFFFFFFF;
    if (s_typingUrchinKey) {
      dispUrchin = s_urchinKeyInput;
      if ((GetTickCount64() / 500) % 2 == 0)
        dispUrchin += "|";
    } else {
      if (s_urchinKeyInput.empty()) {
        dispUrchin = "Click to enter Urchin API Key...";
        textCol = 0xFF6C6C78;
      } else {
        dispUrchin = std::string(s_urchinKeyInput.length(), '*');
      }
    }
    g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispUrchin, applyAlpha(textCol, alpha), 0.42f);

    if (clickEvent && hBox) {
      s_typingUrchinKey = true;
      s_typingApiKey = s_typingSearch = s_typingModuleSearch = s_typingAutoGG =
          s_typingSeraphKey = s_typingAuroraApiKey = s_typingPrefix = false;
      s_urchinKeyInput = Config::getUrchinApiKey();
    } else if (clickEvent && s_typingUrchinKey) {
      Config::setUrchinApiKey(s_urchinKeyInput);
      Config::save();
      NotificationManager::getInstance()->add("Settings", "Urchin API Key Saved",
                                              NotificationType::Success);
      s_typingUrchinKey = false;
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Seraph API Key", "Seraph API key token tagging service")) {
    float cardH = 96.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    float boxX = cardX + 16.0f;
    float boxY = cy + 50.0f;
    float boxW = cardW - 32.0f;
    float boxH = 32.0f;
    bool hBox = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Seraph API Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Seraph tagging service token for player tag lookups", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    glDisable(GL_TEXTURE_2D);
    drawTextInput(boxX, boxY, boxW, boxH, s_typingSeraphKey, hBox, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingSeraphKey)
      s_seraphKeyInput = Config::getSeraphApiKey();

    std::string dispSeraph;
    DWORD textCol = 0xFFFFFFFF;
    if (s_typingSeraphKey) {
      dispSeraph = s_seraphKeyInput;
      if ((GetTickCount64() / 500) % 2 == 0)
        dispSeraph += "|";
    } else {
      if (s_seraphKeyInput.empty()) {
        dispSeraph = "Click to enter Seraph API Key...";
        textCol = 0xFF6C6C78;
      } else {
        dispSeraph = std::string(s_seraphKeyInput.length(), '*');
      }
    }
    g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispSeraph, applyAlpha(textCol, alpha), 0.42f);

    if (clickEvent && hBox) {
      s_typingSeraphKey = true;
      s_typingApiKey = s_typingSearch = s_typingModuleSearch = s_typingAutoGG =
          s_typingUrchinKey = s_typingAuroraApiKey = s_typingPrefix = false;
      NotificationManager::getInstance()->add("Input", "Seraph Key focused",
                                              NotificationType::Info);
    } else if (clickEvent && s_typingSeraphKey) {
      Config::setSeraphApiKey(s_seraphKeyInput);
      Config::save();
      NotificationManager::getInstance()->add("Settings", "Seraph API Key Saved",
                                              NotificationType::Success);
      s_typingSeraphKey = false;
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("API Keyless Mode", "Fetch stats without an API key")) {
    float cardH = 64.0f;
    bool keylessEnabled = Config::isKeylessModeEnabled();
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawSwitch(11, swX, cy + 19.0f, keylessEnabled, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "API Keyless Mode", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Fetch stats without an API key", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (clickEvent && hCard) {
      Config::setKeylessModeEnabled(!keylessEnabled);
      NotificationManager::getInstance()->add(
          "Settings",
          keylessEnabled ? "Keyless Mode Disabled" : "Keyless Mode Enabled",
          !keylessEnabled ? NotificationType::Success : NotificationType::Warning);
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Ping History Mode", "Current Live Aurora Latest Average ping latency")) {
    const char *pingModes[] = {"Current (Live)", "Aurora Latest", "Aurora Average"};
    int currentPingMode = Config::getPingDisplayMode();

    float pDropW = 180.0f;
    float pDropH = 32.0f;
    float pDropX = cardX + cardW - pDropW - 16.0f;

    float dropdownContentH = 0.0f;
    s_pingModeDropdownAnim +=
        (s_isPingModeDropdownOpen ? 1.0f - s_pingModeDropdownAnim
                                  : 0.0f - s_pingModeDropdownAnim) *
        0.15f;
    if (s_pingModeDropdownAnim > 0.01f)
      dropdownContentH = (3 * (pDropH + 2.0f) + 8.0f) * s_pingModeDropdownAnim;

    float cardH = 64.0f + dropdownContentH;
    float pDropY = cy + 16.0f;
    bool hovPDrop = isHovered(mx, my, pDropX, pDropY, pDropW, pDropH);
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawThemeCard(pDropX, pDropY, pDropW, pDropH, hovPDrop, 0.9f * alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Ping History Mode (Aurora)", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Source used for calculating player ping in tab list & tags", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    g_guiFont.drawString(pDropX + 12.0f, pDropY + 7.0f, pingModes[currentPingMode % 3],
                         applyAlpha(0xFFFFFFFF, alpha), 0.40f);
    drawChevron(pDropX + pDropW - 16.0f, pDropY + pDropH * 0.5f, 4.0f,
                s_isPingModeDropdownOpen, 0xFFA0A0A5, alpha);

    if (clickEvent && hovPDrop)
      s_isPingModeDropdownOpen = !s_isPingModeDropdownOpen;

    if (s_pingModeDropdownAnim > 0.01f) {
      float listY = pDropY + pDropH + 4.0f;
      for (int i = 0; i < 3; ++i) {
        float itemY = listY + (i * (pDropH + 2.0f));
        bool hItem = isHovered(mx, my, pDropX, itemY, pDropW, pDropH);
        glDisable(GL_TEXTURE_2D);
        drawThemeCard(pDropX, itemY, pDropW, pDropH, hItem, 0.95f * alpha * s_pingModeDropdownAnim);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(
            pDropX + 14.0f, itemY + 7.0f, pingModes[i],
            applyAlpha(currentPingMode == i ? 0xFFFFFFFF : 0xFFA0A0A5,
                       alpha * s_pingModeDropdownAnim), 0.40f);
        if (clickEvent && hItem && (s_pingModeDropdownAnim > 0.8f)) {
          Config::setPingDisplayMode(i);
          s_isPingModeDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Settings", "Ping Mode: " + std::string(pingModes[i]),
              NotificationType::Info);
        }
      }
    }

    cy += cardH + 12.0f;
  }

  if (s_moduleSearch.empty()) {
    cy += 6.0f;
    drawSectionLabel(cx, cy, "Automation & Commands", alpha);
    cy += 30.0f;
  }

  if (shouldShowInSearch("AutoGG", "Automatically send a message when game ends custom message gg")) {
    bool aggEnabled = Config::isAutoGGEnabled();
    float cardH = aggEnabled ? 116.0f : 64.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    bool hTopRow = isHovered(mx, my, cardX, cy, cardW, 50.0f);

    float boxX = cardX + 16.0f;
    float boxY = cy + 68.0f;
    float boxW = cardW - 32.0f;
    float boxH = 32.0f;
    bool hGG = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawSwitch(3, swX, cy + 19.0f, aggEnabled, hTopRow, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "AutoGG Module", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Automatically send a custom victory message when a game finishes", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (aggEnabled) {
      g_guiFont.drawString(cx, cy + 50.0f, "Custom GG Message", applyAlpha(0xFFA0A0A5, alpha), 0.36f);

      glDisable(GL_TEXTURE_2D);
      drawTextInput(boxX, boxY, boxW, boxH, s_typingAutoGG, hGG, alpha);
      glEnable(GL_TEXTURE_2D);

      if (s_autoGGInput.empty() && !s_typingAutoGG)
        s_autoGGInput = Config::getAutoGGMessage();

      std::string dispGG = s_autoGGInput;
      DWORD textCol = 0xFFFFFFFF;
      if (s_typingAutoGG) {
        if ((GetTickCount64() / 500) % 2 == 0)
          dispGG += "|";
      } else if (dispGG.empty()) {
        dispGG = "Enter victory message (e.g. 'gg', 'good game')...";
        textCol = 0xFF6C6C78;
      }

      g_guiFont.drawString(boxX + 12.0f, boxY + 8.0f, dispGG, applyAlpha(textCol, alpha), 0.42f);
    }

    if (clickEvent) {
      if (aggEnabled && hGG) {
        s_typingAutoGG = true;
        s_typingApiKey = s_typingSearch = s_typingModuleSearch = s_typingUrchinKey =
            s_typingSeraphKey = s_typingAuroraApiKey = s_typingPrefix = false;
        NotificationManager::getInstance()->add("Input", "AutoGG message focused",
                                                NotificationType::Info);
      } else {
        if (s_typingAutoGG) {
          Config::setAutoGGMessage(s_autoGGInput);
          Config::save();
          NotificationManager::getInstance()->add("AutoGG", "Custom message saved",
                                                  NotificationType::Success);
          s_typingAutoGG = false;
        }
        if (hTopRow) {
          Config::setAutoGGEnabled(!aggEnabled);
          NotificationManager::getInstance()->add(
              "AutoGG", !aggEnabled ? "AutoGG Enabled" : "AutoGG Disabled",
              !aggEnabled ? NotificationType::Success : NotificationType::Warning);
        }
      }
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Commands", "Command Interception prefix client commands chat")) {
    bool cmdEnabled = Config::isCommandsEnabled();
    float cardH = cmdEnabled ? 116.0f : 64.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    bool hTopRow = isHovered(mx, my, cardX, cy, cardW, 50.0f);

    float boxX = cardX + 16.0f;
    float boxY = cy + 68.0f;
    float boxW = 75.0f;
    float boxH = 32.0f;
    bool hPrefix = isHovered(mx, my, boxX, boxY, boxW, boxH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawSwitch(4, swX, cy + 19.0f, cmdEnabled, hTopRow, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Command Interception", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Execute client commands directly from in-game chat (.help, .stats, etc.)", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (cmdEnabled) {
      g_guiFont.drawString(cx, cy + 50.0f, "Command Prefix", applyAlpha(0xFFA0A0A5, alpha), 0.36f);

      glDisable(GL_TEXTURE_2D);
      drawTextInput(boxX, boxY, boxW, boxH, s_typingPrefix, hPrefix, alpha);
      glEnable(GL_TEXTURE_2D);

      std::string dispPrefix = s_typingPrefix ? s_prefixInput : Config::getCommandPrefix();
      if (s_typingPrefix && (GetTickCount64() / 500) % 2 == 0)
        dispPrefix += "|";

      float pfxW = g_guiFont.getStringWidth(dispPrefix) * 0.44f;
      g_guiFont.drawString(boxX + (boxW - pfxW) * 0.5f, boxY + 8.0f, dispPrefix,
                           applyAlpha(0xFFFFFFFF, alpha), 0.44f);

      g_guiFont.drawString(boxX + boxW + 16.0f, boxY + 8.0f,
                           "Trigger character for chat commands (default: '.')",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    }

    if (clickEvent) {
      if (cmdEnabled && hPrefix) {
        s_typingPrefix = true;
        s_typingSeraphKey = s_typingUrchinKey = s_typingSearch = s_typingModuleSearch =
            s_typingApiKey = s_typingAutoGG = s_typingAuroraApiKey =
            s_waitingForKey = s_waitingForUninjectKey = false;
        s_prefixInput = Config::getCommandPrefix();
      } else {
        if (s_typingPrefix) {
          Config::setCommandPrefix(s_prefixInput);
          Config::save();
          NotificationManager::getInstance()->add("Commands", "Command prefix saved",
                                                  NotificationType::Success);
          s_typingPrefix = false;
        }
        if (hTopRow) {
          Config::setCommandsEnabled(!cmdEnabled);
          NotificationManager::getInstance()->add(
              "Settings", !cmdEnabled ? "Commands Enabled" : "Commands Disabled",
              !cmdEnabled ? NotificationType::Success : NotificationType::Warning);
        }
      }
    }

    cy += cardH + 12.0f;
  }

  if (s_moduleSearch.empty()) {
    cy += 6.0f;
    drawSectionLabel(cx, cy, "Keybinds & Controls", alpha);
    cy += 30.0f;
  }

  if (shouldShowInSearch("Menu Toggle Key", "Keybind to open close GUI menu keyboard key")) {
    float cardH = 64.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    float btnW = 160.0f;
    float btnH = 32.0f;
    float btnX = cardX + cardW - btnW - 16.0f;
    float btnY = cy + 16.0f;
    bool hBind = isHovered(mx, my, btnX, btnY, btnW, btnH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawThemeButton(btnX, btnY, btnW, btnH, hBind, s_waitingForKey, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Menu Toggle Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Keyboard key to toggle ClickGUI interface open/close", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    std::string keyText = s_waitingForKey
        ? "> Press Key <"
        : ("[ " + ClickGUI::getKeyName(Config::getClickGuiKey()) + " ]");
    float kw = g_guiFont.getStringWidth(keyText) * (0.40f / 0.5f);
    g_guiFont.drawString(btnX + (btnW - kw) * 0.5f, btnY + (btnH - 12.0f) * 0.5f, keyText,
                         applyAlpha(s_waitingForKey ? 0xFFFFFFFF : 0xFFA0A0A5, alpha), 0.40f);

    if (clickEvent && hBind && !s_waitingForKey) {
      s_waitingForKey = true;
      s_waitingForUninjectKey = false;
      s_typingApiKey = s_typingSearch = s_typingModuleSearch = false;
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Uninject Key", "Uninject overlay keyboard key unload eject")) {
    bool uninjectEnabled = Config::isUninjectKeyEnabled();
    float cardH = uninjectEnabled ? 116.0f : 64.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);
    bool hTopRow = isHovered(mx, my, cardX, cy, cardW, 50.0f);

    float btnW = 160.0f;
    float btnH = 32.0f;
    float btnX = cardX + 16.0f;
    float btnY = cy + 68.0f;
    bool hUninjectBind = uninjectEnabled && isHovered(mx, my, btnX, btnY, btnW, btnH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    drawSwitch(12, swX, cy + 19.0f, uninjectEnabled, hTopRow, alpha);
    if (uninjectEnabled) {
      drawThemeButton(btnX, btnY, btnW, btnH, hUninjectBind, s_waitingForUninjectKey, alpha);
    }
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Uninject Key", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Unhook and unload the overlay from memory", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (uninjectEnabled) {
      g_guiFont.drawString(cx, cy + 50.0f, "Assigned Hotkey", applyAlpha(0xFFA0A0A5, alpha), 0.36f);

      std::string uninjectKeyText = s_waitingForUninjectKey
          ? "> Press Key <"
          : ("[ " + ClickGUI::getKeyName(Config::getUninjectKey()) + " ]");
      float ukw = g_guiFont.getStringWidth(uninjectKeyText) * (0.40f / 0.5f);
      g_guiFont.drawString(btnX + (btnW - ukw) * 0.5f, btnY + (btnH - 12.0f) * 0.5f, uninjectKeyText,
                           applyAlpha(s_waitingForUninjectKey ? 0xFFFFFFFF : 0xFFA0A0A5, alpha), 0.40f);
    }

    if (clickEvent) {
      if (uninjectEnabled && hUninjectBind && !s_waitingForUninjectKey) {
        s_waitingForUninjectKey = true;
        s_waitingForKey = false;
        s_typingApiKey = s_typingSearch = s_typingModuleSearch = false;
      } else if (hTopRow && !hUninjectBind) {
        Config::setUninjectKeyEnabled(!uninjectEnabled);
        NotificationManager::getInstance()->add(
            "Settings", !uninjectEnabled ? "Uninject Key Enabled" : "Uninject Key Disabled",
            !uninjectEnabled ? NotificationType::Success : NotificationType::Warning);
      }
    }

    cy += cardH + 12.0f;
  }

  if (s_moduleSearch.empty()) {
    cy += 6.0f;
    drawSectionLabel(cx, cy, "Theme & Appearance", alpha);
    cy += 30.0f;
  }

  /*
  if (shouldShowInSearch("Accent Color", "Theme color preset Navy Ruby Emerald Gold Iris Cyan Flame")) {
    float cardH = 98.0f;
    bool hCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    const DWORD presets[] = {0xFF0055A4, 0xFFD32F2F, 0xFF388E3C, 0xFFFFC107,
                             0xFF8E24AA, 0xFF00ACC1, 0xFFFF5722};
    const char *presetNames[] = {"Navy", "Ruby", "Emerald", "Gold",
                                 "Iris", "Cyan", "Flame"};
    int presetCount = sizeof(presets) / sizeof(presets[0]);
    DWORD currentTheme = Config::getThemeColor();

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Accent Color Theme", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Primary highlight tint for switches, sliders, and active tabs", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    const char *activeThemeName = "Custom";
    for (int i = 0; i < presetCount; ++i) {
      if (currentTheme == presets[i]) {
        activeThemeName = presetNames[i];
        break;
      }
    }
    std::string themeTag = std::string("[ ") + activeThemeName + " ]";
    float tw = g_guiFont.getStringWidth(themeTag) * 0.38f;
    g_guiFont.drawString(cardX + cardW - 16.0f - tw, cy + 12.0f, themeTag,
                         applyAlpha(ClickGUITheme::accent(), alpha), 0.38f);

    float presetBoxSize = 34.0f;
    float presetGap = 12.0f;
    float rowY = cy + 50.0f;
    float startX = cardX + 16.0f;

    for (int i = 0; i < presetCount; ++i) {
      float px = startX + i * (presetBoxSize + presetGap);
      bool selected = (currentTheme == presets[i]);
      bool hPre = isHovered(mx, my, px, rowY, presetBoxSize, presetBoxSize);

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(px, rowY, presetBoxSize, presetBoxSize, 6.0f, presets[i], alpha);
      if (selected) {
        RenderUtils::drawRoundedOutline(px - 3.0f, rowY - 3.0f, presetBoxSize + 6.0f,
                                        presetBoxSize + 6.0f, 8.0f, 2.0f, 0xFFFFFFFF, alpha);
        RenderUtils::drawCircle(px + presetBoxSize * 0.5f, rowY + presetBoxSize * 0.5f,
                                3.5f, 0xFFFFFFFF, alpha);
      } else if (hPre) {
        RenderUtils::drawRoundedOutline(px - 2.0f, rowY - 2.0f, presetBoxSize + 4.0f,
                                        presetBoxSize + 4.0f, 7.0f, 1.5f, 0x88FFFFFF, alpha);
      }
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hPre) {
        Config::setThemeColor(presets[i]);
        Config::save();
        NotificationManager::getInstance()->add(
            "Theme", "Accent set to " + std::string(presetNames[i]),
            NotificationType::Info);
      }
    }

    cy += cardH + 12.0f;
  }
  */

  if (Config::getClickGuiTheme() == "LiquidGlass") {
    if (s_moduleSearch.empty()) {
      cy += 6.0f;
      drawSectionLabel(cx, cy, "LiquidGlass Shader Settings", alpha);
      cy += 30.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Underwater Wiggle", "Underwater Wiggle Edge Reflections Refraction")) {
      float cardH = 64.0f;
      bool wiggleEnabled = Config::isLiquidGlassWiggleEnabled();
      bool hWiggleCard = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hWiggleCard, alpha);
      drawSwitch(1234, swX, cy + 19.0f, wiggleEnabled, hWiggleCard, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Underwater Wiggle",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Enable animated wave distortion on glass surfaces",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      if (clickEvent && hWiggleCard) {
        Config::setLiquidGlassWiggleEnabled(!wiggleEnabled);
        NotificationManager::getInstance()->add(
            "Settings",
            wiggleEnabled ? "Wiggle Disabled" : "Wiggle Enabled",
            !wiggleEnabled ? NotificationType::Success : NotificationType::Warning);
      }
      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Edge Reflections", "Edge Reflections glass rim lighting")) {
      float cardH = 64.0f;
      bool glowEnabled = Config::isLiquidGlassGlowEnabled();
      bool hGlowCard = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hGlowCard, alpha);
      drawSwitch(1235, swX, cy + 19.0f, glowEnabled, hGlowCard, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Edge Reflections",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Enable glass edge lighting rim and reflection accents",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      if (clickEvent && hGlowCard) {
        Config::setLiquidGlassGlowEnabled(!glowEnabled);
        NotificationManager::getInstance()->add(
            "Settings",
            glowEnabled ? "Glow Disabled" : "Glow Enabled",
            !glowEnabled ? NotificationType::Success : NotificationType::Warning);
      }
      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Refraction Strength", "Refraction Strength bend background")) {
      float cardH = 64.0f;
      float refStr = Config::getLiquidGlassRefractStrength();
      bool hRefStr = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hRefStr, alpha);
      bool refChanged = drawSlider(1236, mainX + g_w - 220, cy + 27.0f, 90, 10,
                                   refStr, 0.0f, 1.0f, mx, my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Refraction Strength",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "How intensely the glass distortion bends the background",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      refChanged = drawNumericInput(1236, mainX + g_w - 120, cy + 18.0f, 90, 27,
                                    refStr, 0.0f, 1.0f, 2, "", mx, my,
                                    clickEvent, alpha) || refChanged;
      if (refChanged) Config::setLiquidGlassRefractStrength(refStr);
      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Main Edge Width", "Edge bending for main panel")) {
      float cardH = 64.0f;
      float edgeWidth = Config::getLiquidGlassEdgeWidth();
      bool hEdgeWidth = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hEdgeWidth, alpha);
      bool edgeChanged = drawSlider(1237, mainX + g_w - 220, cy + 27.0f, 90, 10,
                                    edgeWidth, 0.0f, 1.0f, mx, my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Main Edge Width",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Edge refraction curvature width for the main panel",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      edgeChanged = drawNumericInput(1237, mainX + g_w - 120, cy + 18.0f, 90, 27,
                                     edgeWidth, 0.0f, 1.0f, 2, "", mx, my,
                                     clickEvent, alpha) || edgeChanged;
      if (edgeChanged) Config::setLiquidGlassEdgeWidth(edgeWidth);
      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Card Edge Width", "Edge bending for inner cards and buttons")) {
      float cardH = 64.0f;
      float cardEdgeWidth = Config::getLiquidGlassCardEdgeWidth();
      bool hCardEdgeWidth = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hCardEdgeWidth, alpha);
      bool cardEdgeChanged =
          drawSlider(1238, mainX + g_w - 220, cy + 27.0f, 90, 10, cardEdgeWidth,
                     0.0f, 1.0f, mx, my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Card Edge Width",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Edge curvature width for inner cards and interactive controls",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      cardEdgeChanged =
          drawNumericInput(1238, mainX + g_w - 120, cy + 18.0f, 90, 27,
                           cardEdgeWidth, 0.0f, 1.0f, 2, "", mx, my,
                           clickEvent, alpha) || cardEdgeChanged;
      if (cardEdgeChanged) Config::setLiquidGlassCardEdgeWidth(cardEdgeWidth);
      cy += cardH + 12.0f;
    }

    if (shouldShowInSearch("LiquidGlass: Background Opacity", "Darkness tint of glass background")) {
      float cardH = 64.0f;
      float darkness = Config::getLiquidGlassDarkness();
      bool hDarkness = isHovered(mx, my, cardX, cy, cardW, cardH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, cardH, hDarkness, alpha);
      bool darknessChanged =
          drawSlider(1239, mainX + g_w - 220, cy + 27.0f, 90, 10, darkness, 0.0f,
                     1.0f, mx, my, lClick, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy + 12.0f, "LiquidGlass: Background Opacity",
                           applyAlpha(0xFFFFFFFF, alpha), 0.46f);
      g_guiFont.drawString(cx, cy + 30.0f, "Darkness tint intensity of the liquid glass background",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      darknessChanged =
          drawNumericInput(1239, mainX + g_w - 120, cy + 18.0f, 90, 27, darkness,
                           0.0f, 1.0f, 2, "", mx, my, clickEvent, alpha) ||
          darknessChanged;
      if (darknessChanged) Config::setLiquidGlassDarkness(darkness);
      cy += cardH + 12.0f;
    }
  }

  if (shouldShowInSearch("Discord Rich Presence", "Show your activity on Discord rpc broadcasting status")) {
    float cardH = 64.0f;
    bool discordEnabled = Config::isDiscordRpcEnabled();
    bool hDiscordCard = isHovered(mx, my, cardX, cy, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cardH, hDiscordCard, alpha);
    drawSwitch(15, swX, cy + 19.0f, discordEnabled, hDiscordCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 12.0f, "Discord Rich Presence", applyAlpha(0xFFFFFFFF, alpha), 0.46f);
    g_guiFont.drawString(cx, cy + 30.0f, "Broadcast live gameplay status and statistics to your Discord profile", applyAlpha(0xFFA0A0A5, alpha), 0.38f);

    if (clickEvent && hDiscordCard) {
      Config::setDiscordRpcEnabled(!discordEnabled);
      NotificationManager::getInstance()->add(
          "Discord",
          discordEnabled ? "Rich Presence Disabled" : "Rich Presence Enabled",
          !discordEnabled ? NotificationType::Success : NotificationType::Warning);
    }

    cy += cardH + 12.0f;
  }

  if (shouldShowInSearch("Save Config", "Write settings configuration profile to disk cloud sync")) {
    if (s_moduleSearch.empty()) {
      cy += 6.0f;
    }

    float saveBtnW = 180.0f;
    float saveBtnH = 38.0f;
    float saveBtnX = cardX + (cardW - saveBtnW) * 0.5f;
    float saveBtnY = cy;
    bool hover = isHovered(mx, my, saveBtnX, saveBtnY, saveBtnW, saveBtnH);

    glDisable(GL_TEXTURE_2D);
    drawThemeButton(saveBtnX, saveBtnY, saveBtnW, saveBtnH, hover, false, alpha);
    glEnable(GL_TEXTURE_2D);

    std::string saveText = "SAVE CONFIG";
    float tw = g_guiFont.getStringWidth(saveText) * (0.42f / 0.5f);
    g_guiFont.drawString(saveBtnX + (saveBtnW - tw) * 0.5f, saveBtnY + (saveBtnH - 12.0f) * 0.5f, saveText,
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);

    if (clickEvent && hover) {
      Config::save();
      NotificationManager::getInstance()->add(
          "Config", "Settings synchronized successfully!",
          NotificationType::Success);
    }

    cy += saveBtnH + 20.0f;
  }
}

} // namespace Tabs
} // namespace Render
