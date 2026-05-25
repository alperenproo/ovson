#include "Config.h"
#include "../Java.h"
#include "../Render/NotificationManager.h"
#include "../Utils/Logger.h"
#include "StatColors.h"
#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <shlobj.h>
#include <string>
#include <windows.h>

static std::string g_configPath;
static std::string g_apiKey;
static std::string g_overlayMode = "gui"; // default to gui mode
static bool g_tabEnabled = true;
static std::string g_tabDisplayMode = "fkdr";
static bool g_tabSortDescending = true;
static std::string g_sortMode = "Team";
static bool g_ovShowStar = true, g_ovShowFk = true, g_ovShowFkdr = true,
            g_ovShowWins = true, g_ovShowWlr = true, g_ovShowWs = true;
static bool g_ovShowKills = false, g_ovShowKdr = false, g_ovShowBeds = false,
            g_ovShowBlr = false, g_ovShowPing = false, g_ovShowTags = true;

static bool g_proShowStar = true, g_proShowFk = true, g_proShowFkdr = true,
            g_proShowWins = true, g_proShowWlr = true, g_proShowWs = true;
static bool g_proShowKills = true, g_proShowKdr = true, g_proShowBeds = true,
            g_proShowBlr = true, g_proShowPing = true, g_proShowTags = true,
            g_proShowHp = true;

static bool g_debugging = false;
static bool g_bedDefenseEnabled = false;
static int g_clickGuiKey = 45; // INSERT
static bool g_clickGuiOn = true;
static bool g_notificationsEnabled = true;
static bool g_autoGGEnabled = true;
static std::string g_autoGGMessage = "gg";
static DWORD g_themeColor = 0xFF0055A4;
static bool g_motionBlurEnabled = false;
static float g_motionBlurAmount = 0.5f;
static bool g_nameTagsEnabled = true;
static float g_nameTagHeight = 2.4f; // world-Y offset above feet

static std::vector<std::pair<std::string, bool>> g_nameTagStats = {
    {"star", true},
    {"fkdr", false},
    {"fk", false},
    {"wins", false},
    {"wlr", false},
    {"ws", false},
};
static const std::vector<std::string> kNameTagValidKeys = {
    "star", "fkdr", "fk", "wins", "wlr", "ws"
};

static std::string serializeNameTagStats() {
  std::string out;
  for (size_t i = 0; i < g_nameTagStats.size(); ++i) {
    if (i) out += ',';
    out += g_nameTagStats[i].first;
    out += ':';
    out += g_nameTagStats[i].second ? '1' : '0';
  }
  return out;
}

static void parseNameTagStats(const std::string &s) {
  if (s.empty()) return;
  std::vector<std::pair<std::string, bool>> parsed;
  size_t pos = 0;
  while (pos < s.size()) {
    size_t comma = s.find(',', pos);
    if (comma == std::string::npos) comma = s.size();
    std::string entry = s.substr(pos, comma - pos);
    pos = comma + 1;
    size_t colon = entry.find(':');
    if (colon == std::string::npos) continue;
    std::string key = entry.substr(0, colon);
    bool enabled = (entry.substr(colon + 1) == "1");
    bool isValid = false;
    for (const auto &v : kNameTagValidKeys)
      if (v == key) { isValid = true; break; }
    if (isValid) parsed.push_back({key, enabled});
  }
  for (const auto &v : kNameTagValidKeys) {
    bool found = false;
    for (const auto &p : parsed)
      if (p.first == v) { found = true; break; }
    if (!found) parsed.push_back({v, false});
  }
  if (!parsed.empty()) g_nameTagStats = parsed;
}
static bool g_urchinEnabled = false;
static std::string g_urchinApiKey = "";
static bool g_seraphEnabled = false;
static std::string g_seraphApiKey = "";
static bool g_tagsEnabled = false;
static std::string g_activeTagService = "Urchin";
static bool g_chatBypasserEnabled = false;
static bool g_discordRpcEnabled = true;
static std::string g_discordAppId = "1467865675262329019";
static bool g_nickedBypass = true;
static bool g_techEnabled = false;
static bool g_anticheatEnabled = true;
static bool g_anticheatNoSlowEnabled = true;
static bool g_anticheatAutoBlockEnabled = true;
static bool g_anticheatEagleEnabled = true;
static bool g_anticheatScaffoldEnabled = true;
static bool g_anticheatCheckSelfEnabled = true;
static int g_anticheatVl = 5;
static int g_anticheatCooldownSec = 4;
static float g_techX = 0.8f;
static float g_techY = 0.02f;
static bool g_commandsEnabled = true;
static bool g_teamReportEnabled = false;
static std::string g_teamReportChannel = "/pc";
static bool g_preGameChatStatsEnabled = true;
static bool g_smartChatBypassEnabled = false;
static bool g_betterTabModeEnabled = false;
static bool g_keylessMode = false;
static std::string g_commandPrefix = ".";
static std::string g_auroraApiKey = "";
static bool g_numberDenickerEnabled = false;
static int g_pingDisplayMode = 0;
static HMODULE g_hModule = nullptr;

static bool g_debugGlobal = false;
static bool g_debugGameDetection = false;
static bool g_debugBedDetection = false;
static bool g_debugUrchin = false;
static bool g_debugSeraph = false;
static bool g_debugGUI = false;
static bool g_debugBedDefense = false;
static bool g_debugGeneral = false;

static std::string getConfigDir() {
  char appdata[MAX_PATH]{};
  if (SUCCEEDED(
          SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
    std::string dir = std::string(appdata) + "\\OVson";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
  }
  return ".";
}

static std::string getConfigPath() { return getConfigDir() + "\\config.json"; }

static bool parseJsonLine(const std::string &line, const char *key,
                          std::string &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = line.find(pat);
  if (k == std::string::npos)
    return false;
  size_t q1 = line.find('"', k + pat.size());
  if (q1 == std::string::npos)
    return false;
  size_t q2 = line.find('"', q1 + 1);
  if (q2 == std::string::npos)
    return false;
  out = line.substr(q1 + 1, q2 - (q1 + 1));
  return true;
}

static bool parseJsonInt(const std::string &all, const char *key, int &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = all.find(pat);
  if (k == std::string::npos)
    return false;
  size_t colon = all.find(':', k);
  if (colon == std::string::npos)
    return false;
  size_t start = all.find_first_of("0123456789-", colon);
  if (start == std::string::npos)
    return false;
  size_t end = all.find_first_not_of("0123456789-", start);
  out = atoi(all.substr(start, end - start).c_str());
  return true;
}

static bool parseJsonBool(const std::string &all, const char *key, bool &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = all.find(pat);
  if (k == std::string::npos)
    return false;
  size_t colon = all.find(':', k);
  if (colon == std::string::npos)
    return false;
  size_t t = all.find("true", colon);
  size_t f = all.find("false", colon);
  if (t != std::string::npos && (f == std::string::npos || t < f)) {
    out = true;
    return true;
  }
  if (f != std::string::npos) {
    out = false;
    return true;
  }
  return false;
}

static bool parseJsonFloat(const std::string &all, const char *key,
                           float &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = all.find(pat);
  if (k == std::string::npos)
    return false;
  size_t colon = all.find(':', k);
  if (colon == std::string::npos)
    return false;
  size_t start = all.find_first_of("-0123456789.", colon);
  if (start == std::string::npos)
    return false;
  out = (float)std::atof(all.c_str() + start);
  return true;
}

static bool parseJsonUInt(const std::string &all, const char *key, DWORD &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = all.find(pat);
  if (k == std::string::npos)
    return false;
  size_t colon = all.find(':', k);
  if (colon == std::string::npos)
    return false;
  size_t start = all.find_first_of("0123456789", colon);
  if (start == std::string::npos)
    return false;
  out = (DWORD)std::strtoul(all.c_str() + start, nullptr, 10);
  return true;
}

bool Config::initialize(HMODULE self) {
  g_hModule = self;
  g_configPath = getConfigPath();
  FILE *f = nullptr;
  fopen_s(&f, g_configPath.c_str(), "r");
  if (!f) {
    g_apiKey.clear();
    g_overlayMode = "gui";
    g_tabEnabled = true;
    g_tabDisplayMode = "fkdr";
    g_tabSortDescending = true;
    return save();
  }
  char buf[2048];
  std::string all;
  while (fgets(buf, sizeof(buf), f))
    all += buf;
  fclose(f);
  std::string val;
  if (parseJsonLine(all, "apiKey", val))
    g_apiKey = val;
  else
    g_apiKey.clear();
  if (parseJsonLine(all, "overlayMode", val))
    g_overlayMode = val;
  else
    g_overlayMode = "chat";

  if (!parseJsonBool(all, "tabEnabled", g_tabEnabled))
    g_tabEnabled = false;

  if (parseJsonLine(all, "tabDisplayMode", val))
    g_tabDisplayMode = val;
  else
    g_tabDisplayMode = "fkdr";

  if (!parseJsonBool(all, "tabSortDescending", g_tabSortDescending))
    g_tabSortDescending = true;

  if (!parseJsonBool(all, "debugging", g_debugging))
    g_debugging = false;

  if (!parseJsonBool(all, "bedDefenseEnabled", g_bedDefenseEnabled))
    g_bedDefenseEnabled = false;

  if (!parseJsonInt(all, "clickGuiKey", g_clickGuiKey))
    g_clickGuiKey = 45;

  if (!parseJsonBool(all, "clickGuiOn", g_clickGuiOn))
    g_clickGuiOn = true;

  if (!parseJsonBool(all, "notificationsEnabled", g_notificationsEnabled))
    g_notificationsEnabled = true;

  if (!parseJsonBool(all, "autoGGEnabled", g_autoGGEnabled))
    g_autoGGEnabled = true;

  if (parseJsonLine(all, "autoGGMessage", val))
    g_autoGGMessage = val;
  else
    g_autoGGMessage = "gg";

  if (!parseJsonUInt(all, "themeColor", g_themeColor))
    g_themeColor = 0xFF0055A4;

  if (!parseJsonBool(all, "motionBlurEnabled", g_motionBlurEnabled))
    g_motionBlurEnabled = false;

  if (!parseJsonFloat(all, "motionBlurAmount", g_motionBlurAmount))
    g_motionBlurAmount = 0.5f;

  if (!parseJsonBool(all, "nameTagsEnabled", g_nameTagsEnabled))
    g_nameTagsEnabled = true;

  if (!parseJsonFloat(all, "nameTagHeight", g_nameTagHeight))
    g_nameTagHeight = 2.4f;

  {
    std::string ntStats;
    if (parseJsonLine(all, "nameTagStats", ntStats))
      parseNameTagStats(ntStats);
  }

  if (!parseJsonBool(all, "urchinEnabled", g_urchinEnabled))
    g_urchinEnabled = false;

  if (parseJsonLine(all, "urchinApiKey", val))
    g_urchinApiKey = val;
  else
    g_urchinApiKey = "";

  if (!parseJsonBool(all, "seraphEnabled", g_seraphEnabled))
    g_seraphEnabled = false;

  if (parseJsonLine(all, "seraphApiKey", val))
    g_seraphApiKey = val;
  else
    g_seraphApiKey = "";

  if (!parseJsonBool(all, "numberDenickerEnabled", g_numberDenickerEnabled))
    g_numberDenickerEnabled = false;

  if (!parseJsonInt(all, "pingDisplayMode", g_pingDisplayMode))
    g_pingDisplayMode = 0;

  if (parseJsonLine(all, "auroraApiKey", val))
    g_auroraApiKey = val;
  else
    g_auroraApiKey = "";

  if (!parseJsonBool(all, "tagsEnabled", g_tagsEnabled))
    g_tagsEnabled = false;

  if (parseJsonLine(all, "activeTagService", val))
    g_activeTagService = val;
  else
    g_activeTagService = "Urchin";

  if (!parseJsonBool(all, "chatBypasserEnabled", g_chatBypasserEnabled))
    g_chatBypasserEnabled = false;

  if (!parseJsonBool(all, "discordRpcEnabled", g_discordRpcEnabled))
    g_discordRpcEnabled = true;

  if (parseJsonLine(all, "discordAppId", val))
    g_discordAppId = val;
  else
    g_discordAppId = "1467865675262329019";

  if (!parseJsonBool(all, "nickedBypass", g_nickedBypass))
    g_nickedBypass = true;

  if (g_discordAppId == "1335272304856010773") {
    g_discordAppId = "1467865675262329019";
    save();
  }

  if (parseJsonLine(all, "sortMode", val))
    g_sortMode = val;
  else
    g_sortMode = "Team";

  parseJsonBool(all, "ovShowStar", g_ovShowStar);
  parseJsonBool(all, "ovShowFk", g_ovShowFk);
  parseJsonBool(all, "ovShowFkdr", g_ovShowFkdr);
  parseJsonBool(all, "ovShowWins", g_ovShowWins);
  parseJsonBool(all, "ovShowWlr", g_ovShowWlr);
  parseJsonBool(all, "ovShowWs", g_ovShowWs);
  parseJsonBool(all, "ovShowKills", g_ovShowKills);
  parseJsonBool(all, "ovShowKdr", g_ovShowKdr);
  parseJsonBool(all, "ovShowBeds", g_ovShowBeds);
  parseJsonBool(all, "ovShowBlr", g_ovShowBlr);
  parseJsonBool(all, "ovShowPing", g_ovShowPing);
  parseJsonBool(all, "ovShowTags", g_ovShowTags);

  parseJsonBool(all, "proShowStar", g_proShowStar);
  parseJsonBool(all, "proShowFk", g_proShowFk);
  parseJsonBool(all, "proShowFkdr", g_proShowFkdr);
  parseJsonBool(all, "proShowWins", g_proShowWins);
  parseJsonBool(all, "proShowWlr", g_proShowWlr);
  parseJsonBool(all, "proShowWs", g_proShowWs);
  parseJsonBool(all, "proShowKills", g_proShowKills);
  parseJsonBool(all, "proShowKdr", g_proShowKdr);
  parseJsonBool(all, "proShowBeds", g_proShowBeds);
  parseJsonBool(all, "proShowBlr", g_proShowBlr);
  parseJsonBool(all, "proShowPing", g_proShowPing);
  parseJsonBool(all, "proShowTags", g_proShowTags);
  parseJsonBool(all, "proShowHp", g_proShowHp);

  if (!parseJsonBool(all, "debugGlobal", g_debugGlobal))
    g_debugGlobal = false;
  if (!parseJsonBool(all, "debugGameDetection", g_debugGameDetection))
    g_debugGameDetection = false;
  if (!parseJsonBool(all, "debugBedDetection", g_debugBedDetection))
    g_debugBedDetection = false;
  if (!parseJsonBool(all, "debugUrchin", g_debugUrchin))
    g_debugUrchin = false;
  if (!parseJsonBool(all, "debugSeraph", g_debugSeraph))
    g_debugSeraph = false;
  if (!parseJsonBool(all, "debugGUI", g_debugGUI))
    g_debugGUI = false;
  if (!parseJsonBool(all, "debugBedDefense", g_debugBedDefense))
    g_debugBedDefense = false;
  if (!parseJsonBool(all, "debugGeneral", g_debugGeneral))
    g_debugGeneral = false;

  if (!parseJsonBool(all, "techEnabled", g_techEnabled))
    g_techEnabled = false;
  if (!parseJsonFloat(all, "techX", g_techX))
    g_techX = 0.8f;
  if (!parseJsonFloat(all, "techY", g_techY))
    g_techY = 0.02f;
  if (!parseJsonBool(all, "commandsEnabled", g_commandsEnabled))
    g_commandsEnabled = true;
  if (!parseJsonBool(all, "teamReportEnabled", g_teamReportEnabled))
    g_teamReportEnabled = false;
  if (parseJsonLine(all, "teamReportChannel", val))
    g_teamReportChannel = val;
  else
    g_teamReportChannel = "/pc";

  if (!parseJsonBool(all, "preGameChatStatsEnabled", g_preGameChatStatsEnabled))
    g_preGameChatStatsEnabled = true;

  if (!parseJsonBool(all, "keylessMode", g_keylessMode)) {
    g_keylessMode = g_apiKey.empty();
  }

  if (!parseJsonBool(all, "smartChatBypassEnabled", g_smartChatBypassEnabled))
    g_smartChatBypassEnabled = false;

  if (!parseJsonBool(all, "betterTabModeEnabled", g_betterTabModeEnabled))
    g_betterTabModeEnabled = false;

  if (parseJsonLine(all, "commandPrefix", val))
    g_commandPrefix = val;
  else
    g_commandPrefix = ".";

  if (!parseJsonInt(all, "pingDisplayMode", g_pingDisplayMode))
    g_pingDisplayMode = 0;

  StatColors::initialize();
  {
    std::string colorsPath = getConfigDir() + "\\statcolors.json";
    FILE *cf = nullptr;
    fopen_s(&cf, colorsPath.c_str(), "r");
    if (cf) {
      char cbuf[4096];
      std::string colorJson;
      while (fgets(cbuf, sizeof(cbuf), cf))
        colorJson += cbuf;
      fclose(cf);
      if (!colorJson.empty())
        StatColors::deserializeFromJson(colorJson);
    }
  }

  if (!parseJsonBool(all, "anticheatEnabled", g_anticheatEnabled))
    g_anticheatEnabled = true;
  if (!parseJsonBool(all, "anticheatNoSlowEnabled", g_anticheatNoSlowEnabled))
    g_anticheatNoSlowEnabled = true;
  if (!parseJsonBool(all, "anticheatAutoBlockEnabled",
                     g_anticheatAutoBlockEnabled))
    g_anticheatAutoBlockEnabled = true;
  if (!parseJsonBool(all, "anticheatEagleEnabled", g_anticheatEagleEnabled))
    g_anticheatEagleEnabled = true;
  if (!parseJsonBool(all, "anticheatScaffoldEnabled",
                     g_anticheatScaffoldEnabled))
    g_anticheatScaffoldEnabled = true;
  if (!parseJsonBool(all, "anticheatCheckSelfEnabled",
                     g_anticheatCheckSelfEnabled))
    g_anticheatCheckSelfEnabled = true;
  if (!parseJsonInt(all, "anticheatVl", g_anticheatVl))
    g_anticheatVl = 5;
  if (!parseJsonInt(all, "anticheatCooldownSec", g_anticheatCooldownSec))
    g_anticheatCooldownSec = 4;

  return true;
}

HMODULE Config::getModuleHandle() { return g_hModule; }

bool Config::save() {
  FILE *f = nullptr;
  fopen_s(&f, g_configPath.c_str(), "w");
  if (!f)
    return false;
  fprintf(
      f,
      "{\n"
      "  \"apiKey\": \"%s\",\n"
      "  \"overlayMode\": \"%s\",\n"
      "  \"tabEnabled\": %s,\n"
      "  \"debugging\": %s,\n"
      "  \"bedDefenseEnabled\": %s,\n"
      "  \"clickGuiKey\": %d,\n"
      "  \"clickGuiOn\": %s,\n"
      "  \"notificationsEnabled\": %s,\n"
      "  \"autoGGEnabled\": %s,\n"
      "  \"autoGGMessage\": \"%s\",\n"
      "  \"themeColor\": %u,\n"
      "  \"motionBlurEnabled\": %s,\n"
      "  \"motionBlurAmount\": %.2f,\n"
      "  \"nameTagsEnabled\": %s,\n"
      "  \"nameTagHeight\": %.2f,\n"
      "  \"nameTagStats\": \"%s\",\n"
      "  \"urchinEnabled\": %s,\n"
      "  \"urchinApiKey\": \"%s\",\n"
      "  \"seraphEnabled\": %s,\n"
      "  \"seraphApiKey\": \"%s\",\n"
      "  \"tagsEnabled\": %s,\n"
      "  \"activeTagService\": \"%s\",\n"
      "  \"chatBypasserEnabled\": %s,\n"
      "  \"debugGlobal\": %s,\n"
      "  \"debugGameDetection\": %s,\n"
      "  \"debugBedDetection\": %s,\n"
      "  \"debugUrchin\": %s,\n"
      "  \"debugSeraph\": %s,\n"
      "  \"debugGUI\": %s,\n"
      "  \"debugBedDefense\": %s,\n"
      "  \"debugGeneral\": %s,\n"
      "  \"discordRpcEnabled\": %s,\n"
      "  \"discordAppId\": \"%s\",\n"
      "  \"tabDisplayMode\": \"%s\",\n"
      "  \"tabSortDescending\": %s,\n"
      "  \"nickedBypass\": %s,\n"
      "  \"sortMode\": \"%s\",\n"
      "  \"ovShowStar\": %s, \"ovShowFk\": %s, \"ovShowFkdr\": %s, "
      "\"ovShowWins\": %s, \"ovShowWlr\": %s, \"ovShowWs\": %s,\n"
      "  \"ovShowKills\": %s, \"ovShowKdr\": %s, \"ovShowBeds\": %s, "
      "\"ovShowBlr\": %s, \"ovShowPing\": %s, \"ovShowTags\": %s,\n"
      "  \"proShowStar\": %s, \"proShowFk\": %s, \"proShowFkdr\": %s, "
      "\"proShowWins\": %s, \"proShowWlr\": %s, \"proShowWs\": %s,\n"
      "  \"proShowKills\": %s, \"proShowKdr\": %s, \"proShowBeds\": %s, "
      "\"proShowBlr\": %s, \"proShowPing\": %s, \"proShowTags\": %s, "
      "\"proShowHp\": %s,\n"
      "  \"techEnabled\": %s,\n"
      "  \"techX\": %.4f,\n"
      "  \"techY\": %.4f,\n"
      "  \"commandsEnabled\": %s,\n"
      "  \"teamReportEnabled\": %s,\n"
      "  \"teamReportChannel\": \"%s\",\n"
      "  \"preGameChatStatsEnabled\": %s,\n"
      "  \"smartChatBypassEnabled\": %s,\n"
      "  \"betterTabModeEnabled\": %s,\n"
      "  \"keylessMode\": %s,\n"
      "  \"commandPrefix\": \"%s\",\n"
      "  \"auroraApiKey\": \"%s\",\n"
      "  \"numberDenickerEnabled\": %s,\n"
      "  \"pingDisplayMode\": %d,\n"
      "  \"anticheatEnabled\": %s,\n"
      "  \"anticheatNoSlowEnabled\": %s,\n"
      "  \"anticheatAutoBlockEnabled\": %s,\n"
      "  \"anticheatEagleEnabled\": %s,\n"
      "  \"anticheatScaffoldEnabled\": %s,\n"
      "  \"anticheatCheckSelfEnabled\": %s,\n"
      "  \"anticheatVl\": %d,\n"
      "  \"anticheatCooldownSec\": %d\n"
      "}\n",
      g_apiKey.c_str(), g_overlayMode.c_str(), g_tabEnabled ? "true" : "false",
      g_debugging ? "true" : "false", g_bedDefenseEnabled ? "true" : "false",
      g_clickGuiKey, g_clickGuiOn ? "true" : "false",
      g_notificationsEnabled ? "true" : "false",
      g_autoGGEnabled ? "true" : "false", g_autoGGMessage.c_str(), g_themeColor,
      g_motionBlurEnabled ? "true" : "false", g_motionBlurAmount,
      g_nameTagsEnabled ? "true" : "false", g_nameTagHeight,
      serializeNameTagStats().c_str(),
      g_urchinEnabled ? "true" : "false", g_urchinApiKey.c_str(),
      g_seraphEnabled ? "true" : "false", g_seraphApiKey.c_str(),
      g_tagsEnabled ? "true" : "false", g_activeTagService.c_str(),
      g_chatBypasserEnabled ? "true" : "false",
      g_debugGlobal ? "true" : "false", g_debugGameDetection ? "true" : "false",
      g_debugBedDetection ? "true" : "false", g_debugUrchin ? "true" : "false",
      g_debugSeraph ? "true" : "false", g_debugGUI ? "true" : "false",
      g_debugBedDefense ? "true" : "false", g_debugGeneral ? "true" : "false",
      g_discordRpcEnabled ? "true" : "false", g_discordAppId.c_str(),
      g_tabDisplayMode.c_str(), g_tabSortDescending ? "true" : "false",
      g_nickedBypass ? "true" : "false", g_sortMode.c_str(),
      g_ovShowStar ? "true" : "false", g_ovShowFk ? "true" : "false",
      g_ovShowFkdr ? "true" : "false", g_ovShowWins ? "true" : "false",
      g_ovShowWlr ? "true" : "false", g_ovShowWs ? "true" : "false",
      g_ovShowKills ? "true" : "false", g_ovShowKdr ? "true" : "false",
      g_ovShowBeds ? "true" : "false", g_ovShowBlr ? "true" : "false",
      g_ovShowPing ? "true" : "false", g_ovShowTags ? "true" : "false",
      g_proShowStar ? "true" : "false", g_proShowFk ? "true" : "false",
      g_proShowFkdr ? "true" : "false", g_proShowWins ? "true" : "false",
      g_proShowWlr ? "true" : "false", g_proShowWs ? "true" : "false",
      g_proShowKills ? "true" : "false", g_proShowKdr ? "true" : "false",
      g_proShowBeds ? "true" : "false", g_proShowBlr ? "true" : "false",
      g_proShowPing ? "true" : "false", g_proShowTags ? "true" : "false",
      g_proShowHp ? "true" : "false", g_techEnabled ? "true" : "false", g_techX,
      g_techY, g_commandsEnabled ? "true" : "false",
      g_teamReportEnabled ? "true" : "false", g_teamReportChannel.c_str(),
      g_preGameChatStatsEnabled ? "true" : "false",
      g_smartChatBypassEnabled ? "true" : "false",
      g_betterTabModeEnabled ? "true" : "false",
      g_keylessMode ? "true" : "false", g_commandPrefix.c_str(),
      g_auroraApiKey.c_str(), g_numberDenickerEnabled ? "true" : "false",
      g_pingDisplayMode, g_anticheatEnabled ? "true" : "false",
      g_anticheatNoSlowEnabled ? "true" : "false",
      g_anticheatAutoBlockEnabled ? "true" : "false",
      g_anticheatEagleEnabled ? "true" : "false",
      g_anticheatScaffoldEnabled ? "true" : "false",
      g_anticheatCheckSelfEnabled ? "true" : "false", g_anticheatVl,
      g_anticheatCooldownSec);
  fclose(f);

  {
    std::string colorsPath = getConfigDir() + "\\statcolors.json";
    FILE *cf = nullptr;
    fopen_s(&cf, colorsPath.c_str(), "w");
    if (cf) {
      std::string colorJson = StatColors::serializeToJson();
      fputs(colorJson.c_str(), cf);
      fclose(cf);
    }
  }

  return true;
}

const std::string &Config::getApiKey() { return g_apiKey; }

void Config::setApiKey(const std::string &key) {
  g_apiKey = key;
  save();
}

bool Config::isKeylessModeEnabled() { return g_keylessMode; }
void Config::setKeylessModeEnabled(bool enabled) {
  g_keylessMode = enabled;
  save();
}

const std::string &Config::getOverlayMode() { return g_overlayMode; }

void Config::setOverlayMode(const std::string &mode) {
  g_overlayMode = mode;
  save();
}

bool Config::isTabEnabled() { return g_tabEnabled; }
void Config::setTabEnabled(bool enabled) {
  g_tabEnabled = enabled;
  save();
}

const std::string &Config::getSortMode() { return g_sortMode; }
void Config::setSortMode(const std::string &mode) {
  g_sortMode = mode;
  save();
}

const std::string &Config::getTabDisplayMode() { return g_tabDisplayMode; }
void Config::setTabDisplayMode(const std::string &mode) {
  g_tabDisplayMode = mode;
  save();
}

bool Config::isTabSortDescending() { return g_tabSortDescending; }
void Config::setTabSortDescending(bool desc) {
  g_tabSortDescending = desc;
  save();
}

bool Config::isDebugging() { return g_debugging; }
void Config::setDebugging(bool enabled) {
  g_debugging = enabled;
  save();
}

bool Config::isBedDefenseEnabled() {
  if (isForgeEnvironment())
    return false;
  return g_bedDefenseEnabled;
}
void Config::setBedDefenseEnabled(bool enabled) {
  if (isForgeEnvironment()) {
    Render::NotificationManager::getInstance()->add(
        "System", "This mode is disabled on Forge",
        Render::NotificationType::Warning);
    return;
  }
  g_bedDefenseEnabled = enabled;
  save();
}

bool Config::isNickedBypass() { return g_nickedBypass; }
void Config::setNickedBypass(bool enabled) {
  g_nickedBypass = enabled;
  save();
}

int Config::getClickGuiKey() { return g_clickGuiKey; }
void Config::setClickGuiKey(int key) {
  g_clickGuiKey = key;
  save();
}

bool Config::isNotificationsEnabled() { return g_notificationsEnabled; }
void Config::setNotificationsEnabled(bool enabled) {
  g_notificationsEnabled = enabled;
  save();
}

bool Config::isClickGuiOn() { return g_clickGuiOn; }
void Config::setClickGuiOn(bool on) {
  g_clickGuiOn = on;
  save();
}

bool Config::isAutoGGEnabled() { return g_autoGGEnabled; }
void Config::setAutoGGEnabled(bool enabled) {
  g_autoGGEnabled = enabled;
  save();
}
const std::string &Config::getAutoGGMessage() { return g_autoGGMessage; }
void Config::setAutoGGMessage(const std::string &msg) {
  g_autoGGMessage = msg;
  save();
}

DWORD Config::getThemeColor() { return g_themeColor; }
void Config::setThemeColor(DWORD color) {
  g_themeColor = color;
  save();
}

bool Config::isMotionBlurEnabled() { return g_motionBlurEnabled; }
void Config::setMotionBlurEnabled(bool enabled) {
  g_motionBlurEnabled = enabled;
  save();
}
float Config::getMotionBlurAmount() { return g_motionBlurAmount; }
void Config::setMotionBlurAmount(float amount) {
  g_motionBlurAmount = amount;
  save();
}

bool Config::isNameTagsEnabled() { return g_nameTagsEnabled; }
void Config::setNameTagsEnabled(bool enabled) {
  g_nameTagsEnabled = enabled;
  save();
}
float Config::getNameTagHeight() { return g_nameTagHeight; }
void Config::setNameTagHeight(float h) {
  if (h < 0.5f) h = 0.5f;
  if (h > 4.0f) h = 4.0f;
  g_nameTagHeight = h;
  save();
}

std::vector<std::pair<std::string, bool>> Config::getNameTagStats() {
  return g_nameTagStats;
}
void Config::setNameTagStats(
    const std::vector<std::pair<std::string, bool>> &stats) {
  std::vector<std::pair<std::string, bool>> out;
  for (const auto &p : stats) {
    bool valid = false;
    for (const auto &v : kNameTagValidKeys)
      if (v == p.first) { valid = true; break; }
    bool dup = false;
    for (const auto &o : out)
      if (o.first == p.first) { dup = true; break; }
    if (valid && !dup) out.push_back(p);
  }
  for (const auto &v : kNameTagValidKeys) {
    bool found = false;
    for (const auto &o : out)
      if (o.first == v) { found = true; break; }
    if (!found) out.push_back({v, false});
  }
  g_nameTagStats = out;
  save();
}

bool Config::isUrchinEnabled() { return g_urchinEnabled; }
void Config::setUrchinEnabled(bool enabled) {
  g_urchinEnabled = enabled;
  save();
}
const std::string &Config::getUrchinApiKey() { return g_urchinApiKey; }
void Config::setUrchinApiKey(const std::string &key) {
  g_urchinApiKey = key;
  save();
}

bool Config::isSeraphEnabled() { return g_seraphEnabled; }
void Config::setSeraphEnabled(bool enabled) {
  g_seraphEnabled = enabled;
  save();
}
const std::string &Config::getSeraphApiKey() { return g_seraphApiKey; }
void Config::setSeraphApiKey(const std::string &key) {
  g_seraphApiKey = key;
  save();
}

const std::string &Config::getAuroraApiKey() { return g_auroraApiKey; }
void Config::setAuroraApiKey(const std::string &key) {
  g_auroraApiKey = key;
  save();
}
bool Config::isNumberDenickerEnabled() { return g_numberDenickerEnabled; }
void Config::setNumberDenickerEnabled(bool enabled) {
  g_numberDenickerEnabled = enabled;
  save();
}

int Config::getPingDisplayMode() { return g_pingDisplayMode; }
void Config::setPingDisplayMode(int mode) {
  g_pingDisplayMode = mode;
  save();
}

bool Config::isTagsEnabled() { return g_tagsEnabled; }
void Config::setTagsEnabled(bool enabled) {
  g_tagsEnabled = enabled;
  save();
}
const std::string &Config::getActiveTagService() { return g_activeTagService; }
void Config::setActiveTagService(const std::string &service) {
  g_activeTagService = service;
  save();
}

bool Config::isChatBypasserEnabled() { return g_chatBypasserEnabled; }
void Config::setChatBypasserEnabled(bool enabled) {
  g_chatBypasserEnabled = enabled;
  save();
}

bool Config::isDebugEnabled(DebugCategory cat) {
  if (g_debugGlobal)
    return true;
  return false;
}

void Config::setDebugEnabled(DebugCategory cat, bool enabled) {
  switch (cat) {
  case DebugCategory::General:
    g_debugGeneral = enabled;
    break;
  case DebugCategory::GameDetection:
    g_debugGameDetection = enabled;
    break;
  case DebugCategory::BedDetection:
    g_debugBedDetection = enabled;
    break;
  case DebugCategory::Urchin:
    g_debugUrchin = enabled;
    break;
  case DebugCategory::Seraph:
    g_debugSeraph = enabled;
    break;
  case DebugCategory::GUI:
    g_debugGUI = enabled;
    break;
  case DebugCategory::BedDefense:
    g_debugBedDefense = enabled;
    break;
  }
  save();
}

bool Config::isGlobalDebugEnabled() { return g_debugGlobal; }
void Config::setGlobalDebugEnabled(bool enabled) {
  g_debugGlobal = enabled;
  save();
}

bool Config::isDiscordRpcEnabled() { return g_discordRpcEnabled; }
void Config::setDiscordRpcEnabled(bool enabled) {
  g_discordRpcEnabled = enabled;
  save();
}
const std::string &Config::getDiscordAppId() { return g_discordAppId; }
void Config::setDiscordAppId(const std::string &id) {
  g_discordAppId = id;
  save();
}

bool Config::isOvShowStar() { return g_ovShowStar; }
void Config::setOvShowStar(bool b) {
  g_ovShowStar = b;
  save();
}
bool Config::isOvShowFk() { return g_ovShowFk; }
void Config::setOvShowFk(bool b) {
  g_ovShowFk = b;
  save();
}
bool Config::isOvShowFkdr() { return g_ovShowFkdr; }
void Config::setOvShowFkdr(bool b) {
  g_ovShowFkdr = b;
  save();
}
bool Config::isOvShowWins() { return g_ovShowWins; }
void Config::setOvShowWins(bool b) {
  g_ovShowWins = b;
  save();
}
bool Config::isOvShowWlr() { return g_ovShowWlr; }
void Config::setOvShowWlr(bool b) {
  g_ovShowWlr = b;
  save();
}
bool Config::isOvShowWs() { return g_ovShowWs; }
void Config::setOvShowWs(bool b) {
  g_ovShowWs = b;
  save();
}
bool Config::isOvShowKills() { return g_ovShowKills; }
void Config::setOvShowKills(bool b) {
  g_ovShowKills = b;
  save();
}
bool Config::isOvShowKdr() { return g_ovShowKdr; }
void Config::setOvShowKdr(bool b) {
  g_ovShowKdr = b;
  save();
}
bool Config::isOvShowBeds() { return g_ovShowBeds; }
void Config::setOvShowBeds(bool b) {
  g_ovShowBeds = b;
  save();
}
bool Config::isOvShowBlr() { return g_ovShowBlr; }
void Config::setOvShowBlr(bool b) {
  g_ovShowBlr = b;
  save();
}
bool Config::isOvShowPing() { return g_ovShowPing; }
void Config::setOvShowPing(bool b) {
  g_ovShowPing = b;
  save();
}
bool Config::isOvShowTags() { return g_ovShowTags; }
void Config::setOvShowTags(bool b) {
  g_ovShowTags = b;
  save();
}

bool Config::isProShowStar() { return g_proShowStar; }
void Config::setProShowStar(bool b) {
  g_proShowStar = b;
  save();
}
bool Config::isProShowFk() { return g_proShowFk; }
void Config::setProShowFk(bool b) {
  g_proShowFk = b;
  save();
}
bool Config::isProShowFkdr() { return g_proShowFkdr; }
void Config::setProShowFkdr(bool b) {
  g_proShowFkdr = b;
  save();
}
bool Config::isProShowWins() { return g_proShowWins; }
void Config::setProShowWins(bool b) {
  g_proShowWins = b;
  save();
}
bool Config::isProShowWlr() { return g_proShowWlr; }
void Config::setProShowWlr(bool b) {
  g_proShowWlr = b;
  save();
}
bool Config::isProShowWs() { return g_proShowWs; }
void Config::setProShowWs(bool b) {
  g_proShowWs = b;
  save();
}
bool Config::isProShowKills() { return g_proShowKills; }
void Config::setProShowKills(bool b) {
  g_proShowKills = b;
  save();
}
bool Config::isProShowKdr() { return g_proShowKdr; }
void Config::setProShowKdr(bool b) {
  g_proShowKdr = b;
  save();
}
bool Config::isProShowBeds() { return g_proShowBeds; }
void Config::setProShowBeds(bool b) {
  g_proShowBeds = b;
  save();
}
bool Config::isProShowBlr() { return g_proShowBlr; }
void Config::setProShowBlr(bool b) {
  g_proShowBlr = b;
  save();
}
bool Config::isProShowPing() { return g_proShowPing; }
void Config::setProShowPing(bool b) {
  g_proShowPing = b;
  save();
}
bool Config::isProShowTags() { return g_proShowTags; }
void Config::setProShowTags(bool b) {
  g_proShowTags = b;
  save();
}
bool Config::isProShowHp() { return g_proShowHp; }
void Config::setProShowHp(bool b) {
  g_proShowHp = b;
  save();
}

// aliases
bool Config::isShowStar() { return isOvShowStar(); }
void Config::setShowStar(bool b) { setOvShowStar(b); }
bool Config::isShowFk() { return isOvShowFk(); }
void Config::setShowFk(bool b) { setOvShowFk(b); }
bool Config::isShowFkdr() { return isOvShowFkdr(); }
void Config::setShowFkdr(bool b) { setOvShowFkdr(b); }
bool Config::isShowWins() { return isOvShowWins(); }
void Config::setShowWins(bool b) { setOvShowWins(b); }
bool Config::isShowWlr() { return isOvShowWlr(); }
void Config::setShowWlr(bool b) { setOvShowWlr(b); }
bool Config::isShowWs() { return isOvShowWs(); }
void Config::setShowWs(bool b) { setOvShowWs(b); }
bool Config::isShowKills() { return isOvShowKills(); }
void Config::setShowKills(bool b) { setOvShowKills(b); }
bool Config::isShowKdr() { return isOvShowKdr(); }
void Config::setShowKdr(bool b) { setOvShowKdr(b); }
bool Config::isShowBeds() { return isOvShowBeds(); }
void Config::setShowBeds(bool b) { setOvShowBeds(b); }
bool Config::isShowBlr() { return isOvShowBlr(); }
void Config::setShowBlr(bool b) { setOvShowBlr(b); }
bool Config::isShowPing() { return isOvShowPing(); }
void Config::setShowPing(bool b) { setOvShowPing(b); }

bool Config::isTechEnabled() { return g_techEnabled; }
void Config::setTechEnabled(bool enabled) {
  g_techEnabled = enabled;
  save();
}
float Config::getTechX() { return g_techX; }
void Config::setTechX(float x) {
  g_techX = x;
  save();
}
float Config::getTechY() { return g_techY; }
void Config::setTechY(float y) {
  g_techY = y;
  save();
}

bool Config::isCommandsEnabled() { return g_commandsEnabled; }
void Config::setCommandsEnabled(bool enabled) {
  g_commandsEnabled = enabled;
  save();
}

bool Config::isTeamReportEnabled() { return g_teamReportEnabled; }
void Config::setTeamReportEnabled(bool enabled) {
  g_teamReportEnabled = enabled;
  save();
}
const std::string &Config::getTeamReportChannel() {
  return g_teamReportChannel;
}
void Config::setTeamReportChannel(const std::string &channel) {
  g_teamReportChannel = channel;
  save();
}

bool Config::isPreGameChatStatsEnabled() { return g_preGameChatStatsEnabled; }
void Config::setPreGameChatStatsEnabled(bool enabled) {
  g_preGameChatStatsEnabled = enabled;
  save();
}

bool Config::isSmartChatBypassEnabled() { return g_smartChatBypassEnabled; }
void Config::setSmartChatBypassEnabled(bool enabled) {
  g_smartChatBypassEnabled = enabled;
  save();
}

bool Config::isBetterTabModeEnabled() { return g_betterTabModeEnabled; }
void Config::setBetterTabModeEnabled(bool enabled) {
  g_betterTabModeEnabled = enabled;
  save();
}

const std::string &Config::getCommandPrefix() { return g_commandPrefix; }
void Config::setCommandPrefix(const std::string &prefix) {
  g_commandPrefix = prefix;
  save();
}

bool Config::isForgeEnvironment() {
  static bool checked = false;
  static bool forge = false;
  if (!checked) {
    JNIEnv *env = lc->getEnv();
    if (env) {
      jclass forgeCls =
          env->FindClass("net/minecraftforge/common/MinecraftForge");
      if (forgeCls) {
        forge = true;
        env->DeleteLocalRef(forgeCls);
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    checked = true;
  }
  return forge;
}

bool Config::isAnticheatEnabled() { return g_anticheatEnabled; }
void Config::setAnticheatEnabled(bool e) {
  g_anticheatEnabled = e;
  save();
}
bool Config::isAnticheatNoSlowEnabled() { return g_anticheatNoSlowEnabled; }
void Config::setAnticheatNoSlowEnabled(bool e) {
  g_anticheatNoSlowEnabled = e;
  save();
}
bool Config::isAnticheatAutoBlockEnabled() {
  return g_anticheatAutoBlockEnabled;
}
void Config::setAnticheatAutoBlockEnabled(bool e) {
  g_anticheatAutoBlockEnabled = e;
  save();
}
bool Config::isAnticheatEagleEnabled() { return g_anticheatEagleEnabled; }
void Config::setAnticheatEagleEnabled(bool e) {
  g_anticheatEagleEnabled = e;
  save();
}
bool Config::isAnticheatScaffoldEnabled() { return g_anticheatScaffoldEnabled; }
void Config::setAnticheatScaffoldEnabled(bool e) {
  g_anticheatScaffoldEnabled = e;
  save();
}
bool Config::isAnticheatCheckSelfEnabled() {
  return g_anticheatCheckSelfEnabled;
}
void Config::setAnticheatCheckSelfEnabled(bool e) {
  g_anticheatCheckSelfEnabled = e;
  save();
}
int Config::getAnticheatVl() { return g_anticheatVl; }
void Config::setAnticheatVl(int vl) {
  g_anticheatVl = vl;
  save();
}
int Config::getAnticheatCooldownSec() { return g_anticheatCooldownSec; }
void Config::setAnticheatCooldownSec(int s) {
  g_anticheatCooldownSec = s;
  save();
}
