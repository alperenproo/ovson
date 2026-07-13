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

  drawSectionLabel(cx, cy, "Configuration", alpha);
  cy += 38;

  // Theme is chosen from the sidebar-bottom segmented switcher now
  // (see Render.cpp). Only the LiquidGlass-specific sub-options remain
  // below, shown when Glass is selected.

  if (Config::getClickGuiTheme() == "LiquidGlass") {
    bool wiggleEnabled = Config::isLiquidGlassWiggleEnabled();
    bool hWiggleCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hWiggleCard, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Underwater Wiggle",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Enable animated wave distortion",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    drawSwitch(1234, mainX + g_w - 65, cy, wiggleEnabled, hWiggleCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hWiggleCard) {
      Config::setLiquidGlassWiggleEnabled(!wiggleEnabled);
      NotificationManager::getInstance()->add(
          "Settings",
          wiggleEnabled ? "Wiggle Disabled" : "Wiggle Enabled",
          !wiggleEnabled ? NotificationType::Success
                          : NotificationType::Warning);
    }
    cy += 85;

    bool glowEnabled = Config::isLiquidGlassGlowEnabled();
    bool hGlowCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hGlowCard, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Edge Reflections",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Enable glass edge lighting rim",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    drawSwitch(1235, mainX + g_w - 65, cy, glowEnabled, hGlowCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hGlowCard) {
      Config::setLiquidGlassGlowEnabled(!glowEnabled);
      NotificationManager::getInstance()->add(
          "Settings",
          glowEnabled ? "Glow Disabled" : "Glow Enabled",
          !glowEnabled ? NotificationType::Success
                          : NotificationType::Warning);
    }
    cy += 85;

    float refStr = Config::getLiquidGlassRefractStrength();
    bool hRefStr = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hRefStr, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Refraction Strength",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "How much the glass bends the background",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float oldRefStr = refStr;
    if (drawSlider(1236, mainX + g_w - 115, cy + 8, 100, 10, refStr, 0.0f, 1.0f, mx, my, lClick, alpha)) {
        if (oldRefStr != refStr) Config::setLiquidGlassRefractStrength(refStr);
    }
    glEnable(GL_TEXTURE_2D);
    cy += 85;

    float edgeWidth = Config::getLiquidGlassEdgeWidth();
    bool hEdgeWidth = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hEdgeWidth, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Main Edge Width",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Edge bending for the main panel",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float oldEdgeWidth = edgeWidth;
    if (drawSlider(1237, mainX + g_w - 115, cy + 8, 100, 10, edgeWidth, 0.0f, 1.0f, mx, my, lClick, alpha)) {
        if (oldEdgeWidth != edgeWidth) Config::setLiquidGlassEdgeWidth(edgeWidth);
    }
    glEnable(GL_TEXTURE_2D);
    cy += 85;

    float cardEdgeWidth = Config::getLiquidGlassCardEdgeWidth();
    bool hCardEdgeWidth = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hCardEdgeWidth, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Card Edge Width",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Edge bending for inner cards and buttons",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float oldCardEdgeWidth = cardEdgeWidth;
    if (drawSlider(1238, mainX + g_w - 115, cy + 8, 100, 10, cardEdgeWidth, 0.0f, 1.0f, mx, my, lClick, alpha)) {
        if (oldCardEdgeWidth != cardEdgeWidth) Config::setLiquidGlassCardEdgeWidth(cardEdgeWidth);
    }
    glEnable(GL_TEXTURE_2D);
    cy += 85;

    float darkness = Config::getLiquidGlassDarkness();
    bool hDarkness = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hDarkness, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "LiquidGlass: Background Opacity",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Darkness tint of the glass",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float oldDarkness = darkness;
    if (drawSlider(1239, mainX + g_w - 115, cy + 8, 100, 10, darkness, 0.0f, 1.0f, mx, my, lClick, alpha)) {
        if (oldDarkness != darkness) Config::setLiquidGlassDarkness(darkness);
    }
    glEnable(GL_TEXTURE_2D);
    cy += 85;
  }

  g_guiFont.drawString(cx, cy, "Hypixel API Key",
                       applyAlpha(0xFFA0A0A5, alpha));
  cy += 25;
  float keyW = g_w - 210;
  float keyX = mainX + 190;
  bool hKeyBox = isHovered(mx, my, keyX, cy, keyW, 35);
  (void)hKeyBox;

  glDisable(GL_TEXTURE_2D);
  drawTextInput(keyX, cy, keyW, 35, s_typingApiKey, hKeyBox, alpha);
  glEnable(GL_TEXTURE_2D);

  if (!s_typingApiKey)
    s_apiKeyInput = Config::getApiKey();

  std::string dispKey =
      s_typingApiKey
          ? s_apiKeyInput
          : (s_apiKeyInput.empty() ? "None" : "********************");
  if (s_typingApiKey && (GetTickCount64() / 500) % 2 == 0)
    dispKey += "|";

  g_guiFont.drawString(keyX + 10, cy + 12, dispKey,
                       applyAlpha(0xFFFFFFFF, alpha));

  if (clickEvent && isHovered(mx, my, keyX, cy, keyW, 35)) {
    s_typingApiKey = true;
    s_typingSearch = s_typingAutoGG = s_typingUrchinKey = false;
    NotificationManager::getInstance()->add("Input", "API Key focused",
                                            NotificationType::Info);
  } else if (clickEvent) {
    if (s_typingApiKey) {
      Config::setApiKey(s_apiKeyInput);
      NotificationManager::getInstance()->add("Settings", "API Key Saved",
                                              NotificationType::Success);
    }
    s_typingApiKey = false;
  }

  cy += 65;
  g_guiFont.drawString(cx, cy, "Aurora API Key",
                       applyAlpha(0xFFA0A0A5, alpha));
  cy += 25;
  bool hAuroraKey = isHovered(mx, my, keyX, cy, keyW, 35);
  glDisable(GL_TEXTURE_2D);
  drawTextInput(keyX, cy, keyW, 35, s_typingAuroraApiKey, hAuroraKey, alpha);
  glEnable(GL_TEXTURE_2D);

  if (!s_typingAuroraApiKey)
    s_auroraApiKeyInput = Config::getAuroraApiKey();
  std::string dispAurora =
      s_typingAuroraApiKey
          ? s_auroraApiKeyInput
          : (s_auroraApiKeyInput.empty() ? "None" : "********************");
  if (s_typingAuroraApiKey && (GetTickCount64() / 500) % 2 == 0)
    dispAurora += "|";
  g_guiFont.drawString(keyX + 10, cy + 12, dispAurora,
                       applyAlpha(0xFFFFFFFF, alpha));

  if (clickEvent && hAuroraKey) {
    s_typingAuroraApiKey = true;
    s_typingApiKey = s_typingSearch = s_typingAutoGG = s_typingUrchinKey =
        s_typingSeraphKey = false;
    NotificationManager::getInstance()->add("Input", "Aurora Key focused",
                                            NotificationType::Info);
  } else if (clickEvent && s_typingAuroraApiKey) {
    Config::setAuroraApiKey(s_auroraApiKeyInput);
    Config::save();
    NotificationManager::getInstance()->add("Settings", "Aurora Key Saved",
                                            NotificationType::Success);
    s_typingAuroraApiKey = false;
  }

  cy += 65;
  g_guiFont.drawString(cx, cy, "Ping History Mode",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 30;

  const char *pingModes[] = {"Current (Live)", "Aurora Latest",
                             "Aurora Average"};
  int currentPingMode = Config::getPingDisplayMode();

  float pDropW = 220.0f;
  float pDropH = 35.0f;
  bool hovPDrop = isHovered(mx, my, cx, cy, pDropW, pDropH);

  s_pingModeDropdownAnim +=
      (s_isPingModeDropdownOpen ? 1.0f - s_pingModeDropdownAnim
                                : 0.0f - s_pingModeDropdownAnim) *
      0.15f;

  glDisable(GL_TEXTURE_2D);
  drawThemeCard(cx, cy, pDropW, pDropH, hovPDrop, 0.8f * alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx + 10, cy + 6, pingModes[currentPingMode % 3],
                       applyAlpha(0xFFFFFFFF, alpha));
  drawChevron(cx + pDropW - 16, cy + pDropH * 0.5f, 4.0f,
              s_isPingModeDropdownOpen, 0xFFA0A0A5, alpha);

  if (clickEvent && hovPDrop)
    s_isPingModeDropdownOpen = !s_isPingModeDropdownOpen;

  if (s_pingModeDropdownAnim > 0.01f) {
    float listY = cy + pDropH + 2;
    for (int i = 0; i < 3; ++i) {
      float itemY = listY + (i * pDropH);
      bool hItem = isHovered(mx, my, cx, itemY, pDropW, pDropH);
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cx, itemY, pDropW, pDropH, hItem, 0.95f * alpha * s_pingModeDropdownAnim);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(
          cx + 15, itemY + 8, pingModes[i],
          applyAlpha(currentPingMode == i ? 0xFFFFFFFF : 0xFFA0A0A5,
                     alpha * s_pingModeDropdownAnim));
      if (clickEvent && hItem && (s_pingModeDropdownAnim > 0.8f)) {
        Config::setPingDisplayMode(i);
        s_isPingModeDropdownOpen = false;
        NotificationManager::getInstance()->add(
            "Settings", "Ping Mode: " + std::string(pingModes[i]),
            NotificationType::Info);
      }
    }
    cy += (3 * pDropH) * s_pingModeDropdownAnim;
  }

  cy += 65;
  bool keylessEnabled = Config::isKeylessModeEnabled();
  bool hKeylessCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hKeylessCard, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy, "API Keyless Mode",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18, "Fetch stats without an API key",
                       applyAlpha(0xFFA0A0A5, alpha));
  glDisable(GL_TEXTURE_2D);
  drawSwitch(11, mainX + g_w - 65, cy, keylessEnabled, hKeylessCard, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hKeylessCard) {
    Config::setKeylessModeEnabled(!keylessEnabled);
    NotificationManager::getInstance()->add(
        "Settings",
        keylessEnabled ? "Keyless Mode Disabled" : "Keyless Mode Enabled",
        !keylessEnabled ? NotificationType::Success
                        : NotificationType::Warning);
  }

  cy += 85;
  g_guiFont.drawString(cx, cy, "AutoGG Settings",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 35;
  bool aggEnabled = Config::isAutoGGEnabled();
  bool hAggCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hAggCard, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy, "AutoGG Module",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18,
                       "Automatically send a message when game ends",
                       applyAlpha(0xFFA0A0A5, alpha));
  glDisable(GL_TEXTURE_2D);
  float aggSwX = mainX + g_w - 65;
  drawSwitch(3, aggSwX, cy, aggEnabled, hAggCard, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hAggCard)
    Config::setAutoGGEnabled(!aggEnabled);

  cy += 85;
  g_guiFont.drawString(cx, cy, "Custom GG Message",
                       applyAlpha(0xFFA0A0A5, alpha));
  cy += 25;
  float ggW = g_w - 210;
  float ggX = mainX + 190;
  bool hGG = isHovered(mx, my, ggX, cy, ggW, 35);

  glDisable(GL_TEXTURE_2D);
  drawTextInput(ggX, cy, ggW, 35, s_typingAutoGG, hGG, alpha);
  glEnable(GL_TEXTURE_2D);

  if (s_autoGGInput.empty() && !s_typingAutoGG)
    s_autoGGInput = Config::getAutoGGMessage();
  std::string dispGG = s_autoGGInput;
  if (s_typingAutoGG && (GetTickCount64() / 500) % 2 == 0)
    dispGG += "|";
  if (dispGG.empty() && !s_typingAutoGG)
    dispGG = "Enter GG message...";

  g_guiFont.drawString(ggX + 10, cy + 4, dispGG,
                       applyAlpha(0xFFFFFFFF, alpha));
  if (clickEvent && hGG) {
    s_typingAutoGG = true;
    s_typingApiKey = s_typingSearch = s_typingUrchinKey = false;
    NotificationManager::getInstance()->add("Input", "AutoGG message focused",
                                            NotificationType::Info);
  } else if (clickEvent) {
    if (s_typingAutoGG) {
      Config::setAutoGGMessage(s_autoGGInput);
      NotificationManager::getInstance()->add(
          "AutoGG", "Custom message saved", NotificationType::Success);
    }
    s_typingAutoGG = false;
  }

  cy += 70;
  g_guiFont.drawString(cx, cy, "Command Settings",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 30;
  bool cmdEnabled = Config::isCommandsEnabled();
  bool hCmdCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hCmdCard, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy, "Command Interception",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18,
                       "Enable client commands starting with '.'",
                       applyAlpha(0xFFA0A0A5, alpha));
  glDisable(GL_TEXTURE_2D);
  drawSwitch(4, mainX + g_w - 65, cy, cmdEnabled, hCmdCard, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hCmdCard) {
    Config::setCommandsEnabled(!cmdEnabled);
    NotificationManager::getInstance()->add(
        "Settings", !cmdEnabled ? "Commands Enabled" : "Commands Disabled",
        !cmdEnabled ? NotificationType::Success : NotificationType::Warning);
  }
  cy += 75;

  g_guiFont.drawString(cx, cy, "Discord Rich Presence",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 30;
  bool discordEnabled = Config::isDiscordRpcEnabled();
  bool hDiscordCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hDiscordCard, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy, "Broadcasting Status",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18, "Show your activity on Discord",
                       applyAlpha(0xFFA0A0A5, alpha));
  glDisable(GL_TEXTURE_2D);
  float discordSwX = mainX + g_w - 65;
  drawSwitch(15, discordSwX, cy, discordEnabled, hDiscordCard, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hDiscordCard) {
    Config::setDiscordRpcEnabled(!discordEnabled);
    NotificationManager::getInstance()->add(
        "Discord",
        discordEnabled ? "Rich Presence Disabled" : "Rich Presence Enabled",
        !discordEnabled ? NotificationType::Success
                        : NotificationType::Warning);
  }

  cy += 70;
  float saveBtnW = 160.0f;
  bool hover = isHovered(mx, my, cx, cy, saveBtnW, 35);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, saveBtnW, 35, hover, false, alpha);
  glEnable(GL_TEXTURE_2D);

  std::string saveText = "SAVE CONFIG";
  float saveTextX =
      cx + (saveBtnW - g_guiFont.getStringWidth(saveText)) / 2.0f;
  g_guiFont.drawString(saveTextX, cy + 4, saveText,
                       applyAlpha(0xFFFFFFFF, alpha));
  if (clickEvent && hover) {
    Config::save();
    NotificationManager::getInstance()->add(
        "Cloud", "Settings synchronized successfully!",
        NotificationType::Success);
  }
  cy += 50;

  g_guiFont.drawString(cx, cy, "Menu Toggle Key",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 25;

  std::string keyText =
      s_waitingForKey
          ? "Press any key... (ESC to cancel)"
          : ("Current: " + ClickGUI::getKeyName(Config::getClickGuiKey()));
  if (s_waitingForKey && (GetTickCount64() / 300) % 2 == 0)
    keyText = "> " + keyText + " <";

  bool hBind = isHovered(mx, my, cx, cy, 250, 35);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, 250, 35, hBind, s_waitingForKey, alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(
      cx + 20, cy + 10, keyText,
      applyAlpha(s_waitingForKey ? 0xFFFFFFFF : 0xFFA0A0A5, alpha));

  if (clickEvent && hBind && !s_waitingForKey) {
    s_waitingForKey = true;
    s_typingApiKey = s_typingSearch = false;
  }
  cy += 70;

  g_guiFont.drawString(cx, cy, "Accent Color", applyAlpha(0xFFFFFFFF, alpha));
  cy += 25;
  const DWORD presets[] = {0xFF0055A4, 0xFFD32F2F, 0xFF388E3C, 0xFFFFC107,
                           0xFF8E24AA, 0xFF00ACC1, 0xFFFF5722};
  const char *presetNames[] = {"Navy", "Ruby", "Emerald", "Gold",
                               "Iris", "Cyan", "Flame"};
  int presetCount = sizeof(presets) / sizeof(presets[0]);
  DWORD currentTheme = Config::getThemeColor();

  float presetBoxSize = 35.0f;
  float presetGap = 10.0f;
  for (int i = 0; i < presetCount; ++i) {
    float px = cx + i * (presetBoxSize + presetGap);
    bool selected = (currentTheme == presets[i]);
    bool hPre = isHovered(mx, my, px, cy, presetBoxSize, presetBoxSize);

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(px, cy, presetBoxSize, presetBoxSize, 4.0f,
                                 presets[i], alpha);
    if (selected)
      RenderUtils::drawRoundedRect(px, cy + presetBoxSize - 3, presetBoxSize,
                                   4.0f, 2.0f, 0xFFFFFFFF, alpha);
    if (hPre && !selected)
      RenderUtils::drawRoundedRect(px, cy, presetBoxSize, 2, 2.0f, 0xFFFFFFFF,
                                   alpha * 0.6f);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hPre) {
      Config::setThemeColor(presets[i]);
      NotificationManager::getInstance()->add(
          "Theme", "Accent set to " + std::string(presetNames[i]),
          NotificationType::Info);
    }
  }
  cy += 60;

  cy += 50;
  cy += 50;
}

} // namespace Tabs
} // namespace Render
