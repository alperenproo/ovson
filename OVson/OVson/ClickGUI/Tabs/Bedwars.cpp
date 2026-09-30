#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Tabs.h"

#include "../Helpers.h"
#include "../State.h"
#include "../Theme.h"
#include "../../Config/Config.h"
#include "../../Logic/BedDefense/BedDefenseManager.h"
#include "../../Logic/Bedwars/BedwarsConfig.h"
#include "../../Logic/Bedwars/BedwarsCore.h"
#include "../../Logic/Bedwars/BedwarsRuntime.h"
#include "../../Render/BedwarsOverlay.h"
#include "../../Render/NotificationManager.h"

#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Render::Tabs {

void renderBedwars(TabCtx &ctx) {
  using namespace ClickGUIState;
  using namespace ClickGUITheme;
  using namespace OVson::Bedwars;
  namespace BwConfig = OVson::Bedwars::Configuration;

  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  lClick = ctx.lClick;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  const auto settings = BwConfig::get();
  const auto runtime = Runtime::instance().snapshot();
  const bool master = settings.masterEnabled;
  const float cardW = g_w - 210.0f;
  const float cardX = mainX + 190.0f;
  const float swX = mainX + g_w - 65.0f;
  const float valRight = mainX + g_w - 38.0f;

  static ULONGLONG resetArmedAt = 0;

  auto drawSimpleToggleCard = [&](const char *title, const char *description,
                                  bool value, int switchId, auto setter) {
    if (!shouldShowInSearch(title, description)) return;
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, 60.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, 60.0f, hCard, alpha, value);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, title,
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    g_guiFont.drawString(cx, cy + 18.0f, description,
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);

    glDisable(GL_TEXTURE_2D);
    drawSwitch(switchId, swX, cy + 10.0f, value, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hCard) {
      setter(!value);
    }
    cy += 75.0f;
  };

  auto drawToggleSliderCard = [&](const char *title, const char *description,
                                  bool toggleVal, int switchId, auto toggleSetter,
                                  const char *sliderLabel, float sliderVal,
                                  float minVal, float maxVal, int sliderId,
                                  const char *suffix, auto sliderSetter) {
    if (!shouldShowInSearch(title, description)) return;
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, 95.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, 95.0f, hCard, alpha, toggleVal);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, title,
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    g_guiFont.drawString(cx, cy + 18.0f, description,
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);

    glDisable(GL_TEXTURE_2D);
    drawSwitch(switchId, swX, cy + 10.0f, toggleVal, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && isHovered(mx, my, swX - 5.0f, cy + 5.0f, 45.0f, 25.0f)) {
      toggleSetter(!toggleVal);
    } else if (clickEvent && isHovered(mx, my, cardX, cy - 10.0f, cardW, 40.0f)) {
      toggleSetter(!toggleVal);
    }

    const float labelW = g_guiFont.getStringWidth(sliderLabel) * (0.40f / 0.5f) + 16.0f;
    const float slX = cx + labelW;
    const float slW = valRight - 66.0f - slX;
    g_guiFont.drawString(cx, cy + 50.0f, sliderLabel,
                         applyAlpha(0xFFFFFFFF, alpha), 0.40f);

    float workingVal = sliderVal;
    bool changed = drawSlider(sliderId, slX, cy + 57.0f, slW, 8.0f,
                              workingVal, minVal, maxVal, mx, my, lClick, alpha);
    changed = drawNumericInput(sliderId, valRight - 60.0f, cy + 44.0f, 60.0f,
                               24.0f, workingVal, minVal, maxVal, 0, suffix,
                               mx, my, clickEvent, alpha) || changed;
    if (changed) {
      sliderSetter(workingVal);
    }

    cy += 110.0f;
  };

  if (s_moduleSearch.empty()) {
    g_guiFont.drawString(cx, cy, "Bedwars Features", applyAlpha(0xFFFFFFFF, alpha), 0.50f);
    cy += 30.0f;
  }

  if (shouldShowInSearch("Bedwars Tools", "Master switch for all Bedwars features")) {
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, 60.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, 60.0f, hCard, alpha, master);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Bedwars Tools",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    std::string detail = master
        ? (runtime.mapName.empty() ? "Active - Waiting for game" : "Active - Map: " + runtime.mapName)
        : "Master switch; controls all Bedwars overlays and alerts";
    g_guiFont.drawString(cx, cy + 18.0f, detail.c_str(),
                         applyAlpha(master ? 0xFF8FD6FF : 0xFFA0A0A5, alpha), 0.40f);

    glDisable(GL_TEXTURE_2D);
    drawSwitch(500, swX, cy + 10.0f, master, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hCard) {
      BwConfig::setMasterEnabled(!master);
    }
    cy += 75.0f;
  }

  if (s_moduleSearch.empty()) {
    cy += 8.0f;
    drawSectionLabel(cx, cy, "Reminders", alpha);
    cy += 30.0f;
  }

  drawToggleSliderCard(
      "No Trap Reminder",
      "Alerts and reminds you with audio/visual notifications when your bed has no queued trap",
      BwConfig::isModuleEnabled(Module::TrapNotifier), 548,
      [](bool enabled) { BwConfig::setModuleEnabled(Module::TrapNotifier, enabled); },
      "Reminder Interval",
      static_cast<float>(BwConfig::getTrapReminderSeconds()),
      15.0f, 300.0f, 703, "s",
      [](float val) { BwConfig::setTrapReminderSeconds(static_cast<int>(val + 0.5f)); }
  );

  if (s_moduleSearch.empty()) {
    cy += 8.0f;
    drawSectionLabel(cx, cy, "Overlays", alpha);
    cy += 30.0f;
  }

  {
    const HudId hid = HudId::EventTimer;
    auto layout = BwConfig::getHudLayout(hid);
    const bool enabled = layout.visible && BwConfig::isModuleEnabled(Module::EventTimers);
    drawToggleSliderCard(
        "Event Timers",
        "Displays countdown timers for Diamond and Emerald generator upgrades",
        enabled, 600,
        [hid, layout](bool en) mutable {
          layout.visible = en;
          BwConfig::setHudLayout(hid, layout);
          BwConfig::setModuleEnabled(Module::EventTimers, en);
        },
        "Scale", layout.scale * 100.0f, 50.0f, 200.0f, 732, "%",
        [hid, layout](float val) mutable {
          layout.scale = val / 100.0f;
          BwConfig::setHudLayout(hid, layout);
        }
    );
  }

  {
    const HudId hid = HudId::Resource;
    auto layout = BwConfig::getHudLayout(hid);
    const bool enabled = layout.visible && BwConfig::isModuleEnabled(Module::ResourceTracker);
    drawToggleSliderCard(
        "Resource Tracker",
        "Displays total team resources collected: Iron, Gold, Diamonds, and Emeralds",
        enabled, 601,
        [hid, layout](bool en) mutable {
          layout.visible = en;
          BwConfig::setHudLayout(hid, layout);
          BwConfig::setModuleEnabled(Module::ResourceTracker, en);
        },
        "Scale", layout.scale * 100.0f, 50.0f, 200.0f, 735, "%",
        [hid, layout](float val) mutable {
          layout.scale = val / 100.0f;
          BwConfig::setHudLayout(hid, layout);
        }
    );
  }

  {
    const HudId hid = HudId::Height;
    auto layout = BwConfig::getHudLayout(hid);
    const bool enabled = layout.visible && BwConfig::isModuleEnabled(Module::HeightOverlay);
    drawToggleSliderCard(
        "Height Overlay",
        "Displays your current Y height and remaining blocks to map build limit",
        enabled, 602,
        [hid, layout](bool en) mutable {
          layout.visible = en;
          BwConfig::setHudLayout(hid, layout);
          BwConfig::setModuleEnabled(Module::HeightOverlay, en);
        },
        "Scale", layout.scale * 100.0f, 50.0f, 200.0f, 738, "%",
        [hid, layout](float val) mutable {
          layout.scale = val / 100.0f;
          BwConfig::setHudLayout(hid, layout);
        }
    );
  }

  {
    const HudId hid = HudId::TeamState;
    auto layout = BwConfig::getHudLayout(hid);
    const bool enabled = layout.visible && BwConfig::isModuleEnabled(Module::UpgradeHud);
    drawToggleSliderCard(
        "Team Upgrades",
        "Shows active team upgrades (Sharpness, Protection tier, Forge, Haste)",
        enabled, 603,
        [hid, layout](bool en) mutable {
          layout.visible = en;
          BwConfig::setHudLayout(hid, layout);
          BwConfig::setModuleEnabled(Module::UpgradeHud, en);
        },
        "Scale", layout.scale * 100.0f, 50.0f, 200.0f, 741, "%",
        [hid, layout](float val) mutable {
          layout.scale = val / 100.0f;
          BwConfig::setHudLayout(hid, layout);
        }
    );
  }

  if (s_moduleSearch.empty()) {
    cy += 8.0f;
    drawSectionLabel(cx, cy, "Enemy Alerts", alpha);
    cy += 30.0f;
  }

  drawSimpleToggleCard(
      "Armor Alerts",
      "Warns when enemy players purchase Iron or Diamond armor upgrades",
      BwConfig::isModuleEnabled(Module::ArmorAlerts), 546,
      [](bool en) { BwConfig::setModuleEnabled(Module::ArmorAlerts, en); }
  );

  drawSimpleToggleCard(
      "Upgrade Alerts",
      "Alerts when enemy teams buy Sharpness or Protection",
      BwConfig::isModuleEnabled(Module::UpgradeAlerts), 543,
      [](bool en) { BwConfig::setModuleEnabled(Module::UpgradeAlerts, en); }
  );

  drawSimpleToggleCard(
      "Item Alerts",
      "Alerts when enemies hold or purchase Bows, Potions, or Knockback Sticks",
      BwConfig::isModuleEnabled(Module::ItemAlerts), 549,
      [](bool en) { BwConfig::setModuleEnabled(Module::ItemAlerts, en); }
  );

  drawSimpleToggleCard(
      "Consume Alerts",
      "Detects when nearby enemies drink Invisibility or Speed potions",
      BwConfig::isModuleEnabled(Module::ConsumeAlerts), 544,
      [](bool en) { BwConfig::setModuleEnabled(Module::ConsumeAlerts, en); }
  );

  drawSimpleToggleCard(
      "Pickup Alerts",
      "Alerts when enemies collect Diamonds or Emeralds from middle generators",
      BwConfig::isModuleEnabled(Module::PickupAlerts), 545,
      [](bool en) { BwConfig::setModuleEnabled(Module::PickupAlerts, en); }
  );

  drawSimpleToggleCard(
      "Resource Alerts",
      "Notifies in chat or overlay when you collect Iron, Gold, Diamonds, or Emeralds",
      BwConfig::isResourceAlertsEnabled(), 547,
      [](bool en) { BwConfig::setResourceAlertsEnabled(en); }
  );

  if (s_moduleSearch.empty()) {
    cy += 8.0f;
    drawSectionLabel(cx, cy, "Player Alert Settings", alpha);
    cy += 30.0f;
  }

  if (shouldShowInSearch("Visibility Mode", "Condition required to detect enemy players for alerts")) {
    const float cardH = 75.0f;
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, cardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Visibility Mode",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    g_guiFont.drawString(cx, cy + 18.0f, "Condition required to detect enemy players for alerts",
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);

    const char *vmLabels[] = {"Line of Sight", "Range Only", "Camera View"};
    const VisibilityMode vmValues[] = {VisibilityMode::LineOfSight, VisibilityMode::RangeOnly, VisibilityMode::CameraView};
    VisibilityMode curVm = settings.visibilityMode;
    float btnX = cx;
    float btnY = cy + 40.0f;
    float btnW = 100.0f;
    float btnH = 24.0f;
    for (int i = 0; i < 3; ++i) {
      bool hov = isHovered(mx, my, btnX, btnY, btnW, btnH);
      bool sel = (curVm == vmValues[i]);
      glDisable(GL_TEXTURE_2D);
      drawThemeButton(btnX, btnY, btnW, btnH, hov, sel, alpha);
      glEnable(GL_TEXTURE_2D);
      float tw = g_guiFont.getStringWidth(vmLabels[i]) * (0.38f / 0.5f);
      g_guiFont.drawString(btnX + (btnW - tw) * 0.5f, btnY + 5.0f, vmLabels[i],
                           applyAlpha(sel ? 0xFFFFFFFF : 0xFF8A8A92, alpha), 0.38f);
      if (clickEvent && hov) {
        BwConfig::setVisibilityMode(vmValues[i]);
      }
      btnX += btnW + 8.0f;
    }
    cy += cardH + 15.0f;
  }

  if (shouldShowInSearch("Alert Output", "Where detected enemy alerts are delivered")) {
    const float cardH = 75.0f;
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, cardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Alert Output",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    g_guiFont.drawString(cx, cy + 18.0f, "Where detected enemy alerts are delivered",
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);

    const char *aoLabels[] = {"Notification", "Chat", "Chat + Alert"};
    const AlertOutput aoValues[] = {AlertOutput::Overlay, AlertOutput::Chat, AlertOutput::Both};
    AlertOutput curAo = settings.alertOutput;
    float btnX = cx;
    float btnY = cy + 40.0f;
    float btnW = 100.0f;
    float btnH = 24.0f;
    for (int i = 0; i < 3; ++i) {
      bool hov = isHovered(mx, my, btnX, btnY, btnW, btnH);
      bool sel = (curAo == aoValues[i]);
      glDisable(GL_TEXTURE_2D);
      drawThemeButton(btnX, btnY, btnW, btnH, hov, sel, alpha);
      glEnable(GL_TEXTURE_2D);
      float tw = g_guiFont.getStringWidth(aoLabels[i]) * (0.38f / 0.5f);
      g_guiFont.drawString(btnX + (btnW - tw) * 0.5f, btnY + 5.0f, aoLabels[i],
                           applyAlpha(sel ? 0xFFFFFFFF : 0xFF8A8A92, alpha), 0.38f);
      if (clickEvent && hov) {
        BwConfig::setAlertOutput(aoValues[i]);
      }
      btnX += btnW + 8.0f;
    }
    cy += cardH + 15.0f;
  }

  drawSimpleToggleCard(
      "Show Distance",
      "Includes distance to player in chat alert messages (e.g. 18m, bold if <10m)",
      BwConfig::isAlertShowDistanceEnabled(), 508,
      [](bool en) { BwConfig::setAlertShowDistanceEnabled(en); }
  );

  if (shouldShowInSearch("Share to Game Chat", "Broadcast enemy alerts to party or guild chat")) {
    const bool shareOn = BwConfig::isAlertShareChatEnabled();
    const float cardH = shareOn ? 108.0f : 60.0f;
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, cardH);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, cardH, hCard, alpha, shareOn);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Share to Game Chat",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    g_guiFont.drawString(cx, cy + 18.0f, "Broadcast enemy alerts to party, guild, or all chat",
                         applyAlpha(0xFFA0A0A5, alpha), 0.40f);

    glDisable(GL_TEXTURE_2D);
    drawSwitch(509, swX, cy + 10.0f, shareOn, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && (isHovered(mx, my, swX - 5.0f, cy + 5.0f, 45.0f, 25.0f) ||
                       isHovered(mx, my, cardX, cy - 10.0f, cardW, 38.0f))) {
      BwConfig::setAlertShareChatEnabled(!shareOn);
    }

    if (shareOn) {
      g_guiFont.drawString(cx, cy + 44.0f, "Channel:", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      const char *channels[] = {"/pc", "/gc", "/ac"};
      int curCh = BwConfig::getAlertShareChannel();
      float chX = cx + 62.0f;
      float chY = cy + 40.0f;
      for (int i = 0; i < 3; ++i) {
        bool hov = isHovered(mx, my, chX, chY, 46.0f, 22.0f);
        bool sel = (curCh == i);
        glDisable(GL_TEXTURE_2D);
        drawThemeButton(chX, chY, 46.0f, 22.0f, hov, sel, alpha);
        glEnable(GL_TEXTURE_2D);
        float tw = g_guiFont.getStringWidth(channels[i]) * (0.36f / 0.5f);
        g_guiFont.drawString(chX + (46.0f - tw) * 0.5f, chY + 5.0f, channels[i],
                             applyAlpha(sel ? 0xFFFFFFFF : 0xFF8A8A92, alpha), 0.36f);
        if (clickEvent && hov) {
          BwConfig::setAlertShareChannel(i);
        }
        chX += 52.0f;
      }

      g_guiFont.drawString(cx, cy + 76.0f, "Share:", applyAlpha(0xFFA0A0A5, alpha), 0.38f);
      struct Cat { const char *name; bool on; };
      Cat cats[] = {
        {"Items", BwConfig::isAlertShareItemsEnabled()},
        {"KB Stick", BwConfig::isAlertShareKbStickEnabled()},
        {"Potions", BwConfig::isAlertSharePotionsEnabled()},
        {"Consumes", BwConfig::isAlertShareConsumesEnabled()},
        {"Armor", BwConfig::isAlertShareArmorEnabled()},
        {"Upgrades", BwConfig::isAlertShareUpgradesEnabled()}
      };
      float catX = cx + 52.0f;
      float catY = cy + 72.0f;
      for (int i = 0; i < 6; ++i) {
        float tw = g_guiFont.getStringWidth(cats[i].name) * (0.35f / 0.5f);
        float bw = tw + 14.0f;
        bool hov = isHovered(mx, my, catX, catY, bw, 22.0f);
        bool sel = cats[i].on;
        glDisable(GL_TEXTURE_2D);
        drawThemeButton(catX, catY, bw, 22.0f, hov, sel, alpha);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(catX + 7.0f, catY + 5.0f, cats[i].name,
                             applyAlpha(sel ? 0xFFFFFFFF : 0xFF8A8A92, alpha), 0.35f);
        if (clickEvent && hov) {
          switch (i) {
          case 0: BwConfig::setAlertShareItemsEnabled(!sel); break;
          case 1: BwConfig::setAlertShareKbStickEnabled(!sel); break;
          case 2: BwConfig::setAlertSharePotionsEnabled(!sel); break;
          case 3: BwConfig::setAlertShareConsumesEnabled(!sel); break;
          case 4: BwConfig::setAlertShareArmorEnabled(!sel); break;
          case 5: BwConfig::setAlertShareUpgradesEnabled(!sel); break;
          }
        }
        catX += bw + 6.0f;
      }
    }

    cy += cardH + 15.0f;
  }

  if (shouldShowInSearch("Alert Range", "Maximum scanning distance for enemies")) {
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, 60.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, 60.0f, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Alert Range",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    const float labelW = g_guiFont.getStringWidth("Alert Range") * (0.48f / 0.5f) + 16.0f;
    const float slX = cx + labelW + 40.0f;
    const float slW = valRight - 66.0f - slX;

    float workingRange = settings.playerAlertRange;
    bool changed = drawSlider(700, slX, cy + 18.0f, slW, 8.0f,
                              workingRange, 4.0f, 256.0f, mx, my, lClick, alpha);
    changed = drawNumericInput(700, valRight - 60.0f, cy + 5.0f, 60.0f,
                               24.0f, workingRange, 4.0f, 256.0f, 0, "m",
                               mx, my, clickEvent, alpha) || changed;
    if (changed) {
      BwConfig::setPlayerAlertRange(workingRange);
    }
    cy += 75.0f;
  }

  drawSimpleToggleCard(
      "Alert Sounds",
      "Play audio notifications alongside Bedwars enemy alerts and trap warnings",
      settings.sounds, 501,
      [](bool en) { BwConfig::setSoundsEnabled(en); }
  );

  drawSimpleToggleCard(
      "Ignore Teammates",
      "Exclude confirmed teammates from enemy equipment and consume detection",
      settings.ignoreOwnTeam, 506,
      [](bool en) { BwConfig::setIgnoreOwnTeam(en); }
  );

  if (shouldShowInSearch("Reset Bedwars Settings", "Restore default settings")) {
    const bool hCard = isHovered(mx, my, cardX, cy - 10.0f, cardW, 60.0f);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 10.0f, cardW, 60.0f, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Reset Bedwars Settings",
                         applyAlpha(0xFFFFFFFF, alpha), 0.44f);
    const char *desc = resetArmedAt != 0 && GetTickCount64() - resetArmedAt < 8000
        ? "Click again within 8 seconds to confirm reset"
        : "Restores all Bedwars tab settings and HUD layouts to defaults";
    g_guiFont.drawString(cx, cy + 18.0f, desc,
                         applyAlpha(resetArmedAt != 0 ? 0xFFFF7777 : 0xFFA0A0A5, alpha), 0.40f);

    const float btnW = 80.0f;
    const float btnH = 26.0f;
    const float btnX = swX - 45.0f;
    const float btnY = cy + 10.0f;
    const bool hBtn = isHovered(mx, my, btnX, btnY, btnW, btnH);

    glDisable(GL_TEXTURE_2D);
    drawThemeButton(btnX, btnY, btnW, btnH, hBtn, false, alpha);
    glEnable(GL_TEXTURE_2D);

    const char *btnText = resetArmedAt != 0 ? "Confirm" : "Reset";
    const float textW = g_guiFont.getStringWidth(btnText) * (0.42f / 0.5f);
    g_guiFont.drawString(btnX + (btnW - textW) * 0.5f, btnY + 6.0f, btnText,
                         applyAlpha(resetArmedAt != 0 ? 0xFFFF5555 : 0xFFFFFFFF, alpha), 0.42f);

    if (clickEvent && (hBtn || hCard)) {
      const ULONGLONG now = GetTickCount64();
      if (resetArmedAt != 0 && now - resetArmedAt < 8000) {
        BwConfig::save(BwConfig::Settings{});
        BwConfig::resetAllHudLayouts();
        resetArmedAt = 0;
        NotificationManager::getInstance()->add(
            "Bedwars", "Settings reset to defaults", NotificationType::Info);
      } else {
        resetArmedAt = now;
      }
    }
    cy += 75.0f;
  }

  if (s_moduleSearch.empty()) {
    renderCustomScripts(ctx, "Bedwars");
  }
}

} // namespace Render::Tabs
