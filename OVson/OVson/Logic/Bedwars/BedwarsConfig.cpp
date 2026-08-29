#include "BedwarsConfig.h"

#include "../../Config/Config.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace OVson::Bedwars::Configuration {
namespace {

std::mutex g_mutex;
Settings g_settings;
bool g_initialized = false;

std::unordered_map<std::string, std::string> parsePairs(const std::string &data) {
  std::unordered_map<std::string, std::string> pairs;
  std::size_t start = 0;
  while (start < data.size() && pairs.size() < 512) {
    const std::size_t end = data.find(';', start);
    const std::string pair = data.substr(start, end - start);
    const std::size_t equals = pair.find('=');
    if (equals != std::string::npos && equals > 0)
      pairs.emplace(pair.substr(0, equals), pair.substr(equals + 1));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return pairs;
}

bool readBool(const std::unordered_map<std::string, std::string> &pairs,
              const char *key, bool fallback) {
  const auto it = pairs.find(key);
  if (it == pairs.end())
    return fallback;
  if (it->second == "1" || it->second == "true")
    return true;
  if (it->second == "0" || it->second == "false")
    return false;
  return fallback;
}

float readFloat(const std::unordered_map<std::string, std::string> &pairs,
                const char *key, float fallback, float minimum,
                float maximum) {
  const auto it = pairs.find(key);
  if (it == pairs.end())
    return fallback;
  try {
    std::size_t consumed = 0;
    const float value = std::stof(it->second, &consumed);
    return consumed == it->second.size() && std::isfinite(value)
               ? std::clamp(value, minimum, maximum)
               : fallback;
  } catch (...) {
    return fallback;
  }
}

int readInt(const std::unordered_map<std::string, std::string> &pairs,
            const char *key, int fallback, int minimum, int maximum) {
  const auto it = pairs.find(key);
  if (it == pairs.end())
    return fallback;
  try {
    std::size_t consumed = 0;
    const int value = std::stoi(it->second, &consumed);
    return consumed == it->second.size()
               ? std::clamp(value, minimum, maximum)
               : fallback;
  } catch (...) {
    return fallback;
  }
}

void ensureInitializedLocked() {
  if (g_initialized)
    return;
  g_settings = deserialize(Config::getBedwarsSettingsData());
  g_initialized = true;
}

template <typename Fn> void mutate(Fn &&fn) {
  Settings copy;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    ensureInitializedLocked();
    fn(g_settings);
    g_settings = deserialize(serialize(g_settings));
    copy = g_settings;
  }
  Config::setBedwarsSettingsData(serialize(copy));
}

} // namespace

Settings::Settings() {
  hud[static_cast<std::size_t>(HudId::EventTimer)] = {false, 0.02F, 0.18F, 1.0F};
  hud[static_cast<std::size_t>(HudId::Height)] = {false, 0.02F, 0.34F, 1.0F};
  hud[static_cast<std::size_t>(HudId::Resource)] = {false, 0.78F, 0.44F, 1.0F};
  hud[static_cast<std::size_t>(HudId::TeamState)] = {false, 0.78F, 0.18F, 1.0F};
}

bool Settings::enabled(Module module) const {
  const auto index = static_cast<std::size_t>(module);
  return masterEnabled && index < modules.size() && modules[index];
}

const HudLayout &Settings::hudLayout(HudId id) const {
  return hud[static_cast<std::size_t>(id)];
}

bool Settings::resourceHudVisible() const {
  return hud[static_cast<std::size_t>(HudId::Resource)].visible;
}

Settings get() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ensureInitializedLocked();
  return g_settings;
}

void initialize() {
  std::lock_guard<std::mutex> lock(g_mutex);
  ensureInitializedLocked();
}

void reload() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_settings = deserialize(Config::getBedwarsSettingsData());
  g_initialized = true;
}

void save(const Settings &settings) {
  const Settings sanitized = deserialize(serialize(settings));
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_settings = sanitized;
    g_initialized = true;
  }
  Config::setBedwarsSettingsData(serialize(sanitized));
}

bool isMasterEnabled() { return get().masterEnabled; }
void setMasterEnabled(bool enabled) {
  mutate([&](Settings &settings) { settings.masterEnabled = enabled; });
}
bool isModuleEnabled(Module module) {
  const Settings settings = get();
  const auto index = static_cast<std::size_t>(module);
  return index < settings.modules.size() && settings.modules[index];
}
void setModuleEnabled(Module module, bool enabled) {
  if (!isModuleAvailable(module))
    return;
  mutate([&](Settings &settings) {
    const auto index = static_cast<std::size_t>(module);
    if (index < settings.modules.size())
      settings.modules[index] = enabled;
  });
}
bool isDebugEnabled() { return get().debug; }
void setDebugEnabled(bool enabled) {
  mutate([&](Settings &settings) { settings.debug = enabled; });
}
bool areSoundsEnabled() { return get().sounds; }
void setSoundsEnabled(bool enabled) {
  mutate([&](Settings &settings) { settings.sounds = enabled; });
}
bool isOnlyNextEvent() { return get().onlyNextEvent; }
void setOnlyNextEvent(bool enabled) {
  mutate([&](Settings &settings) { settings.onlyNextEvent = enabled; });
}
bool isIgnoringOwnTeam() { return get().ignoreOwnTeam; }
void setIgnoreOwnTeam(bool enabled) {
  mutate([&](Settings &settings) { settings.ignoreOwnTeam = enabled; });
}
bool isResourceHudEnabled() {
  return get().hud[static_cast<std::size_t>(HudId::Resource)].visible;
}
void setResourceHudEnabled(bool enabled) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::Resource)].visible = enabled;
  });
}
bool isItemAlertEnabled(ImportantItem item) {
  const auto settings = get();
  const auto index = static_cast<std::size_t>(item);
  return index >= settings.itemAlerts.size() || settings.itemAlerts[index];
}
void setItemAlertEnabled(ImportantItem item, bool enabled) {
  mutate([&](Settings &settings) {
    const auto index = static_cast<std::size_t>(item);
    if (index < settings.itemAlerts.size())
      settings.itemAlerts[index] = enabled;
  });
}
bool isResourceEnabled(Resource resource) {
  const auto settings = get();
  const auto index = static_cast<std::size_t>(resource);
  return index < settings.resources.size() && settings.resources[index];
}
void setResourceEnabled(Resource resource, bool enabled) {
  mutate([&](Settings &settings) {
    const auto index = static_cast<std::size_t>(resource);
    if (index < settings.resources.size())
      settings.resources[index] = enabled;
  });
}
float getTimerX() {
  return get().hud[static_cast<std::size_t>(HudId::EventTimer)].x;
}
void setTimerX(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::EventTimer)].x =
        std::clamp(value, 0.0F, 1.0F);
  });
}
float getTimerY() {
  return get().hud[static_cast<std::size_t>(HudId::EventTimer)].y;
}
void setTimerY(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::EventTimer)].y =
        std::clamp(value, 0.0F, 1.0F);
  });
}
float getTimerScale() {
  return get().hud[static_cast<std::size_t>(HudId::EventTimer)].scale;
}
void setTimerScale(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::EventTimer)].scale =
        std::clamp(value, 0.5F, 2.5F);
  });
}
float getHeightX() {
  return get().hud[static_cast<std::size_t>(HudId::Height)].x;
}
void setHeightX(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::Height)].x =
        std::clamp(value, 0.0F, 1.0F);
  });
}
float getHeightY() {
  return get().hud[static_cast<std::size_t>(HudId::Height)].y;
}
void setHeightY(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::Height)].y =
        std::clamp(value, 0.0F, 1.0F);
  });
}
float getHeightScale() {
  return get().hud[static_cast<std::size_t>(HudId::Height)].scale;
}
void setHeightScale(float value) {
  mutate([&](Settings &settings) {
    settings.hud[static_cast<std::size_t>(HudId::Height)].scale =
        std::clamp(value, 0.5F, 2.5F);
  });
}
float getPlayerAlertRange() { return get().playerAlertRange; }
void setPlayerAlertRange(float range) {
  mutate([&](Settings &settings) {
    settings.playerAlertRange = std::clamp(range, 4.0F, 256.0F);
  });
}
int getHeightLimitOverride() { return get().heightLimitOverride; }
void setHeightLimitOverride(int limit) {
  mutate([&](Settings &settings) {
    settings.heightLimitOverride = std::clamp(limit, 0, 512);
  });
}
int getTrapReminderSeconds() { return get().trapReminderSeconds; }
void setTrapReminderSeconds(int seconds) {
  mutate([&](Settings &settings) {
    settings.trapReminderSeconds = std::clamp(seconds, 15, 600);
  });
}

AlertOutput getAlertOutput() { return get().alertOutput; }
void setAlertOutput(AlertOutput output) {
  mutate([&](Settings &settings) {
    const auto value = static_cast<std::size_t>(output);
    settings.alertOutput =
        value <= static_cast<std::size_t>(AlertOutput::Both)
            ? output
            : AlertOutput::Overlay;
  });
}

HeightDisplay getHeightDisplay() { return get().heightDisplay; }
void setHeightDisplay(HeightDisplay display) {
  mutate([&](Settings &settings) {
    const auto value = static_cast<std::size_t>(display);
    settings.heightDisplay =
        value <= static_cast<std::size_t>(HeightDisplay::Limit)
            ? display
            : HeightDisplay::Ratio;
  });
}

VisibilityMode getVisibilityMode() { return get().visibilityMode; }
void setVisibilityMode(VisibilityMode mode) {
  mutate([&](Settings &settings) {
    const auto value = static_cast<std::size_t>(mode);
    settings.visibilityMode =
        value <= static_cast<std::size_t>(VisibilityMode::CameraView)
            ? mode
            : VisibilityMode::LineOfSight;
  });
}
HudLayout getHudLayout(HudId hud) {
  const auto settings = get();
  const auto index = static_cast<std::size_t>(hud);
  return index < settings.hud.size() ? settings.hud[index] : HudLayout{};
}
void setHudLayout(HudId hud, const HudLayout &layout) {
  mutate([&](Settings &settings) {
    const auto index = static_cast<std::size_t>(hud);
    if (index < settings.hud.size())
      settings.hud[index] = sanitizeHudLayout(layout, 0.04F, 0.04F);
  });
}
void resetHudLayout(HudId hud) {
  const Settings defaults;
  const auto index = static_cast<std::size_t>(hud);
  if (index < defaults.hud.size())
    setHudLayout(hud, defaults.hud[index]);
}
void resetAllHudLayouts() {
  const Settings defaults;
  mutate([&](Settings &settings) { settings.hud = defaults.hud; });
}

std::string serialize(const Settings &settings) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3);
  out << "version=6;master=" << settings.masterEnabled << ';';
  for (std::size_t i = 0; i < kModuleCount; ++i)
    out << moduleKey(static_cast<Module>(i)) << '=' << settings.modules[i] << ';';
  out << "debug=" << settings.debug << ";sounds=" << settings.sounds
      << ";onlyNextEvent=" << settings.onlyNextEvent
      << ";ignoreOwnTeam=" << settings.ignoreOwnTeam
      << ";resourceHud=" << settings.resourceHudVisible()
      << ";resourceIron=" << settings.resources[0]
      << ";resourceGold=" << settings.resources[1]
      << ";resourceDiamond=" << settings.resources[2]
      << ";resourceEmerald=" << settings.resources[3]
      << ";shopDuplicatePrevention=" << settings.shopDuplicatePrevention
      << ";timerX=" << settings.hudLayout(HudId::EventTimer).x
      << ";timerY=" << settings.hudLayout(HudId::EventTimer).y
      << ";timerScale=" << settings.hudLayout(HudId::EventTimer).scale
      << ";heightX=" << settings.hudLayout(HudId::Height).x
      << ";heightY=" << settings.hudLayout(HudId::Height).y
      << ";heightScale=" << settings.hudLayout(HudId::Height).scale
      << ";heightLimitOverride=" << settings.heightLimitOverride
      << ";playerAlertRange=" << settings.playerAlertRange
      << ";trapReminderSeconds=" << settings.trapReminderSeconds
      << ";visibilityMode=" << static_cast<int>(settings.visibilityMode)
      << ";alertOutput=" << static_cast<int>(settings.alertOutput)
      << ";heightDisplay=" << static_cast<int>(settings.heightDisplay)
      << ';';
  for (std::size_t i = 1; i < kImportantItemCount; ++i) {
    const char *key = importantItemKey(static_cast<ImportantItem>(i));
    if (key && *key)
      out << "itemAlert_" << key << '=' << settings.itemAlerts[i] << ';';
  }
  for (std::size_t i = 0; i < kHudCount; ++i) {
    const auto &hud = settings.hud[i];
    out << "hud" << i << "Visible=" << hud.visible << ";hud" << i
        << "X=" << hud.x << ";hud" << i << "Y=" << hud.y << ";hud" << i
        << "Scale=" << hud.scale << ';';
  }
  return out.str();
}

Settings deserialize(const std::string &data) {
  Settings settings;
  const auto pairs = parsePairs(data);
  // An empty file is a fresh current configuration. Only persisted legacy data
  // without a version marker should receive the v1 compatibility defaults.
  const int version = data.empty() ? 6 : readInt(pairs, "version", 1, 1, 6);
  settings.formatVersion = 6;
  settings.masterEnabled = readBool(pairs, "master", false);
  for (std::size_t i = 0; i < kModuleCount; ++i)
    settings.modules[i] =
        isModuleAvailable(static_cast<Module>(i))
            ? readBool(pairs, moduleKey(static_cast<Module>(i)), false)
            : false;
  settings.debug = readBool(pairs, "debug", false);
  settings.sounds = readBool(pairs, "sounds", true);
  settings.onlyNextEvent = readBool(pairs, "onlyNextEvent", true);
  settings.ignoreOwnTeam = readBool(pairs, "ignoreOwnTeam", true);
  const bool legacyResourceHud = readBool(pairs, "resourceHud", false);
  settings.resources[0] = readBool(pairs, "resourceIron", true);
  settings.resources[1] = readBool(pairs, "resourceGold", true);
  settings.resources[2] = readBool(pairs, "resourceDiamond", true);
  settings.resources[3] = readBool(pairs, "resourceEmerald", true);
  settings.shopDuplicatePrevention =
      readBool(pairs, "shopDuplicatePrevention", false);
  // Absent key means enabled, so a config written before an item existed
  // does not silently mute it.
  for (std::size_t i = 1; i < kImportantItemCount; ++i) {
    const char *key = importantItemKey(static_cast<ImportantItem>(i));
    if (!key || !*key)
      continue;
    settings.itemAlerts[i] =
        readBool(pairs, (std::string("itemAlert_") + key).c_str(), true);
  }
  // v1 kept the timer/height placement in their own top-level keys. From v2
  // on the hud[] block below is the only place they live, so these are read
  // into locals purely to migrate an old file.
  const HudLayout legacyTimer{
      false, readFloat(pairs, "timerX", 0.02F, 0.0F, 1.0F),
      readFloat(pairs, "timerY", 0.20F, 0.0F, 1.0F),
      readFloat(pairs, "timerScale", 1.0F, 0.5F, 2.5F)};
  const HudLayout legacyHeight{
      false, readFloat(pairs, "heightX", 0.02F, 0.0F, 1.0F),
      readFloat(pairs, "heightY", 0.45F, 0.0F, 1.0F),
      readFloat(pairs, "heightScale", 1.0F, 0.5F, 2.5F)};
  settings.heightLimitOverride =
      readInt(pairs, "heightLimitOverride", 0, 0, 512);
  settings.playerAlertRange =
      readFloat(pairs, "playerAlertRange", 32.0F, 4.0F, 256.0F);
  settings.trapReminderSeconds =
      readInt(pairs, "trapReminderSeconds", 90, 15, 600);
  settings.visibilityMode = static_cast<VisibilityMode>(readInt(
      pairs, "visibilityMode",
      version == 1 ? static_cast<int>(VisibilityMode::RangeOnly)
                   : static_cast<int>(VisibilityMode::LineOfSight),
      static_cast<int>(VisibilityMode::RangeOnly),
      static_cast<int>(VisibilityMode::CameraView)));
  settings.alertOutput = static_cast<AlertOutput>(
      readInt(pairs, "alertOutput", static_cast<int>(AlertOutput::Overlay),
              static_cast<int>(AlertOutput::Overlay),
              static_cast<int>(AlertOutput::Both)));
  settings.heightDisplay = static_cast<HeightDisplay>(
      readInt(pairs, "heightDisplay", static_cast<int>(HeightDisplay::Ratio),
              static_cast<int>(HeightDisplay::Ratio),
              static_cast<int>(HeightDisplay::Limit)));
  for (std::size_t i = 0; i < kHudCount; ++i) {
    const std::string prefix = "hud" + std::to_string(i);
    settings.hud[i].visible =
        readBool(pairs, (prefix + "Visible").c_str(), settings.hud[i].visible);
    settings.hud[i].x =
        readFloat(pairs, (prefix + "X").c_str(), settings.hud[i].x, 0.0F, 1.0F);
    settings.hud[i].y =
        readFloat(pairs, (prefix + "Y").c_str(), settings.hud[i].y, 0.0F, 1.0F);
    settings.hud[i].scale = readFloat(pairs, (prefix + "Scale").c_str(),
                                      settings.hud[i].scale, 0.5F, 2.5F);
  }
  if (version == 1) {
    settings.hud[static_cast<std::size_t>(HudId::EventTimer)] = {
        settings.modules[static_cast<std::size_t>(Module::EventTimers)],
        legacyTimer.x, legacyTimer.y, legacyTimer.scale};
    settings.hud[static_cast<std::size_t>(HudId::Height)] = {
        settings.modules[static_cast<std::size_t>(Module::HeightOverlay)],
        legacyHeight.x, legacyHeight.y, legacyHeight.scale};
    settings.hud[static_cast<std::size_t>(HudId::Resource)].visible =
        legacyResourceHud &&
        settings.modules[static_cast<std::size_t>(Module::ResourceTracker)];
    settings.hud[static_cast<std::size_t>(HudId::TeamState)].visible =
        settings.modules[static_cast<std::size_t>(Module::UpgradeHud)];
  }
  return settings;
}

} // namespace OVson::Bedwars::Configuration
