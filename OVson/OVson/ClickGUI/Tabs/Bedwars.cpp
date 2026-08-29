#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Tabs.h"

#include "../Helpers.h"
#include "../State.h"
#include "../Theme.h"
#include "../../Logic/Bedwars/BedwarsConfig.h"
#include "../../Logic/Bedwars/BedwarsCore.h"
#include "../../Logic/Bedwars/BedwarsRuntime.h"
#include "../../Render/BedwarsOverlay.h"

#include <gl/GL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace Render::Tabs {

void renderBedwars(TabCtx &ctx) {
  using namespace ClickGUIState;
  using namespace OVson::Bedwars;
  namespace BwConfig = OVson::Bedwars::Configuration;

  const float panelX = ctx.mainX + 190.0F;
  const float panelW = std::max(300.0F, g_w - 210.0F);
  // Rows are inset by 10 and their text by a further 14, so the switch is
  // inset from the row's right edge by the same 14. It used to sit at
  // panelW - 45, which left it one pixel inside the row border while the text
  // had fourteen -- the lopsided look in the screenshot.
  const float rowRight = panelX + panelW - 10.0F;
  const float switchX = rowRight - 14.0F - 34.0F;
  // Rows are inset from the group card so the group reads as a container
  // holding its rows, rather than as one more row of the same width.
  const float rowX = panelX + 10.0F;
  const float rowW = panelW - 20.0F;
  const float textX = rowX + 14.0F;
  float &cy = ctx.cy;
  const auto settings = BwConfig::get();
  const auto runtime = Runtime::instance().snapshot();
  const bool master = settings.masterEnabled;

  static std::array<bool, 6> expanded = {true,  true,  false,
                                         false, false, false};
  static ULONGLONG resetArmedAt = 0;

  // ---- Group ---------------------------------------------------------------
  // A collapsible group: a header band, then its rows drawn inset beneath it.
  // Only the header is clickable, so a click anywhere on a row below can never
  // collapse the group out from under the control the user was aiming at.
  auto group = [&](std::size_t id, const char *title, const char *caption) {
    const float y = cy - 8.0F;
    const bool hover = isHovered(ctx.mx, ctx.my, panelX, y, panelW, 44.0F);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(panelX, y, panelW, 44.0F, hover, ctx.alpha, expanded[id]);
    drawChevron(panelX + panelW - 26.0F, y + 22.0F, 5.0F, expanded[id],
                0xFF8FD6FF, ctx.alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(textX, cy, title,
                         applyAlpha(0xFFE8F3FF, ctx.alpha), 0.50F);
    g_guiFont.drawString(textX, cy + 17.0F, caption,
                         applyAlpha(0xFF707078, ctx.alpha), 0.34F);
    if (ctx.clickEvent && hover)
      expanded[id] = !expanded[id];
    cy += expanded[id] ? 50.0F : 44.0F;
    return expanded[id];
  };

  auto groupEnd = [&]() { cy += 12.0F; };

  auto toggleRow = [&](const char *title, const char *description, bool value,
                       int switchId, bool available, bool child, auto setter) {
    const float y = cy - 5.0F;
    const bool hover = isHovered(ctx.mx, ctx.my, rowX, y, rowW, 44.0F);
    const bool active = available && (!child || master);
    const float alpha = ctx.alpha * (active ? 1.0F : 0.45F);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(rowX, y, rowW, 44.0F, hover, ctx.alpha, value && active);
    // The row card spans [cy - 5, cy + 39], so its centre is cy + 17. A
    // switch is 18 tall, so it starts at cy + 8. It was drawn at cy + 3,
    // five pixels above centre, which is what made every switch look like it
    // was floating towards the title rather than sitting with the row.
    drawSwitch(switchId, switchX, cy + 8.0F, value, hover && available, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(textX, cy, title, applyAlpha(0xFFFFFFFF, alpha),
                         0.44F);
    std::string detail = description;
    if (child && !master)
      detail += "  -  master is off";
    else if (!available)
      detail += "  -  unavailable this session";
    g_guiFont.drawString(textX, cy + 16.0F, detail,
                         applyAlpha(0xFF8A8A92, alpha), 0.34F);
    // `active` and not just `available`: a row greyed out because the master
    // is off used to still toggle when clicked, so the control said "disabled"
    // and behaved otherwise.
    if (ctx.clickEvent && hover && active)
      setter(!value);
    cy += 48.0F;
  };

  auto sliderRow = [&](const std::string &title, float value, float minimum,
                       float maximum, int sliderId, const char *suffix,
                       auto setter) {
    const float y = cy - 5.0F;
    const bool hover = isHovered(ctx.mx, ctx.my, rowX, y, rowW, 44.0F);
    const float alpha = ctx.alpha * (master ? 1.0F : 0.45F);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(rowX, y, rowW, 44.0F, hover, ctx.alpha);
    float changedValue = value;
    // The readout sits at the right edge, so the track stops short of it
    // instead of running underneath the digits.
    bool changed = drawSlider(
        sliderId, textX + 140.0F, cy + 14.0F,
        std::max(70.0F, rowW - 225.0F), 6.0F, changedValue, minimum, maximum,
        ctx.mx, ctx.my, ctx.lClick, ctx.alpha);
    glEnable(GL_TEXTURE_2D);
    changed = drawNumericInput(sliderId, rowX + rowW - 66.0F, cy - 1.0F,
                               58.0F, 26.0F, changedValue, minimum, maximum,
                               0, suffix, ctx.mx, ctx.my, ctx.clickEvent,
                               alpha) || changed;
    g_guiFont.drawString(textX, cy + 2.0F, title,
                         applyAlpha(0xFFFFFFFF, alpha), 0.42F);
    if (changed)
      setter(changedValue);
    cy += 48.0F;
  };

  auto actionRow = [&](const std::string &title, const std::string &description,
                       bool available, auto action) {
    const float y = cy - 5.0F;
    const bool hover = isHovered(ctx.mx, ctx.my, rowX, y, rowW, 44.0F);
    const float alpha = ctx.alpha * (available ? 1.0F : 0.45F);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(rowX, y, rowW, 44.0F, hover && available, ctx.alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(textX, cy, title, applyAlpha(0xFFE7F3FF, alpha),
                         0.42F);
    g_guiFont.drawString(textX, cy + 16.0F, description,
                         applyAlpha(0xFF8A8A92, alpha), 0.34F);
    if (ctx.clickEvent && hover && available)
      action();
    cy += 48.0F;
  };

  // Every HUD panel is produced by exactly one module. Nothing else consumes
  // these four, so their module switch and their panel switch were two names
  // for one outcome -- and the runtime needed BOTH before a panel would draw.
  // Turning on "Resources" in the HUD group while "Resource Tracker" sat off
  // in Modules produced a panel that was visible, empty, and therefore never
  // drawn: `!panel.lines.empty()` is the last gate in the overlay. That is why
  // the panels appeared to do nothing in game.
  auto hudModule = [](HudId hud) {
    switch (hud) {
    case HudId::EventTimer: return Module::EventTimers;
    case HudId::Height: return Module::HeightOverlay;
    case HudId::Resource: return Module::ResourceTracker;
    case HudId::TeamState: return Module::UpgradeHud;
    default: return Module::EventTimers;
    }
  };

  // A row whose value cycles on click. actionRow was being used for these, and
  // it draws exactly like the rows that do nothing on click -- so "Range Only"
  // read as a label rather than as a control, and people never discovered they
  // could change it. The value now sits in a bordered chip in the accent
  // colour with a chevron after it, which is the same shape every other
  // clickable value in this client uses.
  auto cycleRow = [&](const char *title, const std::string &value,
                      const char *description, auto action) {
    const float y = cy - 5.0F;
    const bool hover = isHovered(ctx.mx, ctx.my, rowX, y, rowW, 44.0F);
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(rowX, y, rowW, 44.0F, hover, ctx.alpha);

    const float chipTextW = g_guiFont.getStringWidth(value) * (0.40F / 0.5F);
    const float chipW = chipTextW + 34.0F;
    const float chipH = 24.0F;
    const float chipX = rowRight - 14.0F - chipW;
    const float chipY = cy + 5.0F;
    drawThemeButton(chipX, chipY, chipW, chipH, hover, false, ctx.alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(textX, cy, title, applyAlpha(0xFFE7F3FF, ctx.alpha),
                         0.42F);
    g_guiFont.drawString(textX, cy + 16.0F, description,
                         applyAlpha(0xFF8A8A92, ctx.alpha), 0.34F);
    g_guiFont.drawString(chipX + 12.0F, chipY + 6.0F, value.c_str(),
                         applyAlpha(ClickGUITheme::accent(), ctx.alpha), 0.40F);
    // A right-pointing chevron. Helpers only ships the up/down one used by the
    // collapsible group headers, and "this cycles forward" is a different
    // statement from "this expands".
    glDisable(GL_TEXTURE_2D);
    {
      const DWORD tint = ClickGUITheme::accent();
      glColor4f(((tint >> 16) & 0xFF) / 255.0F, ((tint >> 8) & 0xFF) / 255.0F,
                (tint & 0xFF) / 255.0F, ctx.alpha);
      const float ax = chipX + chipW - 15.0F;
      const float ay = chipY + chipH * 0.5F;
      glBegin(GL_TRIANGLES);
      glVertex2f(ax, ay - 4.0F);
      glVertex2f(ax, ay + 4.0F);
      glVertex2f(ax + 5.0F, ay);
      glEnd();
    }
    glEnable(GL_TEXTURE_2D);

    if (ctx.clickEvent && hover)
      action();
    cy += 48.0F;
  };

  auto moduleRow = [&](Module module, const char *description) {
    toggleRow(moduleName(module), description,
              BwConfig::isModuleEnabled(module),
              540 + static_cast<int>(module), isModuleAvailable(module), true,
              [module](bool enabled) {
                BwConfig::setModuleEnabled(module, enabled);
              });
  };

  // ---- Header --------------------------------------------------------------
  drawSectionLabel(ctx.cx, cy, "Bedwars", ctx.alpha);
  cy += 30.0F;
  {
    const std::string status =
        runtime.lifecycleStatus.empty()
            ? (master ? std::string("Waiting for game state")
                      : std::string("Disabled"))
            : runtime.lifecycleStatus;
    const float y = cy - 6.0F;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(panelX, y, panelW, 40.0F, false, ctx.alpha, master);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(textX, cy, status,
                         applyAlpha(master ? 0xFF8FD6FF : 0xFF9A9AA0,
                                    ctx.alpha),
                         0.44F);
    const std::string map =
        runtime.mapName.empty() ? std::string("Map unknown")
                                : std::string("Map: ") + runtime.mapName;
    g_guiFont.drawString(textX, cy + 16.0F, map,
                         applyAlpha(0xFF707078, ctx.alpha), 0.34F);
    cy += 50.0F;
  }

  // ---- General -------------------------------------------------------------
  if (group(0, "General", "Master gate, sounds and reset")) {
    toggleRow("Bedwars Tools",
              "Master switch; per-module choices are kept while it is off",
              master, 500, true, false,
              [](bool enabled) { BwConfig::setMasterEnabled(enabled); });
    toggleRow("Alert Sounds", "Play a sound with Bedwars alerts",
              settings.sounds, 501, true, true,
              [](bool enabled) { BwConfig::setSoundsEnabled(enabled); });
    actionRow("Reset Bedwars Settings",
              resetArmedAt != 0 && GetTickCount64() - resetArmedAt < 8000
                  ? "Click again within 8 seconds to confirm"
                  : "Restores defaults for this tab only",
              true, [&]() {
                const ULONGLONG now = GetTickCount64();
                if (resetArmedAt != 0 && now - resetArmedAt < 8000) {
                  BwConfig::save(BwConfig::Settings{});
                  resetArmedAt = 0;
                } else {
                  resetArmedAt = now;
                }
              });
  }
  groupEnd();

  // ---- Modules -------------------------------------------------------------
  // The four HUD-backing modules are not listed here any more. They are the
  // HUD panels, and owning one outcome from two groups is what made this tab
  // confusing. They live in the HUD group now, one switch each.
  if (group(1, "Alerts To Watch For", "Things Bedwars tells you about")) {
    moduleRow(Module::ArmorAlerts, "Enemy armor upgrades");
    moduleRow(Module::UpgradeAlerts, "Enemy team Sharpness");
    moduleRow(Module::ItemAlerts, "Enemy potions, bows and knockback sticks");
    moduleRow(Module::ConsumeAlerts, "Enemies drinking or eating");
    moduleRow(Module::TrapNotifier, "Trap triggers and refill reminders");
    moduleRow(Module::PickupAlerts, "Resource pickups");
    moduleRow(Module::ShopHelper, "Not available yet");
  }
  groupEnd();

  // ---- Alerts --------------------------------------------------------------
  if (group(2, "Alerts", "Who counts as visible, and how far")) {
    toggleRow("Ignore Your Team",
              "Skip teammates after their team color is known",
              settings.ignoreOwnTeam, 506, true, true,
              [](bool enabled) { BwConfig::setIgnoreOwnTeam(enabled); });
    const char *visibility =
        settings.visibilityMode == VisibilityMode::RangeOnly
            ? "Range Only"
            : settings.visibilityMode == VisibilityMode::LineOfSight
                  ? "Line of Sight"
                  : "Camera View";
    cycleRow("Visibility", visibility,
             "Range, line of sight, or camera view", [&]() {
               const int next =
                   (static_cast<int>(BwConfig::getVisibilityMode()) + 1) % 3;
               BwConfig::setVisibilityMode(static_cast<VisibilityMode>(next));
             });
    cycleRow("Alert Output", alertOutputName(settings.alertOutput),
             "Overlay popups, chat lines, or both", [&]() {
               const int next =
                   (static_cast<int>(BwConfig::getAlertOutput()) + 1) % 3;
               BwConfig::setAlertOutput(static_cast<AlertOutput>(next));
             });
    sliderRow("Alert Range", settings.playerAlertRange, 4.0F, 256.0F, 700, "m",
              [](float value) { BwConfig::setPlayerAlertRange(value); });
    sliderRow("Trap Reminder",
              static_cast<float>(settings.trapReminderSeconds), 15.0F, 600.0F,
              703, "s", [](float value) {
                BwConfig::setTrapReminderSeconds(
                    static_cast<int>(value + 0.5F));
              });
    toggleRow("Only Next Event", "Show just the next event on the timer HUD",
              settings.onlyNextEvent, 505, true, true,
              [](bool enabled) { BwConfig::setOnlyNextEvent(enabled); });
    for (std::size_t i = 0; i < kResourceCount; ++i) {
      const Resource resource = static_cast<Resource>(i);
      const std::string title = std::string(resourceName(resource)) + " Alerts";
      toggleRow(title.c_str(), "Count this resource in totals and pickups",
                settings.resources[i], 620 + static_cast<int>(i), true, true,
                [resource](bool enabled) {
                  BwConfig::setResourceEnabled(resource, enabled);
                });
    }
  }
  groupEnd();

  // ---- Item Alerts ---------------------------------------------------------
  // One switch per item rather than one blanket toggle: which items are worth
  // interrupting you for is a matter of how you play, and the only person who
  // can answer that is the person reading the alerts. Item Alerts in the
  // Modules group above is still the master -- these only narrow it.
  if (group(5, "Item Alerts", "Which items are worth telling you about")) {
    for (std::size_t i = 1; i < kImportantItemCount; ++i) {
      const ImportantItem item = static_cast<ImportantItem>(i);
      const char *name = importantItemName(item);
      if (!name || !*name)
        continue;
      toggleRow(name, "Alert when an enemy has this", settings.itemAlerts[i],
                700 + static_cast<int>(i), true, true,
                [item](bool enabled) {
                  BwConfig::setItemAlertEnabled(item, enabled);
                });
    }
  }
  groupEnd();

  // ---- HUD -----------------------------------------------------------------
  if (group(3, "HUD", "Which panels show, where, and how big")) {
    // Position comes from dragging in layout mode, which writes the same x/y
    // a pair of percentage sliders used to; scale has no drag equivalent, so
    // it is the one thing still worth a slider here.
    toggleRow("Layout Mode", "Show every panel and drag it into place",
              Render::BedwarsOverlay::isLayoutMode(), 503, true, false,
              [](bool enabled) {
                Render::BedwarsOverlay::setLayoutMode(enabled);
              });
    for (std::size_t i = 0; i < kHudCount; ++i) {
      const HudId hud = static_cast<HudId>(i);
      const Module backing = hudModule(hud);
      HudLayout layout = BwConfig::getHudLayout(hud);
      // Shown only when BOTH are on, because that is exactly when the panel
      // actually appears. A switch that reads "on" over a panel you cannot
      // see is worse than no switch.
      const bool shown = layout.visible && BwConfig::isModuleEnabled(backing);
      std::string detail = "Show this panel";
      if (hud == HudId::Resource && shown &&
          std::none_of(settings.resources.begin(), settings.resources.end(),
                       [](bool on) { return on; }))
        detail = "Every resource is switched off under Alerts, so this stays empty";
      toggleRow(hudName(hud), detail.c_str(), shown,
                600 + static_cast<int>(i), true, true,
                [hud, backing, layout](bool enabled) mutable {
                  // One click, both switches. The panel and the module that
                  // fills it are the same feature.
                  layout.visible = enabled;
                  BwConfig::setHudLayout(hud, layout);
                  BwConfig::setModuleEnabled(backing, enabled);
                });
      if (shown) {
        sliderRow(std::string(hudName(hud)) + " Scale", layout.scale * 100.0F,
                  50.0F, 250.0F, 732 + static_cast<int>(i) * 3, "%",
                  [hud, layout](float value) mutable {
                    layout.scale = value / 100.0F;
                    BwConfig::setHudLayout(hud, layout);
                  });
      }
    }
    actionRow("Reset HUD Layout", "Restore default positions and scales", true,
              []() { BwConfig::resetAllHudLayouts(); });
  }
  groupEnd();

  // ---- Advanced ------------------------------------------------------------
  if (group(4, "Advanced", "Build limit override and diagnostics")) {
    const auto resolved = resolveMapHeight(runtime.mapName);
    actionRow("Detected Build Limit",
              resolved.maximumPlacementY
                  ? "Y " + std::to_string(*resolved.maximumPlacementY) +
                        " from the built-in map table"
                  : "Unknown map; set a manual limit below",
              false, []() {});
    cycleRow("Height Readout", heightDisplayName(settings.heightDisplay),
             "Y over the ceiling, blocks left, or the ceiling alone", [&]() {
               const int next =
                   (static_cast<int>(BwConfig::getHeightDisplay()) + 1) % 4;
               BwConfig::setHeightDisplay(static_cast<HeightDisplay>(next));
             });
    sliderRow("Manual Build Limit",
              static_cast<float>(settings.heightLimitOverride), 0.0F, 511.0F,
              770, "", [](float value) {
                BwConfig::setHeightLimitOverride(
                    static_cast<int>(value + 0.5F));
              });
    toggleRow("Debug Logging",
              "Rate-limited lifecycle, filter and queue reasons in the log",
              settings.debug, 508, true, false,
              [](bool enabled) { BwConfig::setDebugEnabled(enabled); });
  }
  groupEnd();
}

} // namespace Render::Tabs
