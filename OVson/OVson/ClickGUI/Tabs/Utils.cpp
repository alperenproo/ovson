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
#include "../../Logic/MojangCape.h"
#include "../../Utils/ReplaySpammer.h"
#include "../ClickGUI_Bridge.h"
#include "../../Services/IrcService.h"
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

  if (s_moduleSearch.empty()) {
    g_guiFont.drawString(cx, cy, "Utilities", applyAlpha(0xFFFFFFFF, alpha));
    cy += 40;
  }
  if (shouldShowInSearch("Bed Defense", "X-Ray style outlines for bed defense blocks")) {
    bool hCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    drawSectionLabel(cx, cy, "Bed Defense", alpha);

    g_guiFont.drawString(cx, cy + 18,
                         "X-Ray style outlines for bed defense blocks",
                         applyAlpha(0xFFA0A0A5, alpha));

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
    cy += 75;
  }

  if (shouldShowInSearch("Chat Bypass", "Allows sending messages that would normally be blocked")) {
    drawSectionLabel(cx, cy, "Chat Bypasser", alpha);

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
  }

  if (shouldShowInSearch("Direct UUID", "Use direct game UUIDs for faster stats Faster Stats")) {
    drawSectionLabel(cx, cy, "Faster Stats", alpha);
    bool hNicked = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hNicked, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Direct UUID Fetching",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Use direct game UUIDs for faster stats",
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
  }

  if (shouldShowInSearch("Raw Mouse Fix", "Fixes choppy mouse movement in-game Raw Mouse Input")) {
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
  }

  if (shouldShowInSearch("Mute Own Footsteps", "Silences your own footstep sounds mute own steps")) {
    drawSectionLabel(cx, cy, "Footsteps", alpha);
    bool hMute = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hMute, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Mute Own Steps",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Silences your own footstep sounds",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool muteSteps = Config::isMuteOwnStepsEnabled();
    glDisable(GL_TEXTURE_2D);
    float muteSwX = mainX + g_w - 65;
    drawSwitch(131, muteSwX, cy + 40, muteSteps, hMute, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hMute) {
      Config::setMuteOwnStepsEnabled(!muteSteps);
      NotificationManager::getInstance()->add(
          "Utils",
          !muteSteps ? "Mute Own Steps Enabled"
                     : "Mute Own Steps Disabled",
          !muteSteps ? NotificationType::Success
                     : NotificationType::Warning);
    }
    cy += 110;
  }

  if (shouldShowInSearch("Bow Hit Distance", "Shows shot distance in chat when an arrow hits an enemy player bow distance shot")) {
    drawSectionLabel(cx, cy, "Bow Distance", alpha);
    bool hBowDist = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hBowDist, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Bow Hit Distance",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Shows shot distance in chat when an arrow hits an enemy player",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool bowDist = Config::isBowDistanceEnabled();
    glDisable(GL_TEXTURE_2D);
    float bowSwX = mainX + g_w - 65;
    drawSwitch(132, bowSwX, cy + 40, bowDist, hBowDist, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hBowDist) {
      Config::setBowDistanceEnabled(!bowDist);
      NotificationManager::getInstance()->add(
          "Utils",
          !bowDist ? "Bow Hit Distance Enabled"
                   : "Bow Hit Distance Disabled",
          !bowDist ? NotificationType::Success
                   : NotificationType::Warning);
    }
    cy += 110;
  }

  if (shouldShowInSearch("Prevent Bow Dropping", "Stops you from accidentally dropping your bow with the drop key prevent bow drop")) {
    drawSectionLabel(cx, cy, "Prevent Bow Drop", alpha);
    bool hBowDrop = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hBowDrop, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Prevent Bow Dropping",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Stops you from accidentally dropping your bow with the drop key",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool bowDrop = Config::isPreventBowDropEnabled();
    glDisable(GL_TEXTURE_2D);
    float dropSwX = mainX + g_w - 65;
    drawSwitch(133, dropSwX, cy + 40, bowDrop, hBowDrop, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hBowDrop) {
      Config::setPreventBowDropEnabled(!bowDrop);
      NotificationManager::getInstance()->add(
          "Utils",
          !bowDrop ? "Prevent Bow Dropping Enabled"
                   : "Prevent Bow Dropping Disabled",
          !bowDrop ? NotificationType::Success
                   : NotificationType::Warning);
    }
    cy += 110;
  }

  if (shouldShowInSearch("IRC", "IRC chat network prefix @ encrypted cross-server chat ding sound appear offline")) {
    drawSectionLabel(cx, cy, "IRC", alpha);
    bool ircEnabled = Config::isIrcEnabled();
    const float ircCardH = ircEnabled ? 160.0f : 60.0f;
    bool hIrc = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, ircCardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, ircCardH, hIrc, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "IRC Chat",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Encrypted cross-server chat (Prefix: @)",
                         applyAlpha(0xFFA0A0A5, alpha));

    float ircSwX = mainX + g_w - 65;
    bool hIrcMaster = isHovered(mx, my, ircSwX - 5, cy + 35, 45, 25);
    bool hIrcTopRow = hIrc && my < cy + 70.0f;
    glDisable(GL_TEXTURE_2D);
    drawSwitch(135, ircSwX, cy + 40, ircEnabled, hIrcMaster, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && (hIrcMaster || hIrcTopRow)) {
      bool newState = !ircEnabled;
      Config::setIrcEnabled(newState);
      if (newState) {
        IrcService::initialize();
      } else {
        IrcService::shutdown();
      }
      NotificationManager::getInstance()->add(
          "Utils",
          newState ? "IRC Enabled (Prefix: @)"
                   : "IRC Disabled",
          newState ? NotificationType::Success
                   : NotificationType::Warning);
    }

    if (ircEnabled) {
      float subY = cy + 76.0f;
      g_guiFont.drawString(cx, subY + 2.0f, "Notification Ding",
                           applyAlpha(0xFFFFFFFF, alpha), 0.44f);
      g_guiFont.drawString(cx, subY + 18.0f, "Plays in-game ding sound when a new message arrives",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);

      bool ircDing = Config::isIrcDingEnabled();
      bool hDingSw = isHovered(mx, my, ircSwX - 5, subY + 2.0f, 45, 25);
      bool hDingRow = hIrc && my >= cy + 70.0f && my < cy + 115.0f;
      glDisable(GL_TEXTURE_2D);
      drawSwitch(136, ircSwX, subY + 6.0f, ircDing, hDingSw, alpha);
      glEnable(GL_TEXTURE_2D);
      if (clickEvent && (hDingSw || hDingRow)) {
        Config::setIrcDingEnabled(!ircDing);
        NotificationManager::getInstance()->add(
            "Utils",
            !ircDing ? "IRC Ding Sound Enabled"
                     : "IRC Ding Sound Disabled",
            !ircDing ? NotificationType::Success
                     : NotificationType::Warning);
      }

      float subY2 = cy + 120.0f;
      g_guiFont.drawString(cx, subY2 + 2.0f, "Appear Offline",
                           applyAlpha(0xFFFFFFFF, alpha), 0.44f);
      g_guiFont.drawString(cx, subY2 + 18.0f, "Hides you from the IRC user list (.irc list)",
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);

      bool ircOffline = IrcService::isAppearOffline();
      bool hOfflineSw = isHovered(mx, my, ircSwX - 5, subY2 + 2.0f, 45, 25);
      bool hOfflineRow = hIrc && my >= cy + 115.0f;
      glDisable(GL_TEXTURE_2D);
      drawSwitch(137, ircSwX, subY2 + 6.0f, ircOffline, hOfflineSw, alpha);
      glEnable(GL_TEXTURE_2D);
      if (clickEvent && (hOfflineSw || hOfflineRow)) {
        bool nextOffline = !ircOffline;
        IrcService::setAppearOffline(nextOffline);
        NotificationManager::getInstance()->add(
            "Utils",
            nextOffline ? "IRC: Appear Offline Enabled"
                        : "IRC: Appear Offline Disabled",
            nextOffline ? NotificationType::Warning
                        : NotificationType::Success);
      }
    }
    cy += ircCardH + 50.0f;
  }

  if (shouldShowInSearch("Cape Fixer", "Restores your mojang cape when nicked")) {
    drawSectionLabel(cx, cy, "Original Capes", alpha);
    const bool capeEnabled = Config::isMojangCapeEnabled();
    const float capeCardH = capeEnabled ? 105.0f : 60.0f;
    bool hCape = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, capeCardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, capeCardH, hCape, alpha, capeEnabled);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Cape Fixer",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Restores your mojang cape when nicked",
                         applyAlpha(0xFFA0A0A5, alpha));

    glDisable(GL_TEXTURE_2D);
    float capeSwX = mainX + g_w - 65;
    drawSwitch(134, capeSwX, cy + 40, capeEnabled, hCape, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && isHovered(mx, my, capeSwX - 5, cy + 35, 45, 25)) {
      Config::setMojangCapeEnabled(!capeEnabled);
      NotificationManager::getInstance()->add(
          "Utils",
          !capeEnabled ? "Mojang Cape Enabled"
                       : "Mojang Cape Disabled",
          !capeEnabled ? NotificationType::Success
                       : NotificationType::Warning);
    } else if (clickEvent && hCape && my < cy + 70) {
      Config::setMojangCapeEnabled(!capeEnabled);
      NotificationManager::getInstance()->add(
          "Utils",
          !capeEnabled ? "Mojang Cape Enabled"
                       : "Mojang Cape Disabled",
          !capeEnabled ? NotificationType::Success
                       : NotificationType::Warning);
    }

    if (capeEnabled) {
      std::string statusText = MojangCape::hasAccountCape() ? "Account Cape: Active" : "Account Cape: Detecting...";
      float statusX = cx + 10.0f;
      float statusY = cy + 90.0f;
      g_guiFont.drawString(statusX, statusY + 5.0f, statusText.c_str(),
                           applyAlpha(MojangCape::hasAccountCape() ? 0xFF10B981 : 0xFFF59E0B, alpha), 0.42f);

      float nickedX = statusX + g_guiFont.getStringWidth(statusText) * (0.42f / 0.5f) + 32.0f;
      g_guiFont.drawString(nickedX, statusY + 5.0f, "Only When Nicked",
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);
      bool onlyNicked = Config::isMojangCapeOnlyNicked();
      float nickedSwX = nickedX + g_guiFont.getStringWidth("Only When Nicked") * (0.42f / 0.5f) + 16.0f;
      bool hNickedSw = isHovered(mx, my, nickedSwX - 4, statusY - 2, 45, 26);

      glDisable(GL_TEXTURE_2D);
      drawSwitch(135, nickedSwX, statusY + 2.0f, onlyNicked, hNickedSw, alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hNickedSw) {
        Config::setMojangCapeOnlyNicked(!onlyNicked);
      }
    }

    cy += (int)capeCardH + 50;
  }

  const bool nickRollEnabled = Config::isNickRollEnabled();
  const float nickScoreCardH = nickRollEnabled ? 475.0f : 60.0f;
  if (shouldShowInSearch("Nick Helper", "Score generated /nick names auto-rerolls allow logging nickreroll limbo lobby")) {
    drawSectionLabel(cx, cy, "Nick Helper", alpha);
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
                 nickThreshold, 0.0f, 101.0f, mx, my, lClick, alpha);
  thresholdChanged =
      drawNumericInput(3070, nickValueRight - 50.0f, cy + 82.0f, 50.0f,
                       25.0f, nickThreshold, 0.0f, 101.0f, 0, "", mx, my,
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
                 10.0f, 10000.0f, mx, my, lClick, alpha);
  capChanged =
      drawNumericInput(3072, nickValueRight - 54.0f, cy + 296.0f, 54.0f,
                       25.0f, rerollCap, 10.0f, 10000.0f, 0, "", mx, my,
                       clickEvent, alpha) || capChanged;
  if (capChanged) {
    Config::setNickRollRerollCap(static_cast<int>(std::lround(rerollCap)));
  }

  if (clickEvent && hNickPing)
    Config::setNickScorePingEnabled(!nickPing);
  else if (clickEvent && hAlertEvery)
    Config::setNickScoreAlertEveryEnabled(!alertEvery);
  else if (clickEvent && hAutoReroll)
    Config::setNickRollAutoRerollEnabled(!autoReroll);

  const bool nickLogAll = Config::isNickRollLogAllEnabled();
  const float logRowY = cy + 338.0f;
  const bool hNickLog = hNickScore && my >= logRowY - 2.0f && my < logRowY + 44.0f;
  g_guiFont.drawString(cx + 10, logRowY + 2, "Allow logging",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  g_guiFont.drawString(cx + 10, logRowY + 18, "Log failed attempts in a folder",
                       applyAlpha(0xFFA0A0A5, alpha), 0.35f);
  g_guiFont.drawString(cx + 10, logRowY + 31, "located in %localappdata%\\OVson\\nickreroll",
                       applyAlpha(0xFF75757A, alpha), 0.30f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(73, nickScoreSwX, logRowY + 10, nickLogAll, hNickLog, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hNickLog) {
    Config::setNickRollLogAllEnabled(!nickLogAll);
    NotificationManager::getInstance()->add(
        "Nick Score",
        !nickLogAll ? "Logging enabled (%localappdata%\\OVson\\nickreroll)"
                    : "Logging disabled",
        !nickLogAll ? NotificationType::Success : NotificationType::Warning);
  }


  const bool limboRecovery = Config::isNickRollLimboRecoveryEnabled();
  const float limboRowY = cy + 382.0f;
  const bool hNickLimbo = hNickScore && my >= limboRowY - 2.0f && my < limboRowY + 36.0f;
  g_guiFont.drawString(cx + 10, limboRowY + 2, "Auto Limbo recovery",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  g_guiFont.drawString(cx + 10, limboRowY + 18, "Escapes Hypixel Limbo to lobby automatically",
                       applyAlpha(0xFFA0A0A5, alpha), 0.35f);
  glDisable(GL_TEXTURE_2D);
  drawSwitch(74, nickScoreSwX, limboRowY + 6, limboRecovery, hNickLimbo, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hNickLimbo) {
    Config::setNickRollLimboRecoveryEnabled(!limboRecovery);
    NotificationManager::getInstance()->add(
        "Nick Helper",
        !limboRecovery ? "Limbo recovery enabled" : "Limbo recovery disabled",
        !limboRecovery ? NotificationType::Success : NotificationType::Warning);
  }

  float lobbySwap = static_cast<float>(Config::getNickRollLobbySwapInterval());
  const float swapRowY = cy + 424.0f;
  g_guiFont.drawString(cx + 10, swapRowY + 4, "Lobby swap",
                       applyAlpha(0xFFFFFFFF, alpha), 0.42f);
  g_guiFont.drawString(cx + 10, swapRowY + 19, "Rotate lobby every N rolls (0 = off)",
                       applyAlpha(0xFFA0A0A5, alpha), 0.35f);
  bool swapChanged =
      drawSlider(3073, nickSliderX, swapRowY + 14.0f, nickSliderW, 8.0f, lobbySwap,
                 0.0f, 100.0f, mx, my, lClick, alpha);
  swapChanged =
      drawNumericInput(3073, nickValueRight - 65.0f, swapRowY + 4.0f, 65.0f,
                       25.0f, lobbySwap, 0.0f, 100.0f, 0, " rolls", mx, my,
                       clickEvent, alpha) || swapChanged;
  if (swapChanged) {
    Config::setNickRollLobbySwapInterval(static_cast<int>(std::lround(lobbySwap)));
  }
  }
  cy += nickRollEnabled ? 525.0f : 110.0f;
  }

  const bool blockSoundEnabled = Config::isBlockHitSoundEnabled();
  const float blockSoundCardH = blockSoundEnabled ? 318.0f : 60.0f;
  if (shouldShowInSearch("Block Hit Sound", "Plays a sound when you block a hit audio")) {
    drawSectionLabel(cx, cy, "Block Hit Sound", alpha);
    bool hBlockSound =
        isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, blockSoundCardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, blockSoundCardH,
                  hBlockSound, alpha);
    glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Block Hit Sound",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Plays a sound when you block a hit",
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
        !blockSoundEnabled ? "Block Hit Sound Enabled"
                           : "Block Hit Sound Disabled",
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
  }

  if (shouldShowInSearch("Replay Spammer", "Replay report spammer")) {
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
  }

  if (shouldShowInSearch("Number Denicker", "Reveal nicks via game statistics Aurora")) {
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
  }


  const bool mediaEnabled = Config::isMediaOverlayEnabled();
  static bool mediaColorsExpanded = false;
  static int mediaPicker = -1; // 0=background, 1=accent, 2=text
  const float mediaCardH =
      mediaEnabled
          ? (mediaColorsExpanded ? (mediaPicker >= 0 ? 690.0f : 466.0f)
                                 : 280.0f)
          : 64.0f;
  if (shouldShowInSearch("Now Playing", "Spotify media overlay")) {
    drawSectionLabel(cx, cy, "Now Playing Overlay", alpha);
    const bool hMedia =
        isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, mediaCardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, mediaCardH, hMedia, alpha);
    glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy + 40, "Show what is playing",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(
      cx, cy + 58,
      "Reads Windows media session",
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

    g_guiFont.drawString(contentLeft, cy + 80, "Layout preset",
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);
    const float presetGap = 10.0f;
    const float presetY = cy + 100.0f;
    const float presetH = 50.0f;
    const float presetW = (contentRight - contentLeft - presetGap) * 0.5f;
    const float wideX = contentLeft;
    const float compactX = wideX + presetW + presetGap;
    const int mediaLayout = Config::getMediaOverlayLayout();
    const bool hWide = isHovered(mx, my, wideX, presetY, presetW, presetH);
    const bool hCompact =
        isHovered(mx, my, compactX, presetY, presetW, presetH);
    auto presetTile = [&](float tileX, const char *label, bool selected,
                          bool portrait, bool hovered) {
      const DWORD tileColor = selected ? accent() : (hovered ? 0xFF2F2F37 : 0xFF242429);
      RenderUtils::drawRoundedRect(tileX, presetY, presetW, presetH, 7.0f,
                                   tileColor,
                                   alpha * (selected ? 0.25f
                                                     : (hovered ? 0.22f : 0.13f)));
      RenderUtils::drawRoundedOutline(tileX, presetY, presetW, presetH, 7.0f, 1.0f,
                                      selected ? accent() : (hovered ? 0xFF50505A : 0x22FFFFFF),
                                      alpha * (selected ? 0.60f : (hovered ? 0.50f : 0.25f)));

      const float miniW = portrait ? 18.0f : 34.0f;
      const float miniH = portrait ? 22.0f : 18.0f;
      float lblW = g_guiFont.getStringWidth(label) * (0.44f / 0.5f);
      float groupW = miniW + 12.0f + lblW;
      float startX = tileX + (presetW - groupW) * 0.5f;
      float previewX = startX;
      float textX = startX + miniW + 12.0f;
      float previewY = presetY + (presetH - miniH) * 0.5f - 2.0f;

      if (portrait) {
        RenderUtils::drawRoundedRect(previewX, previewY, miniW, miniH,
                                     4.0f, 0xFF111116, alpha * 0.9f);
        RenderUtils::drawRect(previewX + 4.0f, previewY + 4.0f, 10.0f, 10.0f,
                              selected ? accent() : (hovered ? 0xFF9999A6 : 0xFF777780), alpha * 0.85f);
        RenderUtils::drawRect(previewX + 4.0f, previewY + 16.0f, 10.0f, 1.5f,
                              selected ? accent() : (hovered ? 0xFF9999A6 : 0xFF777780), alpha * 0.85f);
      } else {
        RenderUtils::drawRoundedRect(previewX, previewY, miniW, miniH,
                                     4.0f, 0xFF111116, alpha * 0.9f);
        RenderUtils::drawRect(previewX + 4.0f, previewY + 3.0f, 12.0f, 12.0f,
                              selected ? accent() : (hovered ? 0xFF9999A6 : 0xFF777780), alpha * 0.85f);
        RenderUtils::drawRect(previewX + 19.0f, previewY + 4.0f, 11.0f, 1.5f,
                              0xFFB0B0B6, alpha * 0.7f);
        RenderUtils::drawRect(previewX + 19.0f, previewY + 11.0f, 11.0f, 1.5f,
                              selected ? accent() : (hovered ? 0xFF9999A6 : 0xFF777780), alpha * 0.85f);
      }

      float lblY = selected ? (presetY + 11.0f) : (presetY + 17.0f);
      g_guiFont.drawString(textX, lblY, label,
                           applyAlpha(selected ? 0xFFFFFFFF : (hovered ? 0xFFE0E0E6 : 0xFFC0C0C6),
                                      alpha),
                           0.44f);
      if (selected)
        g_guiFont.drawString(textX, presetY + 26.0f, "Selected",
                             applyAlpha(accent(), alpha), 0.34f);
    };
    presetTile(wideX, "Wide", mediaLayout == 0, false, hWide);
    presetTile(compactX, "Compact", mediaLayout == 1, true, hCompact);
    if (clickEvent && hWide) Config::setMediaOverlayLayout(0);
    if (clickEvent && hCompact) Config::setMediaOverlayLayout(1);

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

    const float advBtnW = 82.0f;
    const float advBtnH = 26.0f;
    const float advBtnX = contentRight - advBtnW;
    const float advBtnY = cy + 276.0f;
    const bool hAdvancedBtn = hMedia && isHovered(mx, my, advBtnX, advBtnY, advBtnW, advBtnH);
    g_guiFont.drawString(contentLeft, cy + 284, "Advanced appearance",
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(advBtnX, advBtnY, advBtnW, advBtnH, hAdvancedBtn, mediaColorsExpanded, alpha);
    glEnable(GL_TEXTURE_2D);
    const char *advBtnLabel = mediaColorsExpanded ? "Hide" : "Customize";
    float advLblW = g_guiFont.getStringWidth(advBtnLabel) * (0.38f / 0.5f);
    g_guiFont.drawString(advBtnX + (advBtnW - advLblW) * 0.5f, advBtnY + 7.0f,
                         advBtnLabel,
                         applyAlpha(mediaColorsExpanded ? accent() : 0xFFFFFFFF, alpha),
                         0.38f);
    if (clickEvent && hAdvancedBtn) {
      mediaColorsExpanded = !mediaColorsExpanded;
      if (!mediaColorsExpanded) {
        mediaPicker = -1;
        ClickGUIHelpers::cancelInlineEditors();
      }
    }

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
  }

  const float acCardH = 262.0f;
  if (shouldShowInSearch("Anticheat", "Built-in heuristic anticheat detection NoSlow AutoBlock Eagle Scaffold")) {
    drawSectionLabel(cx, cy, "Anticheat", alpha);
    bool hAc = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, acCardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy + 30, g_w - 210, acCardH, hAc, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 40, "Detect Cheaters",
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

  if (s_moduleSearch.empty()) {
    renderCustomScripts(ctx, "Utils");
  }
}

} // namespace Tabs
} // namespace Render
