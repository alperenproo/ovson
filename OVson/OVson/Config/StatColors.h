#pragma once
#include <cfloat>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace StatColors {

enum class StatType {
  Star,
  FinalKills,
  FKDR,
  Kills,
  KDR,
  Beds,
  BLR,
  Wins,
  WLR,
  WS,
  Ping,
  COUNT // sentinel
};

struct ColorRange {
  double minVal;
  double maxVal;  // DBL_MAX = infinity
  uint32_t color; // 0xAARRGGBB
};

struct StatColorConfig {
  StatType type;
  std::string name;
  std::vector<ColorRange> ranges;
};

void initialize();

uint32_t getColor(StatType type, double value);
const char *getMcColor(StatType type, double value);
const char *getStatName(StatType type);
std::vector<StatColorConfig> &getAllConfigs();
StatColorConfig &getConfig(StatType type);
bool addRange(StatType type, double minVal, double maxVal, uint32_t color);
bool updateRange(StatType type, int index, double minVal, double maxVal,
                 uint32_t color);
void removeRange(StatType type, int index);
void resetToDefaults(StatType type);
void resetAllToDefaults();
std::string serializeToJson();
void deserializeFromJson(const std::string &json);
const char *rgbToMcColor(uint32_t color);
} // namespace StatColors
