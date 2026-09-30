#pragma once

#include "BedwarsCore.h"

#include <array>
#include <string>
#include <unordered_map>

namespace OVson::Bedwars::Configuration {

namespace Fixed {
inline constexpr bool kDynamicTimerColor = true;
inline constexpr bool kDynamicHeightColor = true;
inline constexpr bool kShortUpgradeLabels = true;
inline constexpr bool kStackedResourceAlerts = true;
inline constexpr int kPlayerAlertCooldownMs = 2500;
inline constexpr float kCameraViewDegrees = 100.0F;
inline constexpr float kDefaultNotificationSeconds = 3.0F;
inline constexpr float kImportantNotificationSeconds = 5.0F;
inline constexpr float kPlayerNotificationSeconds = 4.0F;
inline constexpr float kWarningNotificationSeconds = 5.0F;
inline constexpr int kMaximumVisibleNotifications = 5;
} // namespace Fixed

struct Settings {
  Settings();
  int formatVersion = 6;
  bool masterEnabled = false;
  std::array<bool, kModuleCount> modules{};
  bool debug = false;
  bool sounds = true;
  bool onlyNextEvent = true;
  bool ignoreOwnTeam = true;
  const HudLayout &hudLayout(HudId id) const;
  bool resourceHudVisible() const;
  bool resourceAlerts = false;
  std::array<bool, kResourceCount> resources = {true, true, true, true};
  std::array<bool, kImportantItemCount> itemAlerts = allItemAlertsEnabled();
  bool shopDuplicatePrevention = false;
  int heightLimitOverride = 0;
  float playerAlertRange = 32.0F;
  int trapReminderSeconds = 90;
  VisibilityMode visibilityMode = VisibilityMode::LineOfSight;
  AlertOutput alertOutput = AlertOutput::Overlay;
  HeightDisplay heightDisplay = HeightDisplay::Ratio;
  std::array<HudLayout, kHudCount> hud{};

  bool alertShowDistance = true;
  bool alertShareChat = false;
  int alertShareChannel = 0; // 0: /pc, 1: /gc, 2: /ac
  bool alertShareItems = true;
  bool alertShareKbStick = true;
  bool alertSharePotions = true;
  bool alertShareConsumes = true;
  bool alertShareArmor = true;
  bool alertShareUpgrades = true;

  bool shouldShareAlert(PlayerAlert::Kind kind) const {
    if (!alertShareChat) return false;
    switch (kind) {
    case PlayerAlert::Kind::Item: return alertShareItems;
    case PlayerAlert::Kind::KnockbackStick: return alertShareKbStick;
    case PlayerAlert::Kind::Potion: return alertSharePotions;
    case PlayerAlert::Kind::Consume: return alertShareConsumes;
    case PlayerAlert::Kind::Armor: return alertShareArmor;
    case PlayerAlert::Kind::Upgrade: return alertShareUpgrades;
    default: return false;
    }
  }

  bool enabled(Module module) const;
};

inline const char *alertShareChannelName(int ch) {
  switch (ch) {
  case 1: return "/gc";
  case 2: return "/ac";
  default: return "/pc";
  }
}

Settings get();
void initialize();
void reload();
void save(const Settings &settings);

bool isMasterEnabled();
void setMasterEnabled(bool enabled);
bool isModuleEnabled(Module module);
void setModuleEnabled(Module module, bool enabled);
bool isDebugEnabled();
void setDebugEnabled(bool enabled);
bool areSoundsEnabled();
void setSoundsEnabled(bool enabled);
bool isOnlyNextEvent();
void setOnlyNextEvent(bool enabled);
bool isIgnoringOwnTeam();
void setIgnoreOwnTeam(bool enabled);
bool isResourceHudEnabled();
void setResourceHudEnabled(bool enabled);
bool isResourceAlertsEnabled();
void setResourceAlertsEnabled(bool enabled);
bool isResourceEnabled(Resource resource);
void setResourceEnabled(Resource resource, bool enabled);
bool isItemAlertEnabled(ImportantItem item);
void setItemAlertEnabled(ImportantItem item, bool enabled);
float getTimerX();
void setTimerX(float value);
float getTimerY();
void setTimerY(float value);
float getTimerScale();
void setTimerScale(float value);
float getHeightX();
void setHeightX(float value);
float getHeightY();
void setHeightY(float value);
float getHeightScale();
void setHeightScale(float value);
float getPlayerAlertRange();
void setPlayerAlertRange(float range);
int getHeightLimitOverride();
void setHeightLimitOverride(int limit);
int getTrapReminderSeconds();
void setTrapReminderSeconds(int seconds);
VisibilityMode getVisibilityMode();
void setVisibilityMode(VisibilityMode mode);
AlertOutput getAlertOutput();
void setAlertOutput(AlertOutput output);
HeightDisplay getHeightDisplay();
void setHeightDisplay(HeightDisplay display);
HudLayout getHudLayout(HudId hud);
void setHudLayout(HudId hud, const HudLayout &layout);
void resetHudLayout(HudId hud);
void resetAllHudLayouts();

bool isAlertShowDistanceEnabled();
void setAlertShowDistanceEnabled(bool enabled);
bool isAlertShareChatEnabled();
void setAlertShareChatEnabled(bool enabled);
int getAlertShareChannel();
void setAlertShareChannel(int channel);
const char *getAlertShareChannelName();
bool isAlertShareItemsEnabled();
void setAlertShareItemsEnabled(bool enabled);
bool isAlertShareKbStickEnabled();
void setAlertShareKbStickEnabled(bool enabled);
bool isAlertSharePotionsEnabled();
void setAlertSharePotionsEnabled(bool enabled);
bool isAlertShareConsumesEnabled();
void setAlertShareConsumesEnabled(bool enabled);
bool isAlertShareArmorEnabled();
void setAlertShareArmorEnabled(bool enabled);
bool isAlertShareUpgradesEnabled();
void setAlertShareUpgradesEnabled(bool enabled);

std::string serialize(const Settings &settings);
Settings deserialize(const std::string &data);

} // namespace OVson::Bedwars::Configuration
