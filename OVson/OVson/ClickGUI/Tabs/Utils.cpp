#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../ClickGUI.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Logic/BedDefense/BedDefenseManager.h"
#include "../../Logic/BlockHitSound.h"
#include "../../Utils/ReplaySpammer.h"
#include <cmath>
#include <cstdio>
#include <gl/GL.h>
#include <string>

namespace Render {
namespace Tabs {

void renderUtils(TabCtx &ctx) {
  using namespace ClickGUIState;
  using namespace ClickGUITheme;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  lClick = ctx.lClick;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Utilities", applyAlpha(0xFFFFFFFF, alpha));
  cy += 40;
  bool hCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 95);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 95, hCard, alpha);
  glEnable(GL_TEXTURE_2D);

  drawSectionLabel(cx, cy, "Bed Defense", alpha);

  g_guiFont.drawString(cx, cy + 18,
                       "X-Ray style outlines for bed defense blocks",
                       applyAlpha(0xFFA0A0A5, alpha));
  g_guiFont.drawString(cx, cy + 42,
                       "WARNING: THIS PROVIDES AN UNFAIR ADVANTAGE.",
                       applyAlpha(0xFFFF5555, alpha), 0.4f);
  g_guiFont.drawString(cx, cy + 54,
                       "YOU WILL BE BLACKLISTED IF CAUGHT. USE AT OWN RISK.",
                       applyAlpha(0xFFFF5555, alpha), 0.4f);

  bool enabled = Config::isBedDefenseEnabled();
  glDisable(GL_TEXTURE_2D);
  float swX = mainX + g_w - 65;
  drawSwitch(0, swX, cy + 15, enabled, hCard, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hCard) {
    bool newState = !enabled;
    Config::setBedDefenseEnabled(newState);
    if (newState)
      BedDefense::BedDefenseManager::getInstance()->enable();
    else
      BedDefense::BedDefenseManager::getInstance()->disable();

    NotificationManager::getInstance()->add(
        "Module", newState ? "Bed Defense Activated" : "Bed Defense Disabled",
        newState ? NotificationType::Success : NotificationType::Warning);
  }
  cy += 115;

  g_guiFont.drawString(cx, cy, "Chat Bypasser",
                       applyAlpha(0xFFFFFFFF, alpha));

  bool hBypass = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 85);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 85, hBypass, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Bypass Chat Filter",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 56, "Allows sending messages that would normally be blocked",
      applyAlpha(0xFFA0A0A5, alpha), 0.45f);

  bool bypassEnabled = Config::isChatBypasserEnabled();
  glDisable(GL_TEXTURE_2D);
  float bypassSwX = mainX + g_w - 65;
  drawSwitch(14, bypassSwX, cy + 40, bypassEnabled, hBypass && (my < cy + 65),
             alpha);
  glEnable(GL_TEXTURE_2D);

  bool hSmart = hBypass && (my >= cy + 65);
  bool smartEnabled = Config::isSmartChatBypassEnabled();
  float smartAlpha = alpha * (bypassEnabled ? 1.0f : 0.4f);

  g_guiFont.drawString(cx + 10, cy + 85, "Smart Mode",
                       applyAlpha(0xFFFFFFFF, smartAlpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(25, bypassSwX, cy + 82, smartEnabled, hSmart && bypassEnabled,
             smartAlpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hBypass) {
    if (my < cy + 65) {
      Config::setChatBypasserEnabled(!bypassEnabled);
      NotificationManager::getInstance()->add(
          "Utils", !bypassEnabled ? "Bypasser Enabled" : "Bypasser Disabled",
          !bypassEnabled ? NotificationType::Success
                         : NotificationType::Warning);
    } else if (bypassEnabled) {
      Config::setSmartChatBypassEnabled(!smartEnabled);
    }
  }
  cy += 135;

  drawSectionLabel(cx, cy, "Faster Stats", alpha);
  bool hNicked = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hNicked, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Direct UUID Fetching",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 58, "Use direct game UUIDs for instant stats",
                       applyAlpha(0xFFA0A0A5, alpha));

  bool nickedBypass = Config::isNickedBypass();
  glDisable(GL_TEXTURE_2D);
  float nickSwX = mainX + g_w - 65;
  drawSwitch(20, nickSwX, cy + 40, nickedBypass, hNicked, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hNicked) {
    Config::setNickedBypass(!nickedBypass);
    NotificationManager::getInstance()->add(
        "Utils",
        !nickedBypass ? "Direct UUID Fetching Enabled"
                      : "Direct UUID Fetching Disabled",
        !nickedBypass ? NotificationType::Success
                      : NotificationType::Warning);
  }
  cy += 110;

  drawSectionLabel(cx, cy, "Raw Mouse Fix", alpha);
  bool hMouse = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hMouse, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Raw Mouse Input",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 58, "Fixes choppy mouse movement in-game",
                       applyAlpha(0xFFA0A0A5, alpha));

  bool mouseFix = Config::isRawMouseFixEnabled();
  glDisable(GL_TEXTURE_2D);
  float mouseSwX = mainX + g_w - 65;
  drawSwitch(28, mouseSwX, cy + 40, mouseFix, hMouse, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hMouse) {
    Config::setRawMouseFixEnabled(!mouseFix);
    NotificationManager::getInstance()->add(
        "Utils",
        !mouseFix ? "Raw Mouse Fix Enabled"
                  : "Raw Mouse Fix Disabled",
        !mouseFix ? NotificationType::Success
                  : NotificationType::Warning);
  }
  cy += 110;

  drawSectionLabel(cx, cy, "Nick Score Alerts", alpha);
  const bool nickRollEnabled = Config::isNickRollEnabled();
  const float nickScoreCardH = nickRollEnabled ? 380.0f : 60.0f;
  const bool hNickScore =
      isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, nickScoreCardH);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, nickScoreCardH,
                hNickScore, alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Score generated /nick names",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Stops on the target word, or score when the target is empty",
      applyAlpha(0xFFA0A0A5, alpha), 0.43f);

  const float nickScoreSwX = mainX + g_w - 65;
  const bool hNickMaster =
      hNickScore && my >= cy + 34.0f && my < cy + 76.0f;
  glDisable(GL_TEXTURE_2D);
  drawSwitch(69, nickScoreSwX, cy + 40, nickRollEnabled, hNickMaster, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hNickMaster) {
    if (nickRollEnabled && s_typingNickRollTarget) {
      Config::setNickRollTargetWord(s_nickRollTargetInput);
      s_nickRollTargetInput = Config::getNickRollTargetWord();
      s_typingNickRollTarget = false;
    }
    Config::setNickRollEnabled(!nickRollEnabled);
    NotificationManager::getInstance()->add(
        "Nick Score",
        !nickRollEnabled ? "Nick scoring enabled" : "Nick scoring disabled",
        !nickRollEnabled ? NotificationType::Success
                         : NotificationType::Warning);
  }

  if (nickRollEnabled) {

  // Keep a dedicated value column to the right of every slider. Previously
  // the track ended underneath the number, so values such as "5000ms" and the
  // knob could draw over one another.
  const float nickValueRight = mainX + g_w - 38.0f;
  const float nickSliderX = cx + 100.0f;
  const float nickSliderRight = mainX + g_w - 92.0f;
  const float nickSliderW = nickSliderRight - nickSliderX;

  float nickThreshold =
      static_cast<float>(Config::getNickScoreThreshold());
  g_guiFont.drawString(cx + 10, cy + 88, "Stop score",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  bool thresholdChanged =
      drawSlider(3070, nickSliderX, cy + 96.0f, nickSliderW, 8.0f,
                 nickThreshold, 0.0f, 100.0f, mx, my, lClick, alpha);
  thresholdChanged =
      drawNumericInput(3070, nickValueRight - 50.0f, cy + 82.0f, 50.0f,
                       25.0f, nickThreshold, 0.0f, 100.0f, 0, "", mx, my,
                       clickEvent, alpha) || thresholdChanged;
  if (thresholdChanged) {
    Config::setNickScoreThreshold(
        static_cast<int>(std::lround(nickThreshold)));
  }

  const bool nickPing = Config::isNickScorePingEnabled();
  const bool hNickPing =
      hNickScore && my >= cy + 112.0f && my < cy + 148.0f;
  g_guiFont.drawString(cx + 10, cy + 124, "Ping when target/pass is found",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(70, nickScoreSwX, cy + 121, nickPing, hNickPing, alpha);
  glEnable(GL_TEXTURE_2D);

  const bool alertEvery = Config::isNickScoreAlertEveryEnabled();
  const bool hAlertEvery =
      hNickScore && my >= cy + 148.0f && my < cy + 184.0f;
  g_guiFont.drawString(cx + 10, cy + 160,
                       alertEvery ? "Alert every nick" : "Alert only found",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(71, nickScoreSwX, cy + 157, alertEvery, hAlertEvery, alpha);
  glEnable(GL_TEXTURE_2D);

  const bool autoReroll = Config::isNickRollAutoRerollEnabled();
  const bool hAutoReroll =
      hNickScore && my >= cy + 184.0f && my < cy + 220.0f;
  g_guiFont.drawString(cx + 10, cy + 196, "Auto TRY AGAIN until target/pass",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(72, nickScoreSwX, cy + 193, autoReroll, hAutoReroll, alpha);
  glEnable(GL_TEXTURE_2D);

  const float targetBoxX = cx + 100.0f;
  const float targetBoxY = cy + 220.0f;
  const float targetBoxW = nickValueRight - targetBoxX;
  const float targetBoxH = 30.0f;
  const bool hTargetWord = isHovered(mx, my, targetBoxX, targetBoxY,
                                     targetBoxW, targetBoxH);
  g_guiFont.drawString(cx + 10, cy + 229, "Target word",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawTextInput(targetBoxX, targetBoxY, targetBoxW, targetBoxH,
                s_typingNickRollTarget, hTargetWord, alpha);
  glEnable(GL_TEXTURE_2D);
  std::string targetDisplay = s_nickRollTargetInput;
  if (targetDisplay.empty() && !s_typingNickRollTarget)
    targetDisplay = "empty = score mode";
  if (s_typingNickRollTarget && (GetTickCount64() / 500) % 2 == 0)
    targetDisplay += "|";
  g_guiFont.drawString(targetBoxX + 9.0f, targetBoxY + 7.0f,
                       targetDisplay.c_str(),
                       applyAlpha(s_nickRollTargetInput.empty() &&
                                          !s_typingNickRollTarget
                                      ? textSecondary()
                                      : textPrimary(),
                                  alpha),
                       0.40f);

  if (clickEvent && hTargetWord) {
    s_typingNickRollTarget = true;
    s_typingSearch = s_typingApiKey = s_typingAutoGG = false;
    s_typingUrchinKey = s_typingSeraphKey = false;
    s_typingAuroraApiKey = s_typingPrefix = false;
    s_typingMuteTagPlayer = false;
  } else if (clickEvent && s_typingNickRollTarget) {
    Config::setNickRollTargetWord(s_nickRollTargetInput);
    s_nickRollTargetInput = Config::getNickRollTargetWord();
    s_typingNickRollTarget = false;
  }

  float rerollDelay =
      static_cast<float>(Config::getNickRollRerollDelayMs());
  g_guiFont.drawString(cx + 10, cy + 268, "Delay",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  bool delayChanged =
      drawSlider(3071, nickSliderX, cy + 276.0f, nickSliderW, 8.0f,
                 rerollDelay, 250.0f, 5000.0f, mx, my, lClick, alpha);
  delayChanged =
      drawNumericInput(3071, nickValueRight - 62.0f, cy + 262.0f, 62.0f,
                       25.0f, rerollDelay, 250.0f, 5000.0f, 0, "ms", mx, my,
                       clickEvent, alpha) || delayChanged;
  if (delayChanged) {
    Config::setNickRollRerollDelayMs(
        static_cast<int>(std::lround(rerollDelay)));
  }

  float rerollCap = static_cast<float>(Config::getNickRollRerollCap());
  g_guiFont.drawString(cx + 10, cy + 302, "Max rerolls",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  bool capChanged =
      drawSlider(3072, nickSliderX, cy + 310.0f, nickSliderW, 8.0f, rerollCap,
                 10.0f, 2000.0f, mx, my, lClick, alpha);
  capChanged =
      drawNumericInput(3072, nickValueRight - 54.0f, cy + 296.0f, 54.0f,
                       25.0f, rerollCap, 10.0f, 2000.0f, 0, "", mx, my,
                       clickEvent, alpha) || capChanged;
  if (capChanged) {
    Config::setNickRollRerollCap(static_cast<int>(std::lround(rerollCap)));
  }

  // Rebindable toggle, default HOME. Polled with GetAsyncKeyState inside
  // NickRollRuntime rather than bound to a window message, because the moment
  // it is useful is with the /nick book open -- and an open GUI screen eats
  // key messages before they reach us.
  g_guiFont.drawString(cx + 10, cy + 344, "Toggle key",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  const float bindX = cx + 100.0f;
  const float bindY = cy + 336.0f;
  const float bindW = nickValueRight - bindX;
  const float bindH = 30.0f;
  const bool hBind = isHovered(mx, my, bindX, bindY, bindW, bindH);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(bindX, bindY, bindW, bindH, hBind, s_waitingForNickRollKey,
                  alpha);
  glEnable(GL_TEXTURE_2D);
  std::string bindText =
      s_waitingForNickRollKey
          ? std::string("Press any key... (ESC to cancel)")
          : ClickGUI::getKeyName(Config::getNickRollToggleKey());
  if (s_waitingForNickRollKey && (GetTickCount64() / 300) % 2 == 0)
    bindText = "> " + bindText + " <";
  g_guiFont.drawString(bindX + 9.0f, bindY + 7.0f, bindText.c_str(),
                       applyAlpha(s_waitingForNickRollKey ? 0xFFFFFFFF
                                                          : accent(),
                                  alpha),
                       0.40f);
  if (clickEvent && hBind && !s_waitingForNickRollKey) {
    s_waitingForNickRollKey = true;
    s_typingNickRollTarget = false;
  }

  if (clickEvent && hNickPing)
    Config::setNickScorePingEnabled(!nickPing);
  else if (clickEvent && hAlertEvery)
    Config::setNickScoreAlertEveryEnabled(!alertEvery);
  else if (clickEvent && hAutoReroll)
    Config::setNickRollAutoRerollEnabled(!autoReroll);
  }
  cy += nickRollEnabled ? 430.0f : 110.0f;

  drawSectionLabel(cx, cy, "Block-Hit Sound (Client Heuristic)", alpha);
  const bool blockSoundEnabled = Config::isBlockHitSoundEnabled();
  const float blockSoundCardH = blockSoundEnabled ? 318.0f : 60.0f;
  bool hBlockSound =
      isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, blockSoundCardH);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, blockSoundCardH,
                hBlockSound, alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Correlated Block-Hit Sound",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Client heuristic only; Minecraft sends no confirmed block result",
      applyAlpha(0xFFA0A0A5, alpha), 0.43f);

  const float blockSoundSwX = mainX + g_w - 65;
  bool hBlockSoundMaster = hBlockSound && my >= cy + 34 && my < cy + 76;
  glDisable(GL_TEXTURE_2D);
  drawSwitch(60, blockSoundSwX, cy + 40, blockSoundEnabled,
             hBlockSoundMaster, alpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hBlockSoundMaster) {
    Config::setBlockHitSoundEnabled(!blockSoundEnabled);
    NotificationManager::getInstance()->add(
        "Utils",
        !blockSoundEnabled ? "Heuristic Block-Hit Sound Enabled"
                           : "Heuristic Block-Hit Sound Disabled",
        !blockSoundEnabled ? NotificationType::Success
                           : NotificationType::Warning);
  }

  if (blockSoundEnabled) {

  const std::string &source = Config::getBlockHitSoundSource();
  g_guiFont.drawString(cx + 10, cy + 88, "Sound source",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  const float sourceX = mainX + g_w - 190.0f;
  const float sourceY = cy + 80.0f;
  const float sourceW = 61.0f;
  const float sourceH = 25.0f;
  const bool hDefault =
      isHovered(mx, my, sourceX, sourceY, sourceW, sourceH);
  const bool hCustom =
      isHovered(mx, my, sourceX + sourceW + 6.0f, sourceY, sourceW, sourceH);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(sourceX, sourceY, sourceW, sourceH, hDefault,
                  source == "Default", alpha);
  drawThemeButton(sourceX + sourceW + 6.0f, sourceY, sourceW, sourceH, hCustom,
                  source == "Custom", alpha);
  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(sourceX + 8.0f, sourceY + 6.0f, "Default",
                       applyAlpha(source == "Default" ? accent()
                                                       : textSecondary(),
                                  alpha),
                       0.38f);
  g_guiFont.drawString(sourceX + sourceW + 14.0f, sourceY + 6.0f, "Custom",
                       applyAlpha(source == "Custom" ? accent()
                                                      : textSecondary(),
                                  alpha),
                       0.38f);
  if (clickEvent && hDefault) Config::setBlockHitSoundSource("Default");
  if (clickEvent && hCustom) Config::setBlockHitSoundSource("Custom");

  float blockSoundVolume = Config::getBlockHitSoundVolume();
  g_guiFont.drawString(cx + 10, cy + 125, "Volume",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  const float blockValueRight = mainX + g_w - 38.0f;
  const float blockSliderX = cx + 100.0f;
  const float blockSliderRight = mainX + g_w - 92.0f;
  bool volumeChanged = drawSlider(3060, blockSliderX, cy + 132.0f,
                 blockSliderRight - blockSliderX, 8.0f,
                 blockSoundVolume, 0.0f, 100.0f, mx, my,
                 lClick, alpha);
  volumeChanged =
      drawNumericInput(3060, blockValueRight - 54.0f, cy + 118.0f, 54.0f,
                       25.0f, blockSoundVolume, 0.0f, 100.0f, 0, "%", mx, my,
                       clickEvent, alpha) || volumeChanged;
  if (volumeChanged) {
    Config::setBlockHitSoundVolume(blockSoundVolume);
  }

  std::string filename = Config::getBlockHitSoundFilename();
  if (filename.size() > 34U) filename = filename.substr(0U, 31U) + "...";
  g_guiFont.drawString(cx + 10, cy + 162, "Selected WAV",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  g_guiFont.drawString(cx + 104, cy + 162, filename.c_str(),
                       applyAlpha(textSecondary(), alpha), 0.40f);

  const char *actionLabels[] = {"Next", "Reload", "Preview", "Folder"};
  const float actionX = cx + 10.0f;
  const float actionY = cy + 185.0f;
  const float actionGap = 7.0f;
  const float actionAreaW = g_w - 260.0f;
  const float actionW = (actionAreaW - actionGap * 3.0f) / 4.0f;
  for (int index = 0; index < 4; ++index) {
    const float buttonX = actionX + index * (actionW + actionGap);
    const bool hovered =
        isHovered(mx, my, buttonX, actionY, actionW, 27.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(buttonX, actionY, actionW, 27.0f, hovered, false, alpha);
    glEnable(GL_TEXTURE_2D);
    const float labelW = g_guiFont.getStringWidth(actionLabels[index]) *
                         (0.38f / 0.5f);
    g_guiFont.drawString(buttonX + actionW * 0.5f - labelW * 0.5f,
                         actionY + 7.0f, actionLabels[index],
                         applyAlpha(hovered ? textPrimary() : textSecondary(),
                                    alpha),
                         0.38f);
    if (clickEvent && hovered) {
      if (index == 0)
        BlockHitSound::requestSelectNextCustomSound();
      else if (index == 1)
        BlockHitSound::requestCustomSoundReload();
      else if (index == 2)
        BlockHitSound::requestPreview();
      else if (!BlockHitSound::openSoundsDirectory())
        NotificationManager::getInstance()->add(
            "Block-Hit Sound", "Could not open the sounds folder",
            NotificationType::Warning);
    }
  }

  const bool waitForServer = Config::isBlockHitWaitForServerEnabled();
  const bool hWaitForServer =
      hBlockSound && my >= cy + 224.0f && my < cy + 266.0f;
  const float waitAlpha = alpha * (blockSoundEnabled ? 1.0f : 0.4f);
  g_guiFont.drawString(cx + 10, cy + 233, "Wait for server registration",
                       applyAlpha(0xFFFFFFFF, waitAlpha), 0.42f);
  g_guiFont.drawString(
      cx + 10, cy + 248,
      waitForServer ? "Confirm the hit landed before playing the sound"
                    : "Play as soon as you are blocking and taking damage",
      applyAlpha(0xFFA0A0A5, waitAlpha), 0.36f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(62, blockSoundSwX, cy + 236, waitForServer,
             hWaitForServer && blockSoundEnabled, waitAlpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hWaitForServer && blockSoundEnabled) {
    Config::setBlockHitWaitForServerEnabled(!waitForServer);
  }

  bool blockSoundDebug = Config::isBlockHitSoundDebugEnabled();
  bool hBlockSoundDebug =
      hBlockSound && my >= cy + 266.0f && my < cy + 308.0f;
  const float blockSoundDebugAlpha =
      alpha * (blockSoundEnabled ? 1.0f : 0.4f);
  g_guiFont.drawString(cx + 10, cy + 281, "Debug trigger/rejection reasons",
                       applyAlpha(0xFFFFFFFF, blockSoundDebugAlpha), 0.42f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(61, blockSoundSwX, cy + 278, blockSoundDebug,
             hBlockSoundDebug && blockSoundEnabled, blockSoundDebugAlpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hBlockSoundDebug) {
    Config::setBlockHitSoundDebugEnabled(!blockSoundDebug);
  }
  }
  cy += blockSoundEnabled ? 368.0f : 110.0f;

  g_guiFont.drawString(cx, cy, "Replay Automations",
                       applyAlpha(0xFFFFFFFF, alpha));
  bool hReplay = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hReplay, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Replay Report Spammer",
                       applyAlpha(0xFF808085, alpha));
  g_guiFont.drawString(cx, cy + 58,
                       "Disabled",
                       applyAlpha(0xFFA0A0A5, alpha));

  if (Utils::ReplaySpammer::getInstance().isEnabled())
    Utils::ReplaySpammer::getInstance().toggle();
  glDisable(GL_TEXTURE_2D);
  float replaySwX = mainX + g_w - 65;
  drawSwitch(21, replaySwX, cy + 40, false, false, alpha * 0.4f);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hReplay) {
    NotificationManager::getInstance()->add(
        "Utils", "Replay Spammer is disabled",
        NotificationType::Warning);
  }
  cy += 110;

  g_guiFont.drawString(cx, cy, "Aurora Denicker",
                       applyAlpha(0xFFFFFFFF, alpha));
  bool hDenick = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hDenick, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Number Denicker",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 58,
                       "Reveal nicks via game statistics",
                       applyAlpha(0xFFA0A0A5, alpha));

  bool denickEnabled = Config::isNumberDenickerEnabled();
  glDisable(GL_TEXTURE_2D);
  float denickSwX = mainX + g_w - 65;
  drawSwitch(22, denickSwX, cy + 40, denickEnabled, hDenick, alpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hDenick) {
    Config::setNumberDenickerEnabled(!denickEnabled);
    Config::save();
    NotificationManager::getInstance()->add(
        "Utils",
        !denickEnabled ? "Number Denicker Enabled"
                       : "Number Denicker Disabled",
        !denickEnabled ? NotificationType::Success
                       : NotificationType::Warning);
  }
  cy += 110;


  drawSectionLabel(cx, cy, "Now Playing Overlay", alpha);
  const bool mediaEnabled = Config::isMediaOverlayEnabled();
  static bool mediaColorsExpanded = false;
  static int mediaPicker = -1; // 0=background, 1=accent, 2=text
  const float mediaCardH =
      mediaEnabled
          ? (mediaColorsExpanded ? (mediaPicker >= 0 ? 690.0f : 466.0f)
                                 : 280.0f)
          : 64.0f;
  const bool hMedia =
      isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, mediaCardH);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, mediaCardH, hMedia, alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Show what is playing",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Reads Windows' media session -- Spotify, browser, any player. No login.",
      applyAlpha(0xFFA0A0A5, alpha), 0.43f);
  const float mediaSwX = mainX + g_w - 65;
  const bool hMediaToggle = hMedia && my < cy + 72.0f;
  glDisable(GL_TEXTURE_2D);
  drawSwitch(63, mediaSwX, cy + 40, mediaEnabled, hMediaToggle, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hMediaToggle)
    Config::setMediaOverlayEnabled(!mediaEnabled);

  if (mediaEnabled) {
    const float contentLeft = cx + 10.0f;
    const float contentRight = mainX + g_w - 36.0f;
    const float valueRight = contentRight;

    // Two visual layout presets. They change geometry only; appearance values
    // remain untouched, so switching presets never destroys a custom theme.
    g_guiFont.drawString(contentLeft, cy + 84, "Layout preset",
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);
    const float presetGap = 10.0f;
    const float presetY = cy + 100.0f;
    const float presetH = 54.0f;
    const float presetW = (contentRight - contentLeft - presetGap) * 0.5f;
    const float wideX = contentLeft;
    const float compactX = wideX + presetW + presetGap;
    const int mediaLayout = Config::getMediaOverlayLayout();
    const bool hWide = isHovered(mx, my, wideX, presetY, presetW, presetH);
    const bool hCompact =
        isHovered(mx, my, compactX, presetY, presetW, presetH);
    auto presetTile = [&](float tileX, const char *label, bool selected,
                          bool portrait, bool hovered) {
      const DWORD tileColor = selected ? accent() : 0xFF242429;
      RenderUtils::drawRoundedRect(tileX, presetY, presetW, presetH, 7.0f,
                                   tileColor,
                                   alpha * (selected ? 0.22f
                                                     : (hovered ? 0.20f : 0.13f)));
      const float previewX = tileX + 10.0f;
      const float previewY = presetY + 25.0f;
      if (portrait) {
        const float miniW = 18.0f;
        const float miniH = 23.0f;
        const float miniX = previewX + 1.0f;
        RenderUtils::drawRoundedRect(miniX, previewY - 4.0f, miniW, miniH,
                                     4.0f, 0xFF111116, alpha * 0.9f);
        RenderUtils::drawRect(miniX + 4.0f, previewY, 10.0f, 10.0f,
                              selected ? accent() : 0xFF777780, alpha * 0.8f);
        RenderUtils::drawRect(miniX + 4.0f, previewY + 13.0f, 10.0f, 1.5f,
                              selected ? accent() : 0xFF777780, alpha * 0.8f);
      } else {
        const float miniW = 36.0f;
        const float miniH = 20.0f;
        RenderUtils::drawRoundedRect(previewX, previewY - 2.0f, miniW, miniH,
                                     4.0f, 0xFF111116, alpha * 0.9f);
        RenderUtils::drawRect(previewX + 4.0f, previewY + 2.0f, 12.0f, 12.0f,
                              selected ? accent() : 0xFF777780, alpha * 0.8f);
        RenderUtils::drawRect(previewX + 20.0f, previewY + 3.0f, 12.0f, 1.5f,
                              0xFFB0B0B6, alpha * 0.7f);
        RenderUtils::drawRect(previewX + 20.0f, previewY + 11.0f, 12.0f, 1.5f,
                              selected ? accent() : 0xFF777780, alpha * 0.8f);
      }
      g_guiFont.drawString(tileX + 58.0f, presetY + 19.0f, label,
                           applyAlpha(selected ? 0xFFFFFFFF : 0xFFC0C0C6,
                                      alpha),
                           0.44f);
      if (selected)
        g_guiFont.drawString(tileX + 58.0f, presetY + 34.0f, "Selected",
                             applyAlpha(accent(), alpha), 0.34f);
    };
    presetTile(wideX, "Wide", mediaLayout == 0, false, hWide);
    presetTile(compactX, "Compact", mediaLayout == 1, true, hCompact);
    if (clickEvent && hWide) Config::setMediaOverlayLayout(0);
    if (clickEvent && hCompact) Config::setMediaOverlayLayout(1);

    // Labels, tracks and values have dedicated columns. In particular the
    // percentage text never sits on top of the slider's final segment/thumb.
    const float sliderX = cx + 176.0f;
    const float availableSliderW = contentRight - 60.0f - sliderX;
    const float sliderW = availableSliderW > 80.0f ? availableSliderW : 80.0f;
    auto plainRow = [&](int id, float rowY, const char *label,
                        const char *suffix, float value, float low, float high,
                        void (*apply)(float), float displayFactor) {
      g_guiFont.drawString(contentLeft, rowY, label,
                           applyAlpha(0xFFFFFFFF, alpha),
                           0.42f);
      float working = value;
      bool changed = drawSlider(id, sliderX, rowY + 7.0f, sliderW, 8.0f,
                                working, low, high, mx, my,
                                lClick && hMedia, alpha);
      float displayed = working * displayFactor;
      changed = drawNumericInput(id, valueRight - 58.0f, rowY - 6.0f, 58.0f,
                                 25.0f, displayed, low * displayFactor,
                                 high * displayFactor, 0, suffix, mx, my,
                                 clickEvent && hMedia, alpha) || changed;
      if (displayed != working * displayFactor) {
        working = displayed / displayFactor;
        changed = true;
      }
      if (changed) {
        apply(working);
      }
    };

    float mediaScale = Config::getMediaOverlayScale();
    plainRow(3073, cy + 176, "Size", "%", mediaScale, 0.5f, 2.5f,
             [](float v) { Config::setMediaOverlayScale(v); }, 100.0f);
    float mediaOpacity = Config::getMediaOverlayOpacity();
    plainRow(3074, cy + 212, "Background opacity", "%", mediaOpacity, 0.0f,
             1.0f, [](float v) { Config::setMediaOverlayOpacity(v); }, 100.0f);

    const bool mediaArt = Config::isMediaOverlayArtEnabled();
    const bool hMediaArt = hMedia && my >= cy + 235.0f && my < cy + 268.0f;
    g_guiFont.drawString(contentLeft, cy + 249, "Show album art",
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    glDisable(GL_TEXTURE_2D);
    drawSwitch(64, mediaSwX, cy + 242, mediaArt, hMediaArt, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hMediaArt)
      Config::setMediaOverlayArtEnabled(!mediaArt);

    const bool hAdvanced =
        hMedia && my >= cy + 271.0f && my < cy + 307.0f;
    g_guiFont.drawString(contentLeft, cy + 284, "Advanced appearance",
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    g_guiFont.drawString(contentRight - 82.0f, cy + 284,
                         mediaColorsExpanded ? "Hide" : "Customize",
                         applyAlpha(hAdvanced ? accent() : 0xFFA0A0A5, alpha),
                         0.38f);
    if (clickEvent && hAdvanced) {
      mediaColorsExpanded = !mediaColorsExpanded;
      if (!mediaColorsExpanded) {
        mediaPicker = -1;
        ClickGUIHelpers::cancelInlineEditors();
      }
    }

    // Every media colour uses the same picker as the theme and stat ranges.
    // A compact swatch row keeps the card readable until one is opened.
    auto colorRow = [&](int pickerSlot, float rowY, const char *label,
                        unsigned long current) {
      g_guiFont.drawString(contentLeft, rowY, label,
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);
      const float swatch = 32.0f;
      const float buttonW = 128.0f;
      const float buttonX = valueRight - buttonW;
      const bool hovered =
          isHovered(mx, my, buttonX, rowY - 5.0f, buttonW, 32.0f);
      glDisable(GL_TEXTURE_2D);
      drawThemeButton(buttonX, rowY - 5.0f, buttonW, 32.0f, hovered,
                      mediaPicker == pickerSlot, alpha);
      RenderUtils::drawRoundedRect(buttonX + 5.0f, rowY, swatch, 22.0f, 5.0f,
                                   0xFF000000 | current, alpha);
      glEnable(GL_TEXTURE_2D);
      char hex[12]{};
      snprintf(hex, sizeof(hex), "#%06lX", current & 0xFFFFFFUL);
      g_guiFont.drawString(buttonX + 44.0f, rowY + 1.0f, hex,
                           applyAlpha(0xFFC8C8CE, alpha), 0.38f);
      if (clickEvent && hovered) {
        ClickGUIHelpers::cancelInlineEditors();
        mediaPicker = mediaPicker == pickerSlot ? -1 : pickerSlot;
      }
    };

    if (mediaColorsExpanded) {
      float mediaCorner = Config::getMediaOverlayCorner();
      plainRow(3075, cy + 318, "Corner rounding", "", mediaCorner, 0.0f,
               20.0f,
               [](float v) { Config::setMediaOverlayCorner(v); }, 1.0f);
      colorRow(0, cy + 360, "Background", Config::getMediaOverlayBgColor());
      colorRow(1, cy + 405, "Accent", Config::getMediaOverlayAccentColor());
      colorRow(2, cy + 450, "Text", Config::getMediaOverlayTextColor());
      if (mediaPicker >= 0) {
        unsigned long raw =
            mediaPicker == 0 ? Config::getMediaOverlayBgColor()
                             : (mediaPicker == 1
                                    ? Config::getMediaOverlayAccentColor()
                                    : Config::getMediaOverlayTextColor());
        std::uint32_t selected = 0xFF000000u |
                                 static_cast<std::uint32_t>(raw & 0xFFFFFFUL);
        if (drawColorPicker(9200 + mediaPicker, contentLeft, cy + 493.0f,
                            contentRight - contentLeft, selected, mx, my,
                            lClick && hMedia, clickEvent && hMedia, alpha)) {
          const unsigned long rgb = selected & 0xFFFFFFu;
          if (mediaPicker == 0)
            Config::setMediaOverlayBgColor(rgb);
          else if (mediaPicker == 1)
            Config::setMediaOverlayAccentColor(rgb);
          else
            Config::setMediaOverlayTextColor(rgb);
        }
      }
    }
  }
  cy += mediaEnabled ? mediaCardH + 48.0f : 114.0f;

  drawSectionLabel(cx, cy, "Anticheat", alpha);
  const float acCardH = 262.0f;
  bool hAc = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, acCardH);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, acCardH, hAc, alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Detect Cheaters (BETA)",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Four client-side checks: NoSlow, AutoBlock, Eagle, Scaffold",
      applyAlpha(0xFFA0A0A5, alpha), 0.45f);

  bool acEnabled = Config::isAnticheatEnabled();
  glDisable(GL_TEXTURE_2D);
  float acSwX = mainX + g_w - 65;
  bool hAcMaster = hAc && my < cy + 70;
  drawSwitch(40, acSwX, cy + 40, acEnabled, hAcMaster, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hAcMaster) {
    Config::setAnticheatEnabled(!acEnabled);
    NotificationManager::getInstance()->add(
        "Utils", !acEnabled ? "Anticheat Enabled" : "Anticheat Disabled",
        !acEnabled ? NotificationType::Success : NotificationType::Warning);
  }

  float rowAlpha = alpha * (acEnabled ? 1.0f : 0.45f);
  struct AcSub {
    const char *label;
    bool (*get)();
    void (*set)(bool);
    int switchId;
  };
  static const AcSub kSubs[] = {
      {"NoSlow", &Config::isAnticheatNoSlowEnabled,
       &Config::setAnticheatNoSlowEnabled, 41},
      {"AutoBlock", &Config::isAnticheatAutoBlockEnabled,
       &Config::setAnticheatAutoBlockEnabled, 42},
      {"Eagle", &Config::isAnticheatEagleEnabled,
       &Config::setAnticheatEagleEnabled, 43},
      {"Scaffold", &Config::isAnticheatScaffoldEnabled,
       &Config::setAnticheatScaffoldEnabled, 44},
      {"Check Self", &Config::isAnticheatCheckSelfEnabled,
       &Config::setAnticheatCheckSelfEnabled, 46},
  };
  const float subStartY = cy + 84;
  const float subRowH = 32.0f;
  for (size_t i = 0; i < sizeof(kSubs) / sizeof(kSubs[0]); ++i) {
    float ry = subStartY + (float)i * subRowH;
    bool hSub = hAc && my >= ry - 2 && my < ry + subRowH - 2;
    bool cur = kSubs[i].get();
    g_guiFont.drawString(cx + 10, ry + 4, kSubs[i].label,
                         applyAlpha(0xFFFFFFFF, rowAlpha), 0.42f);
    glDisable(GL_TEXTURE_2D);
    drawSwitch(kSubs[i].switchId, acSwX, ry + 2, cur, hSub && acEnabled,
               rowAlpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hSub && acEnabled) {
      kSubs[i].set(!cur);
    }
  }
  cy += (int)acCardH + 25;
}

} // namespace Tabs
} // namespace Render
