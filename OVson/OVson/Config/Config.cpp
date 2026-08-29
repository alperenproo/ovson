#include "Config.h"
#include "../Java.h"
#include "../Logic/BlockHitAudio.h"
#include "../Render/NotificationManager.h"
#include "../Utils/Logger.h"
#include "StatColors.h"
#include <algorithm>
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
static bool g_chatStatsEnabled = true;
static std::string g_chatStatsFormat = "fkdr";
static std::string g_chatStatsStyle = "Parentheses";

static bool g_proShowStar = true, g_proShowFk = true, g_proShowFkdr = true,
            g_proShowWins = true, g_proShowWlr = true, g_proShowWs = true;
static bool g_proShowKills = true, g_proShowKdr = true, g_proShowBeds = true,
            g_proShowBlr = true, g_proShowPing = true, g_proShowTags = true,
            g_proShowHp = true;

static bool g_debugging = false;
static bool g_bedDefenseEnabled = false;
static int g_clickGuiKey = 45; // INSERT
static bool g_clickGuiOn = true;
static int g_uninjectKey = 35; // END
static bool g_uninjectKeyEnabled = true;
static bool g_notificationsEnabled = true;
static bool g_autoGGEnabled = true;
static std::string g_autoGGMessage = "gg";
static std::string g_lunarLogPath = "";
static std::string g_badlionLogPath = "";
static std::string g_legacyBadlionLogPath = "";
static DWORD g_themeColor = 0xFF3D6EF5;
static bool g_chromaEnabled = false;
static float g_chromaSpeed = 30.0f;
static bool g_motionBlurEnabled = false;
static float g_motionBlurAmount = 0.5f;
static bool g_nameTagsEnabled = true;
static float g_nameTagHeight = 2.4f; // world-Y offset above feet

static bool g_muteTagAlertsEnabled = false;
static bool g_muteSelfTagAlertsEnabled = false;
static bool g_muteTeamTagAlertsEnabled = false;
static std::vector<std::string> g_mutedTagPlayers;

static std::string serializeMutedTagPlayers() {
  std::string out;
  for (size_t i = 0; i < g_mutedTagPlayers.size(); ++i) {
    if (i) out += ',';
    out += g_mutedTagPlayers[i];
  }
  return out;
}

static void parseMutedTagPlayers(const std::string &s) {
  g_mutedTagPlayers.clear();
  if (s.empty()) return;
  size_t start = 0;
  while (true) {
    size_t comma = s.find(',', start);
    if (comma == std::string::npos) {
      g_mutedTagPlayers.push_back(s.substr(start));
      break;
    }
    g_mutedTagPlayers.push_back(s.substr(start, comma - start));
    start = comma + 1;
  }
}

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
// stored as a string for future
static std::string g_clickGuiTheme = "Solid";
static std::string g_clickGuiLayout = "A";
static float g_clickGuiX = 100.0f;
static float g_clickGuiY = 100.0f;
static float g_clickGuiWidth = 920.0f;
static float g_clickGuiHeight = 600.0f;
static std::string g_layoutBData = "";
static std::string g_bedwarsSettingsData = "";
static bool g_liquidGlassWiggle = true;
static bool g_liquidGlassGlow = true;
static float g_liquidGlassRefractStrength = 0.7f;
static float g_liquidGlassEdgeWidth = 0.6f;
static float g_liquidGlassCardEdgeWidth = 0.6f;
static float g_liquidGlassDarkness = 0.58f;
static bool g_discordRpcEnabled = true;
static std::string g_discordAppId = "1467865675262329019";
static bool g_nickedBypass = true;
static bool g_rawMouseFixEnabled = false;
// Off by default because Minecraft 1.8.9 exposes no server-confirmed
// "damage was blocked" bit; this feature is deliberately heuristic.
static bool g_blockHitSoundEnabled = BlockHitAudio::kDefaultFeatureEnabled;
static bool g_blockHitSoundDebugEnabled =
    BlockHitAudio::kDefaultDebugLoggingEnabled;
static std::string g_blockHitSoundSource = "Default";
static std::string g_blockHitSoundFilename = "block-hit.wav";
static float g_blockHitSoundVolume = BlockHitAudio::kDefaultVolumePercent;
static int g_nickScoreThreshold = 70;
static bool g_nickRollEnabled = true;
static bool g_nickScorePingEnabled = true;
static bool g_nickScoreAlertEveryEnabled = true;
static bool g_nickRollAutoRerollEnabled = false;
static int g_nickRollToggleKey = 0x24; // VK_HOME
static bool g_blockHitWaitForServer = true;
static bool g_mediaOverlayEnabled = false;
static float g_mediaOverlayX = 0.015f;
static float g_mediaOverlayY = 0.80f;
static float g_mediaOverlayScale = 1.0f;
static int g_mediaOverlayLayout = 0;
// Spotify's own surface colours: #121212 ground, #1DB954 accent.
static unsigned long g_mediaOverlayBgColor = 0x121212;
static unsigned long g_mediaOverlayAccentColor = 0x1DB954;
static unsigned long g_mediaOverlayTextColor = 0xFFFFFF;
static float g_mediaOverlayOpacity = 0.88f;
static float g_mediaOverlayCorner = 10.0f;
static bool g_mediaOverlayArt = true;
// 600ms is deliberately unhurried. The book has to redraw and the server has
// to answer between presses, and a delay short enough to outrun that reads the
// same page twice and burns rerolls on a name it already scored.
static int g_nickRollRerollDelayMs = 600;
static int g_nickRollRerollCap = 300;
static std::string g_nickRollTargetWord;
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
static bool g_lobbyMentionStatsEnabled = true;
static bool g_smartChatBypassEnabled = false;
static bool g_betterTabModeEnabled = false;
static float g_betterTabX = -1.0f;
static float g_betterTabY = -1.0f;
static float g_betterTabScale = 1.0f;
static bool g_keylessMode = false;
static std::string g_commandPrefix = ".";
static std::string g_auroraApiKey = "";
static std::string g_spoofIp = "";
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

static std::atomic<bool> g_savePending = false;
static ULONGLONG g_lastSaveRequest = 0;

std::string Config::getDataDirectory() {
  char appdata[MAX_PATH]{};
  if (SUCCEEDED(
          SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
    std::string dir = std::string(appdata) + "\\OVson";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
  }
  return ".";
}

static std::string getConfigDir() { return Config::getDataDirectory(); }

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

static std::string sanitizeNickRollTargetWord(const std::string &value) {
  std::string sanitized;
  sanitized.reserve(value.size() < 16U ? value.size() : 16U);
  for (const char c : value) {
    const bool allowed = (c >= 'A' && c <= 'Z') ||
                         (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_';
    if (allowed)
      sanitized.push_back(c);
    if (sanitized.size() == 16U)
      break;
  }
  return sanitized;
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

  if (parseJsonLine(all, "spoofIp", val))
    g_spoofIp = val;
  else
    g_spoofIp = "";

  if (!parseJsonBool(all, "tabSortDescending", g_tabSortDescending))
    g_tabSortDescending = true;

  if (!parseJsonBool(all, "muteTagAlertsEnabled", g_muteTagAlertsEnabled))
    g_muteTagAlertsEnabled = false;

  if (!parseJsonBool(all, "muteSelfTagAlertsEnabled", g_muteSelfTagAlertsEnabled))
    g_muteSelfTagAlertsEnabled = false;
  if (!parseJsonBool(all, "muteTeamTagAlertsEnabled", g_muteTeamTagAlertsEnabled))
    g_muteTeamTagAlertsEnabled = false;

  if (parseJsonLine(all, "mutedTagPlayers", val))
    parseMutedTagPlayers(val);
  else
    g_mutedTagPlayers.clear();

  if (!parseJsonBool(all, "debugging", g_debugging))
    g_debugging = false;

  if (!parseJsonBool(all, "bedDefenseEnabled", g_bedDefenseEnabled))
    g_bedDefenseEnabled = false;

  if (!parseJsonInt(all, "clickGuiKey", g_clickGuiKey))
    g_clickGuiKey = 45;

  if (!parseJsonBool(all, "clickGuiOn", g_clickGuiOn))
    g_clickGuiOn = true;

  if (!parseJsonInt(all, "uninjectKey", g_uninjectKey))
    g_uninjectKey = 35;

  if (!parseJsonBool(all, "uninjectKeyEnabled", g_uninjectKeyEnabled))
    g_uninjectKeyEnabled = true;

  if (!parseJsonBool(all, "notificationsEnabled", g_notificationsEnabled))
    g_notificationsEnabled = true;

  if (!parseJsonBool(all, "autoGGEnabled", g_autoGGEnabled))
    g_autoGGEnabled = true;

  if (parseJsonLine(all, "autoGGMessage", val))
    g_autoGGMessage = val;
  else
    g_autoGGMessage = "gg";

  if (parseJsonLine(all, "lunarLogPath", val))
    g_lunarLogPath = val;
  else
    g_lunarLogPath = "";

  if (parseJsonLine(all, "badlionLogPath", val))
    g_badlionLogPath = val;
  else
    g_badlionLogPath = "";

  if (parseJsonLine(all, "legacyBadlionLogPath", val))
    g_legacyBadlionLogPath = val;
  else
    g_legacyBadlionLogPath = "";

  if (!parseJsonUInt(all, "themeColor", g_themeColor))
    g_themeColor = 0xFF3D6EF5;

  if (!parseJsonBool(all, "chromaEnabled", g_chromaEnabled))
    g_chromaEnabled = false;
  if (!parseJsonFloat(all, "chromaSpeed", g_chromaSpeed))
    g_chromaSpeed = 30.0f;

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

  if (parseJsonLine(all, "clickGuiTheme", val))
    g_clickGuiTheme = val;
  else
    g_clickGuiTheme = "Solid";

  if (parseJsonLine(all, "clickGuiLayout", val))
    g_clickGuiLayout = val;
  if (!parseJsonFloat(all, "clickGuiX", g_clickGuiX))
    g_clickGuiX = 100.0f;
  if (!parseJsonFloat(all, "clickGuiY", g_clickGuiY))
    g_clickGuiY = 100.0f;
  if (!parseJsonFloat(all, "clickGuiWidth", g_clickGuiWidth))
    g_clickGuiWidth = 920.0f;
  if (!parseJsonFloat(all, "clickGuiHeight", g_clickGuiHeight))
    g_clickGuiHeight = 600.0f;
  g_clickGuiX = std::clamp(g_clickGuiX, -4096.0f, 4096.0f);
  g_clickGuiY = std::clamp(g_clickGuiY, -4096.0f, 4096.0f);
  g_clickGuiWidth = std::clamp(g_clickGuiWidth, 640.0f, 1920.0f);
  g_clickGuiHeight = std::clamp(g_clickGuiHeight, 520.0f, 1200.0f);
  if (parseJsonLine(all, "layoutBData", val))
    g_layoutBData = val;
  if (parseJsonLine(all, "bedwarsSettingsData", val))
    g_bedwarsSettingsData = val;

  if (!parseJsonBool(all, "liquidGlassWiggle", g_liquidGlassWiggle))
    g_liquidGlassWiggle = true;
  if (!parseJsonBool(all, "liquidGlassGlow", g_liquidGlassGlow))
    g_liquidGlassGlow = true;
  if (!parseJsonFloat(all, "liquidGlassRefractStrength", g_liquidGlassRefractStrength))
    g_liquidGlassRefractStrength = 0.7f;
  if (!parseJsonFloat(all, "liquidGlassEdgeWidth", g_liquidGlassEdgeWidth))
    g_liquidGlassEdgeWidth = 0.6f;
  if (!parseJsonFloat(all, "liquidGlassCardEdgeWidth", g_liquidGlassCardEdgeWidth))
    g_liquidGlassCardEdgeWidth = 0.6f;
  if (!parseJsonFloat(all, "liquidGlassDarkness", g_liquidGlassDarkness))
    g_liquidGlassDarkness = 0.58f;

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
  if (!parseJsonBool(all, "rawMouseFixEnabled", g_rawMouseFixEnabled))
    g_rawMouseFixEnabled = false;
  if (!parseJsonBool(all, "blockHitSoundEnabled", g_blockHitSoundEnabled))
    g_blockHitSoundEnabled = BlockHitAudio::kDefaultFeatureEnabled;
  if (!parseJsonBool(all, "blockHitSoundDebugEnabled",
                     g_blockHitSoundDebugEnabled))
    g_blockHitSoundDebugEnabled = BlockHitAudio::kDefaultDebugLoggingEnabled;
  if (parseJsonLine(all, "blockHitSoundSource", val))
    g_blockHitSoundSource = BlockHitAudio::soundSourceName(
        BlockHitAudio::parseSoundSource(val));
  else
    g_blockHitSoundSource = "Default";
  if (parseJsonLine(all, "blockHitSoundFilename", val) &&
      BlockHitAudio::isSafeWavFilename(val))
    g_blockHitSoundFilename = val;
  else
    g_blockHitSoundFilename =
        std::string(BlockHitAudio::kDefaultCustomFilename);
  if (!parseJsonFloat(all, "blockHitSoundVolume", g_blockHitSoundVolume))
    g_blockHitSoundVolume = BlockHitAudio::kDefaultVolumePercent;
  g_blockHitSoundVolume =
      BlockHitAudio::sanitizeVolumePercent(g_blockHitSoundVolume);
  if (!parseJsonInt(all, "nickScoreThreshold", g_nickScoreThreshold))
    g_nickScoreThreshold = 70;
  if (g_nickScoreThreshold < 0) g_nickScoreThreshold = 0;
  if (g_nickScoreThreshold > 100) g_nickScoreThreshold = 100;
  if (!parseJsonBool(all, "nickRollEnabled", g_nickRollEnabled))
    g_nickRollEnabled = true;
  if (!parseJsonBool(all, "nickScorePingEnabled", g_nickScorePingEnabled))
    g_nickScorePingEnabled = true;
  if (!parseJsonBool(all, "nickScoreAlertEveryEnabled",
                     g_nickScoreAlertEveryEnabled))
    g_nickScoreAlertEveryEnabled = true;
  // Auto-reroll defaults OFF. It presses a button in the player's game; that
  // is not something to turn on behind their back on first launch.
  if (!parseJsonBool(all, "nickRollAutoRerollEnabled",
                     g_nickRollAutoRerollEnabled))
    g_nickRollAutoRerollEnabled = false;
  if (!parseJsonInt(all, "nickRollRerollDelayMs", g_nickRollRerollDelayMs))
    g_nickRollRerollDelayMs = 600;
  if (g_nickRollRerollDelayMs < 250) g_nickRollRerollDelayMs = 250;
  if (g_nickRollRerollDelayMs > 5000) g_nickRollRerollDelayMs = 5000;
  if (!parseJsonInt(all, "nickRollToggleKey", g_nickRollToggleKey))
    g_nickRollToggleKey = 0x24;
  if (g_nickRollToggleKey < 1 || g_nickRollToggleKey > 254)
    g_nickRollToggleKey = 0x24;
  // Defaults TRUE so an existing config keeps the confirmation behaviour it
  // has always had; only someone who deliberately turns it off loses it.
  if (!parseJsonBool(all, "blockHitWaitForServer", g_blockHitWaitForServer))
    g_blockHitWaitForServer = true;
  if (!parseJsonBool(all, "mediaOverlayEnabled", g_mediaOverlayEnabled))
    g_mediaOverlayEnabled = false;
  if (!parseJsonFloat(all, "mediaOverlayX", g_mediaOverlayX))
    g_mediaOverlayX = 0.015f;
  if (!parseJsonFloat(all, "mediaOverlayY", g_mediaOverlayY))
    g_mediaOverlayY = 0.80f;
  if (!parseJsonFloat(all, "mediaOverlayScale", g_mediaOverlayScale))
    g_mediaOverlayScale = 1.0f;
  if (!parseJsonInt(all, "mediaOverlayLayout", g_mediaOverlayLayout))
    g_mediaOverlayLayout = 0;
  if (g_mediaOverlayX < 0.0f) g_mediaOverlayX = 0.0f;
  if (g_mediaOverlayX > 1.0f) g_mediaOverlayX = 1.0f;
  if (g_mediaOverlayY < 0.0f) g_mediaOverlayY = 0.0f;
  if (g_mediaOverlayY > 1.0f) g_mediaOverlayY = 1.0f;
  if (g_mediaOverlayScale < 0.5f) g_mediaOverlayScale = 0.5f;
  if (g_mediaOverlayScale > 2.5f) g_mediaOverlayScale = 2.5f;
  if (g_mediaOverlayLayout < 0 || g_mediaOverlayLayout > 1)
    g_mediaOverlayLayout = 0;
  int mediaColor = 0;
  if (parseJsonInt(all, "mediaOverlayBgColor", mediaColor))
    g_mediaOverlayBgColor = static_cast<unsigned long>(mediaColor) & 0xFFFFFFul;
  if (parseJsonInt(all, "mediaOverlayAccentColor", mediaColor))
    g_mediaOverlayAccentColor =
        static_cast<unsigned long>(mediaColor) & 0xFFFFFFul;
  if (parseJsonInt(all, "mediaOverlayTextColor", mediaColor))
    g_mediaOverlayTextColor =
        static_cast<unsigned long>(mediaColor) & 0xFFFFFFul;
  if (!parseJsonFloat(all, "mediaOverlayOpacity", g_mediaOverlayOpacity))
    g_mediaOverlayOpacity = 0.88f;
  if (g_mediaOverlayOpacity < 0.0f) g_mediaOverlayOpacity = 0.0f;
  if (g_mediaOverlayOpacity > 1.0f) g_mediaOverlayOpacity = 1.0f;
  if (!parseJsonFloat(all, "mediaOverlayCorner", g_mediaOverlayCorner))
    g_mediaOverlayCorner = 10.0f;
  if (g_mediaOverlayCorner < 0.0f) g_mediaOverlayCorner = 0.0f;
  if (g_mediaOverlayCorner > 20.0f) g_mediaOverlayCorner = 20.0f;
  if (!parseJsonBool(all, "mediaOverlayArt", g_mediaOverlayArt))
    g_mediaOverlayArt = true;
  if (!parseJsonInt(all, "nickRollRerollCap", g_nickRollRerollCap))
    g_nickRollRerollCap = 300;
  if (g_nickRollRerollCap < 10) g_nickRollRerollCap = 10;
  if (g_nickRollRerollCap > 2000) g_nickRollRerollCap = 2000;
  if (parseJsonLine(all, "nickRollTargetWord", val))
    g_nickRollTargetWord = sanitizeNickRollTargetWord(val);
  else
    g_nickRollTargetWord.clear();

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
  if (!parseJsonBool(all, "lobbyMentionStatsEnabled", g_lobbyMentionStatsEnabled))
    g_lobbyMentionStatsEnabled = true;

  if (!parseJsonBool(all, "chatStatsEnabled", g_chatStatsEnabled))
    g_chatStatsEnabled = true;
  if (parseJsonLine(all, "chatStatsFormat", val))
    g_chatStatsFormat = val;
  else
    g_chatStatsFormat = "fkdr";
  if (parseJsonLine(all, "chatStatsStyle", val))
    g_chatStatsStyle = val;
  else
    g_chatStatsStyle = "Parentheses";

  if (!parseJsonBool(all, "keylessMode", g_keylessMode)) {
    g_keylessMode = g_apiKey.empty();
  }

  if (!parseJsonBool(all, "smartChatBypassEnabled", g_smartChatBypassEnabled))
    g_smartChatBypassEnabled = false;

  if (!parseJsonBool(all, "betterTabModeEnabled", g_betterTabModeEnabled))
    g_betterTabModeEnabled = false;

  if (!parseJsonFloat(all, "betterTabX", g_betterTabX))
    g_betterTabX = -1.0f;
  if (!parseJsonFloat(all, "betterTabY", g_betterTabY))
    g_betterTabY = -1.0f;
  if (!parseJsonFloat(all, "betterTabScale", g_betterTabScale))
    g_betterTabScale = 1.0f;

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

static bool saveImpl();

void Config::update() {
  if (g_savePending) {
    if (GetTickCount64() - g_lastSaveRequest > 500) {
      g_savePending = false;
      saveImpl();
    }
  }
}

static bool saveImpl() {
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
      "  \"uninjectKey\": %d,\n"
      "  \"uninjectKeyEnabled\": %s,\n"
      "  \"notificationsEnabled\": %s,\n"
      "  \"autoGGEnabled\": %s,\n"
      "  \"autoGGMessage\": \"%s\",\n"
      "  \"themeColor\": %u,\n"
      "  \"chromaEnabled\": %s,\n"
      "  \"chromaSpeed\": %.2f,\n"
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
      "  \"muteTagAlertsEnabled\": %s,\n"
      "  \"muteSelfTagAlertsEnabled\": %s,\n"
      "  \"muteTeamTagAlertsEnabled\": %s,\n"
      "  \"mutedTagPlayers\": \"%s\",\n"
      "  \"clickGuiTheme\": \"%s\",\n"
      "  \"clickGuiLayout\": \"%s\",\n"
      "  \"clickGuiX\": %.2f,\n"
      "  \"clickGuiY\": %.2f,\n"
      "  \"clickGuiWidth\": %.2f,\n"
      "  \"clickGuiHeight\": %.2f,\n"
      "  \"layoutBData\": \"%s\",\n"
      "  \"bedwarsSettingsData\": \"%s\",\n"
      "  \"liquidGlassWiggle\": %s,\n"
      "  \"liquidGlassGlow\": %s,\n"
      "  \"liquidGlassRefractStrength\": %.2f,\n"
      "  \"liquidGlassEdgeWidth\": %.2f,\n"
      "  \"liquidGlassCardEdgeWidth\": %.2f,\n"
      "  \"liquidGlassDarkness\": %.2f,\n"
      "  \"chatBypasserEnabled\": %s,\n"
      "  \"chatStatsEnabled\": %s,\n"
      "  \"chatStatsFormat\": \"%s\",\n"
      "  \"chatStatsStyle\": \"%s\",\n"
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
      "  \"spoofIp\": \"%s\",\n"
      "  \"tabDisplayMode\": \"%s\",\n"
      "  \"tabSortDescending\": %s,\n"
      "  \"nickedBypass\": %s,\n"
      "  \"rawMouseFixEnabled\": %s,\n"
      "  \"blockHitSoundEnabled\": %s,\n"
      "  \"blockHitSoundDebugEnabled\": %s,\n"
      "  \"blockHitSoundSource\": \"%s\",\n"
      "  \"blockHitSoundFilename\": \"%s\",\n"
      "  \"blockHitSoundVolume\": %.2f,\n"
      "  \"nickScoreThreshold\": %d,\n"
      "  \"nickRollEnabled\": %s,\n"
      "  \"nickScorePingEnabled\": %s,\n"
      "  \"nickScoreAlertEveryEnabled\": %s,\n"
      "  \"nickRollAutoRerollEnabled\": %s,\n"
      "  \"nickRollRerollDelayMs\": %d,\n"
      "  \"nickRollRerollCap\": %d,\n"
      "  \"nickRollToggleKey\": %d,\n"
      "  \"blockHitWaitForServer\": %s,\n"
      "  \"mediaOverlayEnabled\": %s,\n"
      "  \"mediaOverlayX\": %.4f,\n"
      "  \"mediaOverlayY\": %.4f,\n"
      "  \"mediaOverlayScale\": %.4f,\n"
      "  \"mediaOverlayLayout\": %d,\n"
      "  \"mediaOverlayBgColor\": %lu,\n"
      "  \"mediaOverlayAccentColor\": %lu,\n"
      "  \"mediaOverlayTextColor\": %lu,\n"
      "  \"mediaOverlayOpacity\": %.4f,\n"
      "  \"mediaOverlayCorner\": %.4f,\n"
      "  \"mediaOverlayArt\": %s,\n"
      "  \"nickRollTargetWord\": \"%s\",\n"
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
      "  \"lobbyMentionStatsEnabled\": %s,\n"
      "  \"smartChatBypassEnabled\": %s,\n"
      "  \"betterTabModeEnabled\": %s,\n"
      "  \"betterTabX\": %.2f,\n"
      "  \"betterTabY\": %.2f,\n"
      "  \"betterTabScale\": %.2f,\n"
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
      g_uninjectKey, g_uninjectKeyEnabled ? "true" : "false",
      g_notificationsEnabled ? "true" : "false",
      g_autoGGEnabled ? "true" : "false", g_autoGGMessage.c_str(), g_themeColor,
      g_chromaEnabled ? "true" : "false", g_chromaSpeed,
      g_motionBlurEnabled ? "true" : "false", g_motionBlurAmount,
      g_nameTagsEnabled ? "true" : "false", g_nameTagHeight,
      serializeNameTagStats().c_str(),
      g_urchinEnabled ? "true" : "false", g_urchinApiKey.c_str(),
      g_seraphEnabled ? "true" : "false", g_seraphApiKey.c_str(),
      g_tagsEnabled ? "true" : "false", g_activeTagService.c_str(),
      g_muteTagAlertsEnabled ? "true" : "false",
      g_muteSelfTagAlertsEnabled ? "true" : "false",
      g_muteTeamTagAlertsEnabled ? "true" : "false",
      serializeMutedTagPlayers().c_str(),
      g_clickGuiTheme.c_str(),
      g_clickGuiLayout.c_str(),
      g_clickGuiX, g_clickGuiY, g_clickGuiWidth, g_clickGuiHeight,
      g_layoutBData.c_str(),
      g_bedwarsSettingsData.c_str(),
      g_liquidGlassWiggle ? "true" : "false",
      g_liquidGlassGlow ? "true" : "false",
      g_liquidGlassRefractStrength,
      g_liquidGlassEdgeWidth,
      g_liquidGlassCardEdgeWidth,
      g_liquidGlassDarkness,
      g_chatBypasserEnabled ? "true" : "false",
      g_chatStatsEnabled ? "true" : "false",
      g_chatStatsFormat.c_str(),
      g_chatStatsStyle.c_str(),
      g_debugGlobal ? "true" : "false", g_debugGameDetection ? "true" : "false",
      g_debugBedDetection ? "true" : "false", g_debugUrchin ? "true" : "false",
      g_debugSeraph ? "true" : "false", g_debugGUI ? "true" : "false",
      g_debugBedDefense ? "true" : "false", g_debugGeneral ? "true" : "false",
      g_discordRpcEnabled ? "true" : "false", g_discordAppId.c_str(),
      g_spoofIp.c_str(),
      g_tabDisplayMode.c_str(), g_tabSortDescending ? "true" : "false",
      g_nickedBypass ? "true" : "false",
      g_rawMouseFixEnabled ? "true" : "false",
      g_blockHitSoundEnabled ? "true" : "false",
      g_blockHitSoundDebugEnabled ? "true" : "false",
      g_blockHitSoundSource.c_str(), g_blockHitSoundFilename.c_str(),
      g_blockHitSoundVolume,
      g_nickScoreThreshold, g_nickRollEnabled ? "true" : "false",
      g_nickScorePingEnabled ? "true" : "false",
      g_nickScoreAlertEveryEnabled ? "true" : "false",
      g_nickRollAutoRerollEnabled ? "true" : "false",
      g_nickRollRerollDelayMs, g_nickRollRerollCap, g_nickRollToggleKey,
      g_blockHitWaitForServer ? "true" : "false",
      g_mediaOverlayEnabled ? "true" : "false", g_mediaOverlayX,
      g_mediaOverlayY, g_mediaOverlayScale, g_mediaOverlayLayout,
      g_mediaOverlayBgColor, g_mediaOverlayAccentColor,
      g_mediaOverlayTextColor, g_mediaOverlayOpacity, g_mediaOverlayCorner,
      g_mediaOverlayArt ? "true" : "false",
      g_nickRollTargetWord.c_str(),
      g_sortMode.c_str(),
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
      g_lobbyMentionStatsEnabled ? "true" : "false",
      g_smartChatBypassEnabled ? "true" : "false",
      g_betterTabModeEnabled ? "true" : "false",
      g_betterTabX, g_betterTabY, g_betterTabScale,
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

bool Config::save() {
  g_savePending = true;
  g_lastSaveRequest = GetTickCount64();
  return true;
}

bool Config::saveNow() {
  g_savePending = false;
  return saveImpl();
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

bool Config::isRawMouseFixEnabled() { return g_rawMouseFixEnabled; }
void Config::setRawMouseFixEnabled(bool enabled) {
  g_rawMouseFixEnabled = enabled;
  save();
}

bool Config::isBlockHitSoundEnabled() { return g_blockHitSoundEnabled; }
void Config::setBlockHitSoundEnabled(bool enabled) {
  if (g_blockHitSoundEnabled == enabled) return;
  g_blockHitSoundEnabled = enabled;
  save();
}

bool Config::isBlockHitSoundDebugEnabled() {
  return g_blockHitSoundDebugEnabled;
}
void Config::setBlockHitSoundDebugEnabled(bool enabled) {
  if (g_blockHitSoundDebugEnabled == enabled) return;
  g_blockHitSoundDebugEnabled = enabled;
  save();
}

const std::string &Config::getBlockHitSoundSource() {
  return g_blockHitSoundSource;
}
void Config::setBlockHitSoundSource(const std::string &source) {
  const std::string sanitized = BlockHitAudio::soundSourceName(
      BlockHitAudio::parseSoundSource(source));
  if (g_blockHitSoundSource == sanitized) return;
  g_blockHitSoundSource = sanitized;
  save();
}

const std::string &Config::getBlockHitSoundFilename() {
  return g_blockHitSoundFilename;
}
void Config::setBlockHitSoundFilename(const std::string &filename) {
  if (!BlockHitAudio::isSafeWavFilename(filename) ||
      g_blockHitSoundFilename == filename)
    return;
  g_blockHitSoundFilename = filename;
  save();
}

float Config::getBlockHitSoundVolume() { return g_blockHitSoundVolume; }
void Config::setBlockHitSoundVolume(float volumePercent) {
  const float sanitized =
      BlockHitAudio::sanitizeVolumePercent(volumePercent);
  if (g_blockHitSoundVolume == sanitized) return;
  g_blockHitSoundVolume = sanitized;
  save();
}

int Config::getNickScoreThreshold() { return g_nickScoreThreshold; }
void Config::setNickScoreThreshold(int threshold) {
  const int sanitized = threshold < 0 ? 0 : (threshold > 100 ? 100 : threshold);
  if (g_nickScoreThreshold == sanitized) return;
  g_nickScoreThreshold = sanitized;
  save();
}

bool Config::isNickRollEnabled() { return g_nickRollEnabled; }
void Config::setNickRollEnabled(bool enabled) {
  if (g_nickRollEnabled == enabled) return;
  g_nickRollEnabled = enabled;
  save();
}

bool Config::isNickScorePingEnabled() { return g_nickScorePingEnabled; }
void Config::setNickScorePingEnabled(bool enabled) {
  if (g_nickScorePingEnabled == enabled) return;
  g_nickScorePingEnabled = enabled;
  save();
}

bool Config::isNickScoreAlertEveryEnabled() {
  return g_nickScoreAlertEveryEnabled;
}
void Config::setNickScoreAlertEveryEnabled(bool enabled) {
  if (g_nickScoreAlertEveryEnabled == enabled) return;
  g_nickScoreAlertEveryEnabled = enabled;
  save();
}

bool Config::isNickRollAutoRerollEnabled() {
  return g_nickRollAutoRerollEnabled;
}
void Config::setNickRollAutoRerollEnabled(bool enabled) {
  if (g_nickRollAutoRerollEnabled == enabled) return;
  g_nickRollAutoRerollEnabled = enabled;
  save();
}

int Config::getNickRollRerollDelayMs() { return g_nickRollRerollDelayMs; }
void Config::setNickRollRerollDelayMs(int milliseconds) {
  const int sanitized =
      milliseconds < 250 ? 250 : (milliseconds > 5000 ? 5000 : milliseconds);
  if (g_nickRollRerollDelayMs == sanitized) return;
  g_nickRollRerollDelayMs = sanitized;
  save();
}

int Config::getNickRollToggleKey() { return g_nickRollToggleKey; }
void Config::setNickRollToggleKey(int virtualKey) {
  if (virtualKey < 1 || virtualKey > 254) return;
  if (g_nickRollToggleKey == virtualKey) return;
  g_nickRollToggleKey = virtualKey;
  save();
}

bool Config::isBlockHitWaitForServerEnabled() { return g_blockHitWaitForServer; }
void Config::setBlockHitWaitForServerEnabled(bool enabled) {
  if (g_blockHitWaitForServer == enabled) return;
  g_blockHitWaitForServer = enabled;
  save();
}

bool Config::isMediaOverlayEnabled() { return g_mediaOverlayEnabled; }
void Config::setMediaOverlayEnabled(bool enabled) {
  if (g_mediaOverlayEnabled == enabled) return;
  g_mediaOverlayEnabled = enabled;
  save();
}

float Config::getMediaOverlayX() { return g_mediaOverlayX; }
void Config::setMediaOverlayX(float x) {
  const float clamped = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
  if (g_mediaOverlayX == clamped) return;
  g_mediaOverlayX = clamped;
  save();
}

float Config::getMediaOverlayY() { return g_mediaOverlayY; }
void Config::setMediaOverlayY(float y) {
  const float clamped = y < 0.0f ? 0.0f : (y > 1.0f ? 1.0f : y);
  if (g_mediaOverlayY == clamped) return;
  g_mediaOverlayY = clamped;
  save();
}

float Config::getMediaOverlayScale() { return g_mediaOverlayScale; }
void Config::setMediaOverlayScale(float scale) {
  const float clamped = scale < 0.5f ? 0.5f : (scale > 2.5f ? 2.5f : scale);
  if (g_mediaOverlayScale == clamped) return;
  g_mediaOverlayScale = clamped;
  save();
}

int Config::getMediaOverlayLayout() { return g_mediaOverlayLayout; }
void Config::setMediaOverlayLayout(int layout) {
  const int clamped = layout < 0 ? 0 : (layout > 1 ? 1 : layout);
  if (g_mediaOverlayLayout == clamped) return;
  g_mediaOverlayLayout = clamped;
  save();
}

unsigned long Config::getMediaOverlayBgColor() { return g_mediaOverlayBgColor; }
void Config::setMediaOverlayBgColor(unsigned long rgb) {
  const unsigned long masked = rgb & 0xFFFFFFul;
  if (g_mediaOverlayBgColor == masked) return;
  g_mediaOverlayBgColor = masked;
  save();
}

unsigned long Config::getMediaOverlayAccentColor() {
  return g_mediaOverlayAccentColor;
}
void Config::setMediaOverlayAccentColor(unsigned long rgb) {
  const unsigned long masked = rgb & 0xFFFFFFul;
  if (g_mediaOverlayAccentColor == masked) return;
  g_mediaOverlayAccentColor = masked;
  save();
}

unsigned long Config::getMediaOverlayTextColor() {
  return g_mediaOverlayTextColor;
}
void Config::setMediaOverlayTextColor(unsigned long rgb) {
  const unsigned long masked = rgb & 0xFFFFFFul;
  if (g_mediaOverlayTextColor == masked) return;
  g_mediaOverlayTextColor = masked;
  save();
}

float Config::getMediaOverlayOpacity() { return g_mediaOverlayOpacity; }
void Config::setMediaOverlayOpacity(float opacity) {
  const float clamped = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
  if (g_mediaOverlayOpacity == clamped) return;
  g_mediaOverlayOpacity = clamped;
  save();
}

float Config::getMediaOverlayCorner() { return g_mediaOverlayCorner; }
void Config::setMediaOverlayCorner(float radius) {
  const float clamped = radius < 0.0f ? 0.0f : (radius > 20.0f ? 20.0f : radius);
  if (g_mediaOverlayCorner == clamped) return;
  g_mediaOverlayCorner = clamped;
  save();
}

bool Config::isMediaOverlayArtEnabled() { return g_mediaOverlayArt; }
void Config::setMediaOverlayArtEnabled(bool enabled) {
  if (g_mediaOverlayArt == enabled) return;
  g_mediaOverlayArt = enabled;
  save();
}

int Config::getNickRollRerollCap() { return g_nickRollRerollCap; }
void Config::setNickRollRerollCap(int cap) {
  const int sanitized = cap < 10 ? 10 : (cap > 2000 ? 2000 : cap);
  if (g_nickRollRerollCap == sanitized) return;
  g_nickRollRerollCap = sanitized;
  save();
}

const std::string &Config::getNickRollTargetWord() {
  return g_nickRollTargetWord;
}
void Config::setNickRollTargetWord(const std::string &targetWord) {
  const std::string sanitized = sanitizeNickRollTargetWord(targetWord);
  if (g_nickRollTargetWord == sanitized) return;
  g_nickRollTargetWord = sanitized;
  save();
}

int Config::getClickGuiKey() { return g_clickGuiKey; }
void Config::setClickGuiKey(int key) {
  g_clickGuiKey = key;
  save();
}

int Config::getUninjectKey() { return g_uninjectKey; }
void Config::setUninjectKey(int key) {
  g_uninjectKey = key;
  save();
}

bool Config::isUninjectKeyEnabled() { return g_uninjectKeyEnabled; }
void Config::setUninjectKeyEnabled(bool enabled) {
  g_uninjectKeyEnabled = enabled;
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

bool Config::isChromaEnabled() { return g_chromaEnabled; }
void Config::setChromaEnabled(bool enabled) { g_chromaEnabled = enabled; save(); }

float Config::getChromaSpeed() { return g_chromaSpeed; }
void Config::setChromaSpeed(float speed) { g_chromaSpeed = speed; save(); }

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

const std::string &Config::getLunarLogPath() { return g_lunarLogPath; }
void Config::setLunarLogPath(const std::string &path) {
  g_lunarLogPath = path;
  save();
}

const std::string &Config::getBadlionLogPath() { return g_badlionLogPath; }
void Config::setBadlionLogPath(const std::string &path) {
  g_badlionLogPath = path;
  save();
}

const std::string &Config::getLegacyBadlionLogPath() { return g_legacyBadlionLogPath; }
void Config::setLegacyBadlionLogPath(const std::string &path) {
  g_legacyBadlionLogPath = path;
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
bool Config::isMuteTagAlertsEnabled() { return g_muteTagAlertsEnabled; }
void Config::setMuteTagAlertsEnabled(bool enabled) {
  g_muteTagAlertsEnabled = enabled;
  save();
}
bool Config::isMuteSelfTagAlertsEnabled() { return g_muteSelfTagAlertsEnabled; }
bool Config::isMuteTeamTagAlertsEnabled() { return g_muteTeamTagAlertsEnabled; }
void Config::setMuteTeamTagAlertsEnabled(bool enabled) {
  if (g_muteTeamTagAlertsEnabled == enabled) return;
  g_muteTeamTagAlertsEnabled = enabled;
  save();
}
void Config::setMuteSelfTagAlertsEnabled(bool enabled) {
  g_muteSelfTagAlertsEnabled = enabled;
  save();
}
const std::vector<std::string> &Config::getMutedTagPlayers() {
  return g_mutedTagPlayers;
}
void Config::addMutedTagPlayer(const std::string &name) {
  if (name.empty()) return;
  std::string lowerName = name;
  for (auto &c : lowerName) c = std::tolower(c);
  for (const auto &p : g_mutedTagPlayers) {
    std::string lp = p;
    for (auto &c : lp) c = std::tolower(c);
    if (lp == lowerName) return;
  }
  g_mutedTagPlayers.push_back(name);
  save();
}
void Config::removeMutedTagPlayer(const std::string &name) {
  std::string lowerName = name;
  for (auto &c : lowerName) c = std::tolower(c);
  for (auto it = g_mutedTagPlayers.begin(); it != g_mutedTagPlayers.end(); ++it) {
    std::string lp = *it;
    for (auto &c : lp) c = std::tolower(c);
    if (lp == lowerName) {
      g_mutedTagPlayers.erase(it);
      save();
      break;
    }
  }
}
const std::string &Config::getClickGuiTheme() { return g_clickGuiTheme; }
void Config::setClickGuiTheme(const std::string &theme) {
  g_clickGuiTheme = theme;
  save();
}

const std::string &Config::getClickGuiLayout() { return g_clickGuiLayout; }
void Config::setClickGuiLayout(const std::string &layout) {
  g_clickGuiLayout = layout;
  save();
}
float Config::getClickGuiX() { return g_clickGuiX; }
float Config::getClickGuiY() { return g_clickGuiY; }
float Config::getClickGuiWidth() { return g_clickGuiWidth; }
float Config::getClickGuiHeight() { return g_clickGuiHeight; }
void Config::setClickGuiBounds(float x, float y, float width, float height) {
  x = std::clamp(x, -4096.0f, 4096.0f);
  y = std::clamp(y, -4096.0f, 4096.0f);
  width = std::clamp(width, 640.0f, 1920.0f);
  height = std::clamp(height, 520.0f, 1200.0f);
  if (g_clickGuiX == x && g_clickGuiY == y && g_clickGuiWidth == width &&
      g_clickGuiHeight == height)
    return;
  g_clickGuiX = x;
  g_clickGuiY = y;
  g_clickGuiWidth = width;
  g_clickGuiHeight = height;
  save();
}
const std::string &Config::getLayoutBData() { return g_layoutBData; }
void Config::setLayoutBData(const std::string &data) {
  g_layoutBData = data;
  save();
}
const std::string &Config::getBedwarsSettingsData() {
  return g_bedwarsSettingsData;
}
void Config::setBedwarsSettingsData(const std::string &data) {
  g_bedwarsSettingsData = data;
  save();
}

bool Config::isLiquidGlassWiggleEnabled() { return g_liquidGlassWiggle; }
void Config::setLiquidGlassWiggleEnabled(bool enabled) {
  g_liquidGlassWiggle = enabled;
  save();
}

bool Config::isLiquidGlassGlowEnabled() { return g_liquidGlassGlow; }
void Config::setLiquidGlassGlowEnabled(bool enabled) {
  g_liquidGlassGlow = enabled;
  save();
}

float Config::getLiquidGlassRefractStrength() { return g_liquidGlassRefractStrength; }
void Config::setLiquidGlassRefractStrength(float str) {
  g_liquidGlassRefractStrength = str;
  save();
}

float Config::getLiquidGlassEdgeWidth() { return g_liquidGlassEdgeWidth; }
void Config::setLiquidGlassEdgeWidth(float w) {
  g_liquidGlassEdgeWidth = w;
  save();
}

float Config::getLiquidGlassCardEdgeWidth() { return g_liquidGlassCardEdgeWidth; }
void Config::setLiquidGlassCardEdgeWidth(float w) {
  g_liquidGlassCardEdgeWidth = w;
  save();
}

float Config::getLiquidGlassDarkness() { return g_liquidGlassDarkness; }
void Config::setLiquidGlassDarkness(float d) {
  g_liquidGlassDarkness = d;
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

std::string Config::getSpoofIp() { return g_spoofIp; }
void Config::setSpoofIp(const std::string& ip) {
  g_spoofIp = ip;
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

bool Config::isLobbyMentionStatsEnabled() { return g_lobbyMentionStatsEnabled; }
void Config::setLobbyMentionStatsEnabled(bool enabled) {
  g_lobbyMentionStatsEnabled = enabled;
  save();
}

bool Config::isChatStatsEnabled() { return g_chatStatsEnabled; }
void Config::setChatStatsEnabled(bool enabled) {
  g_chatStatsEnabled = enabled;
  save();
}
const std::string &Config::getChatStatsFormat() { return g_chatStatsFormat; }
void Config::setChatStatsFormat(const std::string &format) {
  g_chatStatsFormat = format;
  save();
}

const std::string &Config::getChatStatsStyle() { return g_chatStatsStyle; }
void Config::setChatStatsStyle(const std::string &style) {
  g_chatStatsStyle = style;
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
float Config::getBetterTabX() { return g_betterTabX; }
void Config::setBetterTabX(float x) {
  g_betterTabX = x;
  save();
}
float Config::getBetterTabY() { return g_betterTabY; }
void Config::setBetterTabY(float y) {
  g_betterTabY = y;
  save();
}
float Config::getBetterTabScale() { return g_betterTabScale; }
void Config::setBetterTabScale(float scale) {
  g_betterTabScale = scale;
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
