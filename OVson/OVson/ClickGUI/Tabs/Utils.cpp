#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Logic/BedDefense/BedDefenseManager.h"
#include "../../Utils/ReplaySpammer.h"
#include <gl/GL.h>

namespace Render {
namespace Tabs {

void renderUtils(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Utilities", applyAlpha(0xFFFFFFFF, alpha));
  cy += 40;
  g_guiFont.drawString(cx, cy, "Bed Defense", applyAlpha(0xFFFFFFFF, alpha));
  bool hCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 95);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 95, hCard, alpha);
  glEnable(GL_TEXTURE_2D);

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

  g_guiFont.drawString(cx, cy, "Faster Stats", applyAlpha(0xFFFFFFFF, alpha));
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

  g_guiFont.drawString(cx, cy, "Replay Automations",
                       applyAlpha(0xFFFFFFFF, alpha));
  bool hReplay = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy + 30, g_w - 210, 60, hReplay, alpha);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(cx, cy + 40, "Replay Report Spammer",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 58,
                       "Auto reporting for cheating (requires Anvil menu)",
                       applyAlpha(0xFFA0A0A5, alpha));

  bool replaySpammer = Utils::ReplaySpammer::getInstance().isEnabled();
  glDisable(GL_TEXTURE_2D);
  float replaySwX = mainX + g_w - 65;
  drawSwitch(21, replaySwX, cy + 40, replaySpammer, hReplay, alpha);
  glEnable(GL_TEXTURE_2D);
  if (clickEvent && hReplay) {
    Utils::ReplaySpammer::getInstance().toggle();
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
                       "Reveal nicks via game statistics (Powered by Aurora)",
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

  g_guiFont.drawString(cx, cy, "Anticheat", applyAlpha(0xFFFFFFFF, alpha));
  const float acCardH = 222.0f;
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
  const float subStartY = cy + 78;
  const float subRowH = 22.0f;
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
