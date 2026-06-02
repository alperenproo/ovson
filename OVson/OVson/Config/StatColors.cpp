#include "StatColors.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace StatColors {

static std::vector<StatColorConfig> g_configs;
static bool g_initialized = false;

struct McColorEntry {
  const char *code;
  uint8_t r, g, b;
};

static const McColorEntry MC_COLORS[] = {
    {"§0", 0, 0, 0},       // Black
    {"§1", 0, 0, 170},     // Dark Blue
    {"§2", 0, 170, 0},     // Dark Green
    {"§3", 0, 170, 170},   // Dark Aqua
    {"§4", 170, 0, 0},     // Dark Red
    {"§5", 170, 0, 170},   // Dark Purple
    {"§6", 255, 170, 0},   // Gold
    {"§7", 170, 170, 170}, // Gray
    {"§8", 85, 85, 85},    // Dark Gray
    {"§9", 85, 85, 255},   // Blue
    {"§a", 85, 255, 85},   // Green
    {"§b", 85, 255, 255},  // Aqua
    {"§c", 255, 85, 85},   // Red
    {"§d", 255, 85, 255},  // Light Purple
    {"§e", 255, 255, 85},  // Yellow
    {"§f", 255, 255, 255}, // White
};
static const int MC_COLOR_COUNT = sizeof(MC_COLORS) / sizeof(MC_COLORS[0]);

static void loadDefaults() {
  g_configs.clear();
  g_configs.resize((int)StatType::COUNT);

  auto &star = g_configs[(int)StatType::Star];
  star.type = StatType::Star;
  star.name = "Star";
  star.ranges = {
      {0, 100, 0xFFAAAAAA},       {100, 200, 0xFFFFFFFF},
      {200, 300, 0xFFFFAA00},     {300, 400, 0xFF55FFFF},
      {400, 500, 0xFF55FF55},     {500, 600, 0xFF55FFFF},
      {600, DBL_MAX, 0xFFFF5555},
  };

  auto &fk = g_configs[(int)StatType::FinalKills];
  fk.type = StatType::FinalKills;
  fk.name = "FK";
  fk.ranges = {
      {0, 1000, 0xFFAAAAAA},     {1000, 2000, 0xFFFFFFFF},
      {2000, 4000, 0xFFFFAA00},  {4000, 5000, 0xFF55FFFF},
      {5000, 10000, 0xFFFF5555}, {10000, DBL_MAX, 0xFFAA00AA},
  };

  auto &fkdr = g_configs[(int)StatType::FKDR];
  fkdr.type = StatType::FKDR;
  fkdr.name = "FKDR";
  fkdr.ranges = {
      {0, 1, 0xFFAAAAAA},       {1, 2, 0xFFFFFFFF}, {2, 3, 0xFFFFAA00},
      {3, 4, 0xFF55FFFF},       {4, 5, 0xFF55FF55}, {5, 6, 0xFFAA00AA},
      {6, DBL_MAX, 0xFFFF5555},
  };

  auto &kills = g_configs[(int)StatType::Kills];
  kills.type = StatType::Kills;
  kills.name = "Kills";
  kills.ranges = {
      {0, 1000, 0xFFAAAAAA},        {1000, 3000, 0xFFFFFFFF},
      {3000, 6000, 0xFFFFAA00},     {6000, 10000, 0xFF55FFFF},
      {10000, DBL_MAX, 0xFFFF5555},
  };

  auto &kdr = g_configs[(int)StatType::KDR];
  kdr.type = StatType::KDR;
  kdr.name = "KDR";
  kdr.ranges = {
      {0, 1, 0xFFAAAAAA}, {1, 2, 0xFFFFFFFF},       {2, 3, 0xFFFFAA00},
      {3, 5, 0xFF55FFFF}, {5, DBL_MAX, 0xFFFF5555},
  };

  auto &beds = g_configs[(int)StatType::Beds];
  beds.type = StatType::Beds;
  beds.name = "Beds";
  beds.ranges = {
      {0, 500, 0xFFAAAAAA},        {500, 1000, 0xFFFFFFFF},
      {1000, 2000, 0xFFFFAA00},    {2000, 4000, 0xFF55FFFF},
      {4000, DBL_MAX, 0xFFFF5555},
  };

  auto &blr = g_configs[(int)StatType::BLR];
  blr.type = StatType::BLR;
  blr.name = "BLR";
  blr.ranges = {
      {0, 1, 0xFFAAAAAA}, {1, 2, 0xFFFFFFFF},       {2, 3, 0xFFFFAA00},
      {3, 5, 0xFF55FFFF}, {5, DBL_MAX, 0xFFFF5555},
  };

  auto &wins = g_configs[(int)StatType::Wins];
  wins.type = StatType::Wins;
  wins.name = "Wins";
  wins.ranges = {
      {0, 500, 0xFFAAAAAA},        {500, 1000, 0xFFFFFFFF},
      {1000, 2000, 0xFFFFFF55},    {2000, 4000, 0xFFFF5555},
      {4000, DBL_MAX, 0xFFAA00AA},
  };

  auto &wlr = g_configs[(int)StatType::WLR];
  wlr.type = StatType::WLR;
  wlr.name = "WLR";
  wlr.ranges = {
      {0, 1, 0xFFFFFFFF},
      {1, 3, 0xFFFFAA00},
      {3, 5, 0xFFFF5555},
      {5, DBL_MAX, 0xFFAA00AA},
  };

  auto &ws = g_configs[(int)StatType::WS];
  ws.type = StatType::WS;
  ws.name = "WS";
  ws.ranges = {
      {0, 3, 0xFFAAAAAA},         {3, 5, 0xFFFFFFFF},   {5, 10, 0xFFFFAA00},
      {10, 20, 0xFF55FFFF},       {20, 50, 0xFF55FF55}, {50, 100, 0xFFAA00AA},
      {100, DBL_MAX, 0xFFFF5555},
  };

  auto &ping = g_configs[(int)StatType::Ping];
  ping.type = StatType::Ping;
  ping.name = "Ping";
  ping.ranges = {
      {0, 80, 0xFF55FF55},
      {80, 150, 0xFFFFFF55},
      {150, 220, 0xFFFFAA00},
      {220, DBL_MAX, 0xFFFF5555},
  };
}

void initialize() {
  if (g_initialized)
    return;
  loadDefaults();
  g_initialized = true;
}

uint32_t getColor(StatType type, double value) {
  if (!g_initialized)
    initialize();
  int idx = (int)type;
  if (idx < 0 || idx >= (int)g_configs.size())
    return 0xFFFFFFFF;
  const auto &ranges = g_configs[idx].ranges;
  for (const auto &r : ranges) {
    if (value >= r.minVal && value < r.maxVal)
      return r.color;
  }
  if (!ranges.empty())
    return ranges.back().color;
  return 0xFFFFFFFF;
}

const char *rgbToMcColor(uint32_t color) {
  uint8_t cr = (color >> 16) & 0xFF;
  uint8_t cg = (color >> 8) & 0xFF;
  uint8_t cb = color & 0xFF;

  int bestDist = INT_MAX;
  int bestIdx = 15; // default white
  for (int i = 0; i < MC_COLOR_COUNT; ++i) {
    int dr = (int)cr - MC_COLORS[i].r;
    int dg = (int)cg - MC_COLORS[i].g;
    int db = (int)cb - MC_COLORS[i].b;
    int dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      bestIdx = i;
    }
  }
  return MC_COLORS[bestIdx].code;
}

static const char *MC_RAW_CODES[] = {
    "\xC2\xA7"
    "0",
    "\xC2\xA7"
    "1",
    "\xC2\xA7"
    "2",
    "\xC2\xA7"
    "3",
    "\xC2\xA7"
    "4",
    "\xC2\xA7"
    "5",
    "\xC2\xA7"
    "6",
    "\xC2\xA7"
    "7",
    "\xC2\xA7"
    "8",
    "\xC2\xA7"
    "9",
    "\xC2\xA7"
    "a",
    "\xC2\xA7"
    "b",
    "\xC2\xA7"
    "c",
    "\xC2\xA7"
    "d",
    "\xC2\xA7"
    "e",
    "\xC2\xA7"
    "f",
};

const char *getMcColor(StatType type, double value) {
  if (!g_initialized)
    initialize();
  uint32_t color = getColor(type, value);

  uint8_t cr = (color >> 16) & 0xFF;
  uint8_t cg = (color >> 8) & 0xFF;
  uint8_t cb = color & 0xFF;

  int bestDist = INT_MAX;
  int bestIdx = 15;
  for (int i = 0; i < MC_COLOR_COUNT; ++i) {
    int dr = (int)cr - MC_COLORS[i].r;
    int dg = (int)cg - MC_COLORS[i].g;
    int db = (int)cb - MC_COLORS[i].b;
    int dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      bestIdx = i;
    }
  }
  return MC_RAW_CODES[bestIdx];
}

const char *getStatName(StatType type) {
  switch (type) {
  case StatType::Star:
    return "Star";
  case StatType::FinalKills:
    return "FK";
  case StatType::FKDR:
    return "FKDR";
  case StatType::Kills:
    return "Kills";
  case StatType::KDR:
    return "KDR";
  case StatType::Beds:
    return "Beds";
  case StatType::BLR:
    return "BLR";
  case StatType::Wins:
    return "Wins";
  case StatType::WLR:
    return "WLR";
  case StatType::WS:
    return "WS";
  case StatType::Ping:
    return "Ping";
  default:
    return "???";
  }
}

std::vector<StatColorConfig> &getAllConfigs() {
  if (!g_initialized)
    initialize();
  return g_configs;
}

StatColorConfig &getConfig(StatType type) {
  if (!g_initialized)
    initialize();
  return g_configs[(int)type];
}

bool addRange(StatType type, double minVal, double maxVal, uint32_t color) {
  if (minVal >= maxVal)
    return false;
  auto &cfg = getConfig(type);

  for (const auto &r : cfg.ranges) {
    if (minVal < r.maxVal && maxVal > r.minVal)
      return false; // !!!!!
  }

  cfg.ranges.push_back({minVal, maxVal, color});
  std::sort(cfg.ranges.begin(), cfg.ranges.end(),
            [](const ColorRange &a, const ColorRange &b) {
              return a.minVal < b.minVal;
            });
  return true;
}

bool updateRange(StatType type, int index, double minVal, double maxVal,
                 uint32_t color) {
  if (minVal >= maxVal)
    return false;
  auto &cfg = getConfig(type);
  if (index < 0 || index >= (int)cfg.ranges.size())
    return false;

  for (int i = 0; i < (int)cfg.ranges.size(); ++i) {
    if (i == index)
      continue;
    const auto &r = cfg.ranges[i];
    if (minVal < r.maxVal && maxVal > r.minVal)
      return false; // !!!!!!!!
  }

  cfg.ranges[index] = {minVal, maxVal, color};
  std::sort(cfg.ranges.begin(), cfg.ranges.end(),
            [](const ColorRange &a, const ColorRange &b) {
              return a.minVal < b.minVal;
            });
  return true;
}

void removeRange(StatType type, int index) {
  auto &cfg = getConfig(type);
  if (index >= 0 && index < (int)cfg.ranges.size())
    cfg.ranges.erase(cfg.ranges.begin() + index);
}

void resetToDefaults(StatType type) {
  StatColorConfig backup;
  auto oldConfigs = g_configs;
  loadDefaults();
  auto newCfg = g_configs[(int)type];
  g_configs = oldConfigs;
  g_configs[(int)type] = newCfg;
}

void resetAllToDefaults() { loadDefaults(); }

std::string serializeToJson() {
  std::string result = "[\n";
  for (int i = 0; i < (int)g_configs.size(); ++i) {
    const auto &cfg = g_configs[i];
    result += "  {\"name\":\"" + cfg.name + "\",\"ranges\":[";
    for (int j = 0; j < (int)cfg.ranges.size(); ++j) {
      const auto &r = cfg.ranges[j];
      char buf[128];
      if (r.maxVal >= 1e300)
        snprintf(buf, sizeof(buf), "{\"min\":%.2f,\"max\":-1,\"color\":%u}",
                 r.minVal, r.color);
      else
        snprintf(buf, sizeof(buf), "{\"min\":%.2f,\"max\":%.2f,\"color\":%u}",
                 r.minVal, r.maxVal, r.color);
      result += buf;
      if (j + 1 < (int)cfg.ranges.size())
        result += ",";
    }
    result += "]}";
    if (i + 1 < (int)g_configs.size())
      result += ",";
    result += "\n";
  }
  result += "]";
  return result;
}

static bool parseNextNumber(const std::string &s, size_t &pos, double &out) {
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == ':' || s[pos] == ','))
    pos++;
  size_t start = pos;
  bool hasDot = false;
  bool hasSign = false;
  if (pos < s.size() && s[pos] == '-') {
    pos++;
    hasSign = true;
  }
  while (pos < s.size() &&
         ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.')) {
    if (s[pos] == '.')
      hasDot = true;
    pos++;
  }
  if (pos == start || (hasSign && pos == start + 1))
    return false;
  out = atof(s.substr(start, pos - start).c_str());
  return true;
}

void deserializeFromJson(const std::string &json) {
  if (json.empty())
    return;

  size_t configIdx = 0;
  size_t pos = 0;

  while (pos < json.size() && configIdx < g_configs.size()) {
    size_t rangesKey = json.find("\"ranges\"", pos);
    if (rangesKey == std::string::npos)
      break;

    size_t arrStart = json.find('[', rangesKey);
    if (arrStart == std::string::npos)
      break;
    size_t arrEnd = json.find(']', arrStart);
    if (arrEnd == std::string::npos)
      break;

    std::string rangesStr = json.substr(arrStart, arrEnd - arrStart + 1);

    g_configs[configIdx].ranges.clear();

    size_t rp = 0;
    while (rp < rangesStr.size()) {
      size_t objStart = rangesStr.find('{', rp);
      if (objStart == std::string::npos)
        break;
      size_t objEnd = rangesStr.find('}', objStart);
      if (objEnd == std::string::npos)
        break;

      std::string obj = rangesStr.substr(objStart, objEnd - objStart + 1);

      ColorRange range = {0, DBL_MAX, 0xFFFFFFFF};

      size_t minP = obj.find("\"min\"");
      if (minP != std::string::npos) {
        size_t p = minP + 5;
        parseNextNumber(obj, p, range.minVal);
      }
      size_t maxP = obj.find("\"max\"");
      if (maxP != std::string::npos) {
        size_t p = maxP + 5;
        double maxV;
        if (parseNextNumber(obj, p, maxV)) {
          range.maxVal = (maxV < 0) ? DBL_MAX : maxV;
        }
      }
      size_t colP = obj.find("\"color\"");
      if (colP != std::string::npos) {
        size_t p = colP + 7;
        double colV;
        if (parseNextNumber(obj, p, colV))
          range.color = (uint32_t)colV;
      }

      g_configs[configIdx].ranges.push_back(range);
      rp = objEnd + 1;
    }

    pos = arrEnd + 1;
    configIdx++;
  }
}

} // namespace StatColors
