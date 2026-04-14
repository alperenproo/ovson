#define WIN32_LEAN_AND_MEAN
#include "ChatInterceptor.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/AutoGG.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Render/NotificationManager.h"
#include "../Services/AbyssService.h"
#include "../Services/DiscordManager.h"
#include "../Services/Hypixel.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/ChatBypasser.h"
#include "../Utils/Logger.h"
#include "../Utils/ThreadTracker.h"
#include "../Utils/Timer.h"
#include "ChatSDK.h"
#include "Commands.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace ChatInterceptor;

// gosh so many messy messy messy
static bool g_initialized = false;
static std::string g_lastOnlineLine;
static std::vector<std::string> g_onlinePlayers;
static size_t g_nextFetchIdx = 0;
static std::unordered_map<std::string, std::string> g_pendingTabNames;
static bool g_lastInGameStatus = false;
static std::string g_lastDetectedModeName = "";
static std::string g_logsDir;
static std::string g_logFilePath;
static HANDLE g_logHandle = INVALID_HANDLE_VALUE;
static long long g_logOffset = 0;
static std::string g_logBuf;
static ULONGLONG g_lastImmediateTeamProbeTick = 0;
static int g_lobbyGraceTicks = 0;
static bool g_explicitLobbySignal = false;
static std::unordered_map<std::string, std::string>
    g_stableRankMap; // Map to freeze positions
static std::mutex g_stableRankMutex;
static std::string g_localTeam; // NEW: track local player's team
static std::string g_localName; // Track local player's name globally
std::unordered_map<std::string, std::string> ChatInterceptor::g_playerTeamColor;
static ULONGLONG g_lastTeamScanTick = 0;
static ULONGLONG g_lastChatReadTick = 0;
static ULONGLONG g_lastResetTick = 0;
static ULONGLONG g_lastDetectionLogTick = 0;
static int g_mode = 0; // 0 bedwars, 1 skywars, 2 duels
static ULONGLONG g_bootstrapStartTick = 0;
static std::unordered_map<std::string, int> g_teamProbeTries;
static std::unordered_set<std::string> g_processedPlayers;
// static std::queue<std::string> g_fetchQueue; // Removed
static std::unordered_set<std::string> g_queuedPlayers;
static std::unordered_set<std::string> g_alertedPlayers;
static std::mutex g_alertedMutex;
static std::mutex g_queueMutex;

static std::unordered_set<std::string> g_activeFetches;
static std::mutex g_activeFetchesMutex;
static bool g_inHypixelGame = false;
static bool g_inPreGameLobby = false;
static std::vector<std::string> g_manualPushedPlayers;
static std::unordered_set<std::string> g_forceChatOutputPlayers;
static std::unordered_set<std::string> g_chatPrintedPlayers;
static ULONGLONG g_preGameDetectTick = 0;
static std::unordered_map<std::string, Hypixel::PlayerStats> g_pendingStatsMap;
static std::mutex g_pendingStatsMutex;
static std::unordered_map<std::string, ULONGLONG> g_retryUntil;
static std::mutex g_retryMutex;
static std::unordered_map<std::string, int> g_playerFetchRetries;
static std::unordered_set<std::string> g_eliminatedPlayers;
static std::mutex g_eliminatedMutex;
std::unordered_map<std::string, Hypixel::PlayerStats>
    ChatInterceptor::g_playerStatsMap;
std::mutex ChatInterceptor::g_statsMutex;
float ChatInterceptor::g_jniLatency = 0.0f;
float ChatInterceptor::g_apiLatency = 0.0f;
float ChatInterceptor::g_scanSpeed = 0.0f;

struct CachedStats {
  Hypixel::PlayerStats stats;
  ULONGLONG timestamp = 0;
  CachedStats() : timestamp(0) {}
  CachedStats(const Hypixel::PlayerStats &s, ULONGLONG t)
      : stats(s), timestamp(t) {}
};
static std::unordered_map<std::string, CachedStats> g_persistentStatsCache;
static std::mutex g_cacheMutex;
static const size_t MAX_STATS_CACHE_SIZE = 500;
static const ULONGLONG STATS_CACHE_EXPIRY_MS = 600000;

static std::unordered_map<std::string, std::string> g_playerUuidMap;
static std::mutex g_uuidMapMutex;
static bool g_teamReportSent = false;

static const char *colorForFkdr(double fkdr) {
  if (fkdr < 1.0)
    return "\xC2\xA7"
           "7";
  if (fkdr < 2.0)
    return "\xC2\xA7"
           "f";
  if (fkdr < 3.0)
    return "\xC2\xA7"
           "6";
  if (fkdr < 4.0)
    return "\xC2\xA7"
           "b";
  if (fkdr < 5.0)
    return "\xC2\xA7"
           "a";
  if (fkdr < 6.0)
    return "\xC2\xA7"
           "5";
  return "\xC2\xA7"
         "4";
}

static const char *colorForWlr(double wlr) {
  if (wlr < 1.0)
    return "\xC2\xA7"
           "f";
  if (wlr < 3.0)
    return "\xC2\xA7"
           "6";
  if (wlr < 5.0)
    return "\xC2\xA7"
           "4";
  return "\xC2\xA7"
         "4";
}

static const char *colorForWins(int wins) {
  if (wins < 500)
    return "\xC2\xA7"
           "7";
  if (wins < 1000)
    return "\xC2\xA7"
           "f";
  if (wins < 2000)
    return "\xC2\xA7"
           "e";
  if (wins < 4000)
    return "\xC2\xA7"
           "4";
  return "\xC2\xA7"
         "4";
}

static const char *colorForFinalKills(int fk) {
  if (fk < 1000)
    return "\xC2\xA7"
           "7";
  if (fk < 2000)
    return "\xC2\xA7"
           "f";
  if (fk < 4000)
    return "\xC2\xA7"
           "6";
  if (fk < 5000)
    return "\xC2\xA7"
           "b";
  if (fk < 10000)
    return "\xC2\xA7"
           "4";
  return "\xC2\xA7"
         "4";
}

static const char *colorForStar(int star) {
  if (star < 100)
    return "\xC2\xA7"
           "7";
  if (star < 200)
    return "\xC2\xA7"
           "f";
  if (star < 300)
    return "\xC2\xA7"
           "6";
  if (star < 400)
    return "\xC2\xA7"
           "b";
  if (star < 500)
    return "\xC2\xA7"
           "a";
  if (star < 600)
    return "\xC2\xA7"
           "b";
  return "\xC2\xA7"
         "4";
}

static void sendTeamStatsReport() {
  if (g_teamReportSent || !Config::isTeamReportEnabled() || g_localTeam.empty())
    return;

  std::vector<Hypixel::PlayerStats> teammates;
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    for (const auto &pair : ChatInterceptor::g_playerStatsMap) {
      auto it = g_playerTeamColor.find(pair.first);
      if (it != g_playerTeamColor.end() && it->second == g_localTeam) {
        teammates.push_back(pair.second);
      }
    }
  }

  if (teammates.empty())
    return;

  int totalStars = 0;
  int totalWins = 0;
  int totalFK = 0;
  int totalFD = 0;
  for (const auto &s : teammates) {
    totalStars += s.bedwarsStar;
    totalWins += s.bedwarsWins;
    totalFK += s.bedwarsFinalKills;
    totalFD += s.bedwarsFinalDeaths;
  }

  int count = (int)teammates.size();
  double avgStar = (double)totalStars / count;
  double avgWins = (double)totalWins / count;
  double teamFkdr =
      (totalFD == 0) ? (double)totalFK : (double)totalFK / totalFD;

  std::string channel = Config::getTeamReportChannel();
  if (channel != "/shout" && channel != "/pc" && channel != "/ac")
    channel = "/pc";

  const char *s = "\xC2\xA7"; // § symbol
  const char *f = "\xC2\xA7"
                  "f"; // Reset/White
  const char *g = "\xC2\xA7"
                  "7"; // Gray

  char buf[512];
  // format
  snprintf(buf, sizeof(buf),
           "%s %sTeam Average %s| %s%.0f* %s| %s%.0f Wins %s| %s%.2f FKDR",
           channel.c_str(), f, g, colorForStar((int)avgStar), avgStar, g,
           colorForWins((int)avgWins), avgWins, g, colorForFkdr(teamFkdr),
           teamFkdr);

  ChatSDK::sendClientChat(buf);

  g_teamReportSent = true;
  Logger::info("Automated Team Average Stats Report sent to %s",
               channel.c_str());
}

static void pruneStatsCache() {
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  ULONGLONG now = GetTickCount64();

  for (auto it = g_persistentStatsCache.begin();
       it != g_persistentStatsCache.end();) {
    if ((now - it->second.timestamp) > STATS_CACHE_EXPIRY_MS) {
      it = g_persistentStatsCache.erase(it);
    } else {
      ++it;
    }
  }

  while (g_persistentStatsCache.size() > MAX_STATS_CACHE_SIZE) {
    auto oldest = g_persistentStatsCache.begin();
    for (auto it = g_persistentStatsCache.begin();
         it != g_persistentStatsCache.end(); ++it) {
      if (it->second.timestamp < oldest->second.timestamp) {
        oldest = it;
      }
    }
    g_persistentStatsCache.erase(oldest);
  }
}

static const char *mcColorForTeam(const std::string &team) {
  if (team == "Red")
    return "\xC2\xA7"
           "c";
  if (team == "Blue")
    return "\xC2\xA7"
           "9";
  if (team == "Green")
    return "\xC2\xA7"
           "a";
  if (team == "Yellow")
    return "\xC2\xA7"
           "e";
  if (team == "Aqua")
    return "\xC2\xA7"
           "b";
  if (team == "White")
    return "\xC2\xA7"
           "f";
  if (team == "Pink")
    return "\xC2\xA7"
           "d";
  if (team == "Gray" || team == "Grey")
    return "\xC2\xA7"
           "8";
  return "\xC2\xA7"
         "f";
}

static void resetGameCache();
static void syncTeamColors();

struct JCache {
  jclass worldCls = nullptr;
  jmethodID m_getScoreboard = nullptr;

  jclass sbCls = nullptr;
  jmethodID m_getPlayersTeam = nullptr;
  jmethodID m_getObjectiveInDisplaySlot = nullptr;
  jmethodID m_getObjective = nullptr;
  jmethodID m_getValueFromObjective = nullptr;
  jmethodID m_onScoreUpdated = nullptr;

  jclass teamCls = nullptr;
  jmethodID m_getPrefix = nullptr;

  jclass scoreCls = nullptr;
  jmethodID m_getScorePoints = nullptr;
  jmethodID m_setScorePoints = nullptr;

  jfieldID f_gpName = nullptr;

  std::atomic<bool> initialized{false};
  std::mutex initMutex;

  void init(JNIEnv *env) {
    if (initialized)
      return;
    std::lock_guard<std::mutex> lock(initMutex);
    if (initialized)
      return;

    if (!env)
      return;

    if (!worldCls) {

      jclass local =
          lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
      if (!local)
        return;
      if (local)
        worldCls = (jclass)env->NewGlobalRef(local);
    }
    if (worldCls) {
      m_getScoreboard = env->GetMethodID(
          worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScoreboard =
            env->GetMethodID(worldCls, "func_96441_U",
                             "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScoreboard =
            env->GetMethodID(worldCls, "func_96441_as",
                             "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScoreboard =
            env->GetMethodID(worldCls, "func_72967_aN",
                             "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScoreboard =
            env->GetMethodID(worldCls, "func_72883_A",
                             "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScoreboard = env->GetMethodID(worldCls, "Z", "()Lauo;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (!sbCls) {

      jclass local = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
      if (!local)
        return;
      if (local)
        sbCls = (jclass)env->NewGlobalRef(local);
    }
    if (sbCls) {
      m_getPlayersTeam = env->GetMethodID(
          sbCls, "getPlayersTeam",
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getPlayersTeam = env->GetMethodID(
            sbCls, "func_96509_i",
            "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getPlayersTeam =
            env->GetMethodID(sbCls, "h", "(Ljava/lang/String;)Laul;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();

      m_getObjectiveInDisplaySlot =
          env->GetMethodID(sbCls, "getObjectiveInDisplaySlot",
                           "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getObjectiveInDisplaySlot =
            env->GetMethodID(sbCls, "func_96539_a",
                             "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getObjectiveInDisplaySlot = env->GetMethodID(sbCls, "a", "(I)Lauk;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();

      m_getObjective = env->GetMethodID(
          sbCls, "getObjective",
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScoreObjective;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getObjective = env->GetMethodID(
            sbCls, "func_96518_b",
            "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScoreObjective;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getObjective =
            env->GetMethodID(sbCls, "b", "(Ljava/lang/String;)Lauk;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();

      m_getValueFromObjective =
          env->GetMethodID(sbCls, "getValueFromObjective",
                           "(Ljava/lang/String;Lnet/minecraft/scoreboard/"
                           "ScoreObjective;)Lnet/minecraft/scoreboard/Score;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getValueFromObjective = env->GetMethodID(
            sbCls, "func_96529_a",
            "(Ljava/lang/String;Lnet/minecraft/scoreboard/ScoreObjective;)Lnet/"
            "minecraft/scoreboard/Score;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getValueFromObjective =
            env->GetMethodID(sbCls, "c", "(Ljava/lang/String;Lauk;)Laum;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_onScoreUpdated = env->GetMethodID(sbCls, "broadcastScoreUpdate",
                                          "(Ljava/lang/String;)V");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_onScoreUpdated =
            env->GetMethodID(sbCls, "func_96516_a", "(Ljava/lang/String;)V");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_onScoreUpdated =
            env->GetMethodID(sbCls, "a", "(Ljava/lang/String;)V");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (!teamCls) {

      jclass local = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
      if (!local)
        return;
      if (local)
        teamCls = (jclass)env->NewGlobalRef(local);
    }
    if (teamCls) {
      m_getPrefix =
          env->GetMethodID(teamCls, "getPrefix", "()Ljava/lang/String;");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getPrefix =
            env->GetMethodID(teamCls, "func_96668_e", "()Ljava/lang/String;");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getPrefix = env->GetMethodID(teamCls, "e", "()Ljava/lang/String;");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (!scoreCls) {

      jclass local = lc->GetClass("net.minecraft.scoreboard.Score");
      if (!local)
        return;
      if (local)
        scoreCls = (jclass)env->NewGlobalRef(local);
    }
    if (scoreCls) {
      m_getScorePoints = env->GetMethodID(scoreCls, "getScorePoints", "()I");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScorePoints = env->GetMethodID(scoreCls, "func_96652_c", "()I");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getScorePoints = env->GetMethodID(scoreCls, "c", "()I");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();

      m_setScorePoints = env->GetMethodID(scoreCls, "setScorePoints", "(I)V");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        // im fucking tired of these mappings
        m_setScorePoints = env->GetMethodID(scoreCls, "func_96647_a", "(I)V");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_setScorePoints = env->GetMethodID(scoreCls, "func_96647_c", "(I)V");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_setScorePoints = env->GetMethodID(scoreCls, "c", "(I)V");
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (!f_gpName) {

      jclass gpCls = lc->GetClass("com.mojang.authlib.GameProfile");
      if (!gpCls)
        return;
      if (gpCls) {
        f_gpName = env->GetFieldID(gpCls, "name", "Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          f_gpName =
              env->GetFieldID(gpCls, "field_109761_d", "Ljava/lang/String;");
        }
        if (env->ExceptionCheck())
          env->ExceptionClear();
      }
    }

    initialized = true;

    // debug
    Logger::info("JCache init: m_getScorePoints=%p m_setScorePoints=%p "
                 "m_getValueFromObjective=%p m_onScoreUpdated=%p "
                 "m_getObjectiveInDisplaySlot=%p",
                 m_getScorePoints, m_setScorePoints, m_getValueFromObjective,
                 m_onScoreUpdated, m_getObjectiveInDisplaySlot);
  }

  void cleanup(JNIEnv *env) {
    std::lock_guard<std::mutex> lock(initMutex);
    if (!initialized)
      return;
    if (env) {
      if (worldCls)
        env->DeleteGlobalRef(worldCls);
      if (sbCls)
        env->DeleteGlobalRef(sbCls);
      if (teamCls)
        env->DeleteGlobalRef(teamCls);
      if (scoreCls)
        env->DeleteGlobalRef(scoreCls);
    }
    worldCls = nullptr;
    sbCls = nullptr;
    teamCls = nullptr;
    scoreCls = nullptr;
    f_gpName = nullptr;
    initialized = false;
  }
};

static JCache g_jCache;

static std::string resolveTeamForNameEx(JNIEnv *env, const std::string &name,
                                        jobject scoreboard,
                                        jmethodID m_getPlayersTeam,
                                        jclass teamCls, jmethodID m_getPrefix);

// moved

static const char *teamInitial(const std::string &team) {
  if (team == "Red")
    return "R";
  if (team == "Blue")
    return "B";
  if (team == "Green")
    return "G";
  if (team == "Yellow")
    return "Y";
  if (team == "Aqua")
    return "A";
  if (team == "White")
    return "W";
  if (team == "Pink")
    return "P";
  if (team == "Gray" || team == "Grey") // haha grey
    return "G";
  return "?";
}

static void detectTeamsFromLine(const std::string &chat) {
  static const char *teams[] = {"Red",   "Blue", "Green", "Yellow", "Aqua",
                                "White", "Pink", "Gray",  "Grey"};
  for (const char *t : teams) {
    std::string needle1 = std::string("You are on the ") + t + " Team!";
    if (chat.find(needle1) != std::string::npos) {
      Logger::info("Local team detected: %s", t);
      g_localTeam = t;
      if (!g_localName.empty() && !g_localTeam.empty()) {
        g_playerTeamColor[g_localName] = g_localTeam;
      }
      sendTeamStatsReport();
    }
    std::string needle2 = std::string(" joined (") + t + ")";
    auto p2 = chat.find(needle2);
    if (p2 != std::string::npos) {
      auto s = chat.rfind(' ', p2);
      std::string name =
          (s == std::string::npos) ? std::string() : chat.substr(0, s);
      auto sp = name.find_last_of(' ');
      if (sp != std::string::npos)
        name = name.substr(sp + 1);
      if (!name.empty()) {
        g_playerTeamColor[name] = t;
        Logger::info("Team detected: %s -> %s", name.c_str(), t);
      }
    }
  }
}

static std::string teamFromColorCode(char code) {
  switch (code) {
  case 'c':
    return "Red";
  case '9':
    return "Blue";
  case 'a':
    return "Green";
  case 'e':
    return "Yellow";
  case 'b':
    return "Aqua";
  case 'f':
    return "White";
  case '7':
    return "Gray";
  case 'd':
    return "Pink";
  case '8':
    return "Gray";
  default:
    return "Unknown";
  }
}

static void detectFinalKillsFromLine(const std::string &chat) {
  // another check
  std::string lowerChat = chat;
  for (auto &c : lowerChat)
    c = toupper(c);

  if (lowerChat.find("FINAL KILL!") == std::string::npos)
    return;

  // "gamerboy80 was killed by sekerbenimkedim. FINAL KILL!"

  std::string clean = "";
  for (size_t i = 0; i < chat.length(); ++i) {
    if ((unsigned char)chat[i] == 0xC2 && i + 1 < chat.length() &&
        (unsigned char)chat[i + 1] == 0xA7) {
      i += 2;
      continue;
    }
    if ((unsigned char)chat[i] == 0xA7) {
      i += 1;
      continue;
    }
    clean += chat[i];
  }
  while (!clean.empty() && isspace(clean[0]))
    clean.erase(0, 1);
  if (clean.empty())
    return;

  std::string victim;
  if (clean[0] == '[') {
    size_t firstSpace = clean.find(' ');
    if (firstSpace != std::string::npos) {
      std::string afterRank = clean.substr(firstSpace + 1);
      size_t endOfName = afterRank.find(' ');
      if (endOfName != std::string::npos) {
        victim = afterRank.substr(0, endOfName);
      }
    }
  } else {
    size_t firstSpace = clean.find(' ');
    if (firstSpace != std::string::npos) {
      victim = clean.substr(0, firstSpace);
    }
  }

  if (!victim.empty()) {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    if (ChatInterceptor::g_playerStatsMap.find(victim) !=
        ChatInterceptor::g_playerStatsMap.end()) {
      ChatInterceptor::g_playerStatsMap.erase(victim);
      Logger::info("Player removed from GUI due to FINAL KILL: %s",
                   victim.c_str());
    }
  }
}

static void detectBedDestructionFromLine(const std::string &chat) {
  std::string clean = "";
  for (size_t i = 0; i < chat.length(); ++i) {
    if ((unsigned char)chat[i] == 0xC2 && i + 1 < chat.length() &&
        (unsigned char)chat[i + 1] == 0xA7) {
      i += 2;
      continue;
    }
    if ((unsigned char)chat[i] == 0xA7) {
      i += 1;
      continue;
    }
    clean += chat[i];
  }

  if (clean.find("BED DESTRUCTION >") != std::string::npos) {
    // another useless check nvm this
  }
}

static void updateTeamsFromScoreboard() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  jobject mcObj = nullptr;
  if (m_getMc)
    mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj && theMc)
    mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;
  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  if (!f_world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass worldCls =
      lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
  if (!worldCls) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jmethodID m_getScoreboard = env->GetMethodID(
      worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(
        worldCls, "func_96441_U", "()Lnet/minecraft/scoreboard/Scoreboard;");
  }
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(worldCls, "Z", "()Lauo;");
  }

  if (!m_getScoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject scoreboard = env->CallObjectMethod(world, m_getScoreboard);
  if (!scoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass sbCls = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
  if (!sbCls) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jmethodID m_getPlayersTeam = env->GetMethodID(
      sbCls, "getPlayersTeam",
      "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam = env->GetMethodID(
        sbCls, "func_96509_i",
        "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  }
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam =
        env->GetMethodID(sbCls, "h", "(Ljava/lang/String;)Laul;");
  }

  if (!m_getPlayersTeam) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
  if (!teamCls)
    return;
  jmethodID m_getColorPrefixStatic =
      teamCls ? env->GetStaticMethodID(teamCls, "getColorPrefix",
                                       "(Lnet/minecraft/scoreboard/"
                                       "ScorePlayerTeam;)Ljava/lang/String;")
              : nullptr;
  jmethodID m_getColorPrefixInst =
      teamCls
          ? env->GetMethodID(teamCls, "getColorPrefix", "()Ljava/lang/String;")
          : nullptr;
  jmethodID m_getPrefix =
      teamCls ? env->GetMethodID(teamCls, "getPrefix", "()Ljava/lang/String;")
              : nullptr;
  if (!m_getPrefix && teamCls) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPrefix =
        env->GetMethodID(teamCls, "func_96668_e", "()Ljava/lang/String;");
  }
  if (!m_getPrefix && teamCls) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPrefix = env->GetMethodID(teamCls, "e", "()Ljava/lang/String;");
  }
  jmethodID m_getColorPrefixSrg =
      teamCls
          ? env->GetMethodID(teamCls, "func_96668_e", "()Ljava/lang/String;")
          : nullptr;

  for (const std::string &name : g_onlinePlayers) {
    jstring jn = env->NewStringUTF(name.c_str());
    jobject team = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jn);
    const char *tstr = "";
    if (team) {
      jstring pref = nullptr;
      if (m_getColorPrefixStatic)
        pref = (jstring)env->CallStaticObjectMethod(
            teamCls, m_getColorPrefixStatic, team);
      if (!pref && m_getColorPrefixInst)
        pref = (jstring)env->CallObjectMethod(team, m_getColorPrefixInst);
      if (!pref && m_getPrefix)
        pref = (jstring)env->CallObjectMethod(team, m_getPrefix);
      if (!pref && m_getColorPrefixSrg)
        pref = (jstring)env->CallObjectMethod(team, m_getColorPrefixSrg);
      if (pref) {
        const char *utf = env->GetStringUTFChars(pref, 0);
        if (utf) {
          const char *sect = strchr(utf, '\xC2');
          char code = 0;
          const char *raw = strchr(utf, '\xA7');
          if (raw && raw[1])
            code = raw[1];
          if (!code && sect) {
            const unsigned char *u = (const unsigned char *)utf;
            for (size_t i = 0; u[i]; ++i) {
              if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
                code = (char)u[i + 2];
                break;
              }
            }
          }
          if (code) {
            std::string tname = teamFromColorCode(code);
            if (!tname.empty())
              g_playerTeamColor[name] = tname;
          }
          env->ReleaseStringUTFChars(pref, utf);
        }
        env->DeleteLocalRef(pref);
      }
      env->DeleteLocalRef(team);
    }
    env->DeleteLocalRef(jn);
  }
  env->DeleteLocalRef(scoreboard);
  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

static std::string resolveTeamForNameEx(JNIEnv *env, const std::string &name,
                                        jobject scoreboard,
                                        jmethodID m_getPlayersTeam,
                                        jclass teamCls, jmethodID m_getPrefix) {
  if (!env || !scoreboard || !m_getPlayersTeam || !teamCls || !m_getPrefix)
    return std::string();

  jstring jname = env->NewStringUTF(name.c_str());
  jobject teamObj = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jname);
  env->ExceptionClear();
  env->DeleteLocalRef(jname);

  std::string result;
  if (teamObj) {
    jstring pref = (jstring)env->CallObjectMethod(teamObj, m_getPrefix);
    env->ExceptionClear();
    if (pref) {
      const unsigned char *u =
          (const unsigned char *)env->GetStringUTFChars(pref, 0);
      char code = 0;
      if (u) {
        for (size_t i = 0; u[i]; ++i) {
          if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
            code = (char)u[i + 2];
            break;
          }
        }
        env->ReleaseStringUTFChars(pref, (const char *)u);
      }
      if (code) {
        std::string tname = teamFromColorCode(code);
        if (!tname.empty())
          result = tname;
      }
      env->DeleteLocalRef(pref);
    }
    env->DeleteLocalRef(teamObj);
  }
  return result;
}

static std::string resolveTeamForName(const std::string &name) {
  JNIEnv *env = lc->getEnv();
  if (!g_initialized || !env)
    return std::string();

  if (!g_localName.empty() && name == g_localName) {
    if (!g_localTeam.empty())
      return g_localTeam;
  }

  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    auto itT = g_playerTeamColor.find(name);
    if (itT != g_playerTeamColor.end() && !itT->second.empty()) {
      return itT->second;
    }
  }

  g_jCache.init(env);

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return std::string();
  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
  if (!mcObj)
    return std::string();

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

  std::string result;
  if (world) {
    jmethodID m_getScoreboard = g_jCache.m_getScoreboard;
    jobject scoreboard = m_getScoreboard
                             ? env->CallObjectMethod(world, m_getScoreboard)
                             : nullptr;
    env->ExceptionClear();
    if (scoreboard) {
      result =
          resolveTeamForNameEx(env, name, scoreboard, g_jCache.m_getPlayersTeam,
                               g_jCache.teamCls, g_jCache.m_getPrefix);
      env->DeleteLocalRef(scoreboard);
    }
    env->DeleteLocalRef(world);
  }
  env->DeleteLocalRef(mcObj);
  return result;
}

static void updateTabListStats() {
  JNIEnv *env = lc->getEnv();
  jobject iter = nullptr;
  if (!g_initialized || !env)
    return;

  static bool s_firstTick = true;
  if (s_firstTick) {
    if (Config::isGlobalDebugEnabled()) {
      ChatSDK::showPrefixed("§a[DEBUG] updateTabListStats heartbeat active.");
    }
    s_firstTick = false;
  }

  static ULONGLONG lastUpdate = 0;
  ULONGLONG now = GetTickCount64();

  bool isTabEnabled = Config::isTabEnabled();
  if (!isTabEnabled) {
    static ULONGLONG lastWarn = 0;
    if (now - lastWarn > 30000) {
      if (Config::isGlobalDebugEnabled()) {
        ChatSDK::showPrefixed(
            "§7[DEBUG] Detection skipped: Tab is disabled in Config.");
      }
      lastWarn = now;
    }
  }

  static bool s_wasTabEnabled = false;
  bool forceReset = s_wasTabEnabled && !isTabEnabled;
  s_wasTabEnabled = isTabEnabled;

  bool doTabUpdate =
      (isTabEnabled && (now - lastUpdate >= (g_inHypixelGame ? 20 : 50))) ||
      forceReset;
  if (doTabUpdate && isTabEnabled)
    lastUpdate = now;

  if (!isTabEnabled && !forceReset)
    return;
  if (!env)
    return;

  static jclass mcCls = nullptr;
  static jfieldID theMc = nullptr;
  static jmethodID m_getNet = nullptr;
  static jclass nhCls = nullptr;
  static jmethodID m_getMap = nullptr;
  static jclass localCollCls = nullptr;
  static jmethodID m_size = nullptr;

  if (!mcCls) {
    mcCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.client.Minecraft"));
    if (mcCls) {
      theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                    "Lnet/minecraft/client/Minecraft;");
      if (!theMc) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                      "Lnet/minecraft/client/Minecraft;");
      }
      if (!theMc) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
      }

      m_getNet = env->GetMethodID(
          mcCls, "getNetHandler",
          "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
      if (!m_getNet) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getNet = env->GetMethodID(
            mcCls, "func_147114_u",
            "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
      }
      if (!m_getNet) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getNet = env->GetMethodID(mcCls, "ay", "()Lbcy;");
      }
    } else {
      ChatSDK::showPrefixed("§cCRITICAL: FAILED to find Minecraft class!");
      Logger::log(Config::DebugCategory::GUI,
                  "FAILED to find Minecraft class!");
    }

    jclass tmpNh =
        lc->GetClass("net.minecraft.client.network.NetHandlerPlayClient");
    if (!tmpNh) {
      ChatSDK::showPrefixed(
          "§cCRITICAL: FAILED to find NetHandlerPlayClient class!");
      return;
    }

    nhCls = (jclass)env->NewGlobalRef(tmpNh);
    m_getMap =
        env->GetMethodID(nhCls, "getPlayerInfoMap", "()Ljava/util/Collection;");
    if (!m_getMap) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getMap =
          env->GetMethodID(nhCls, "func_175106_d", "()Ljava/util/Collection;");
    }
    if (!m_getMap) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getMap = env->GetMethodID(nhCls, "d", "()Ljava/util/Collection;");
    }
    if (!m_getMap) {
      if (Config::isGlobalDebugEnabled()) {
        Logger::log(Config::DebugCategory::GUI,
                    "FAILED: getPlayerInfoMap. Dumping NetHandler methods...");
        jint mCount = 0;
        jmethodID *pM = nullptr;
        lc->jvmti->GetClassMethods(nhCls, &mCount, &pM);
        for (int i = 0; i < mCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetMethodName(pM[i], &n, &s, nullptr);
          if (n && s)
            Logger::log(Config::DebugCategory::GUI, "Method: %s | %s", n, s);
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pM)
          lc->jvmti->Deallocate((unsigned char *)pM);
      }
    }

    jclass tmpColl = env->FindClass("java/util/Collection");
    if (tmpColl) {
      localCollCls = (jclass)env->NewGlobalRef(tmpColl);
      m_size = env->GetMethodID(localCollCls, "size", "()I");
    }
  }

  if (!mcCls)
    return;
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
  if (!mcObj) {
    static bool s_oneTime = false;
    if (!s_oneTime) {
      ChatSDK::showPrefixed("§cCRITICAL: theMinecraft object is NULL!");
      s_oneTime = true;
    }
    return;
  }

  jobject nh = m_getNet ? env->CallObjectMethod(mcObj, m_getNet) : nullptr;
  if (!nh) {
    // brute force
    jmethodID bf_getNet = lc->FindMethodBySignature(
        mcCls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
    if (!bf_getNet)
      bf_getNet = lc->FindMethodBySignature(mcCls, "()Lbcy;");
    if (bf_getNet)
      nh = env->CallObjectMethod(mcObj, bf_getNet);
  }

  if (!nh) {
    ChatSDK::showPrefixed("§cCRITICAL: NetHandler object is NULL!");
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject col = m_getMap ? env->CallObjectMethod(nh, m_getMap) : nullptr;
  if (!col) {
    ChatSDK::showPrefixed("§cCRITICAL: PlayerInfoMap collection is NULL!");
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
    return;
  }

  int playerCount = m_size ? env->CallIntMethod(col, m_size) : 0;

  bool appearsToBeLobby = true;
  bool hasStrictGameKeywords = false;
  std::string detectedServer = "unknown";
  std::string detectionReason = "Default (Lobby)";
  std::string footerText = "";
  std::string compSig = "Lnet/minecraft/util/IChatComponent;";

  static jfieldID f_currServer = nullptr;
  static jclass serverDataCls = nullptr;
  static jfieldID f_serverMOTD = nullptr;

  static jfieldID f_gui = nullptr;
  static jclass guiCls = nullptr;
  static jfieldID f_tab = nullptr;
  static jclass tabCls = nullptr;
  static jfieldID f_footer = nullptr;
  static jclass compCls = nullptr;
  static jmethodID m_getUnf = nullptr;

  if (!f_currServer && mcCls) {
    f_currServer =
        env->GetFieldID(mcCls, "currentServerData",
                        "Lnet/minecraft/client/multiplayer/ServerData;");
    if (!f_currServer) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_currServer =
          env->GetFieldID(mcCls, "field_71422_O",
                          "Lnet/minecraft/client/multiplayer/ServerData;");
    }
    if (!f_currServer) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_currServer = env->GetFieldID(mcCls, "Q", "Lbha;");
    }

    jclass tmpSD = lc->GetClass("net.minecraft.client.multiplayer.ServerData");
    if (!tmpSD)
      return;
    if (tmpSD) {
      serverDataCls = (jclass)env->NewGlobalRef(tmpSD);
      f_serverMOTD =
          env->GetFieldID(serverDataCls, "serverMOTD", "Ljava/lang/String;");
      if (!f_serverMOTD) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_serverMOTD = env->GetFieldID(serverDataCls, "field_78847_f",
                                       "Ljava/lang/String;");
      }
    }

    f_gui = env->GetFieldID(mcCls, "ingameGUI",
                            "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "field_71456_v",
                              "Lnet/minecraft/client/gui/GuiIngame;");
    }
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "q", "Laxe;");
    }
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "q", "Lavo;");
    }

    jclass tmpGui = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (tmpGui) {
      guiCls = (jclass)env->NewGlobalRef(tmpGui);

      jclass clsCls = env->GetObjectClass(guiCls);
      jmethodID mid_getName =
          env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
      jstring jsName = (jstring)env->CallObjectMethod(guiCls, mid_getName);
      const char *cName = env->GetStringUTFChars(jsName, 0);
      std::string guiSig =
          "L" +
          (std::string)(cName ? cName : "net/minecraft/client/gui/GuiIngame") +
          ";";
      for (size_t i = 0; i < guiSig.length(); ++i)
        if (guiSig[i] == '.')
          guiSig[i] = '/';
      if (cName)
        env->ReleaseStringUTFChars(jsName, cName);

      if (!f_gui) {
        f_gui = lc->FindFieldBySignature(mcCls, guiSig.c_str());
        if (f_gui)
          Logger::log(Config::DebugCategory::GUI, "AUTO-FOUND GUI: %s",
                      guiSig.c_str());
      }

      f_tab = env->GetFieldID(guiCls, "overlayPlayerList",
                              "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;");
      if (!f_tab) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_tab = env->GetFieldID(guiCls, "v", "Lawh;");
      }
      if (!f_tab) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_tab =
            env->GetFieldID(guiCls, "field_175181_C",
                            "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;");
      }

      if (!f_tab) {
        jint fCount = 0;
        jfieldID *pF = nullptr;
        lc->jvmti->GetClassFields(guiCls, &fCount, &pF);
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(guiCls, pF[i], &n, &s, nullptr);
          if (n && s &&
              (std::string(s).find("GuiPlayerTabOverlay") !=
                   std::string::npos ||
               std::string(s) == "Lawh;")) {
            f_tab = pF[i];
            if (f_tab)
              Logger::log(Config::DebugCategory::GUI, "AUTO-FOUND TAB: %s", s);
            break;
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pF)
          lc->jvmti->Deallocate((unsigned char *)pF);
      }
    }

    jclass tmpTab =
        lc->GetClass("net.minecraft.client.gui.GuiPlayerTabOverlay");
    if (tmpTab) {
      tabCls = (jclass)env->NewGlobalRef(tmpTab);

      jclass tmpComp = lc->GetClass("net.minecraft.util.IChatComponent");
      compSig = "Lnet/minecraft/util/IChatComponent;";
      if (tmpComp) {
        jclass compClsCls = env->GetObjectClass(tmpComp);
        jmethodID comp_getName =
            env->GetMethodID(compClsCls, "getName", "()Ljava/lang/String;");
        jstring jsCompName =
            (jstring)env->CallObjectMethod(tmpComp, comp_getName);
        const char *cCompName = env->GetStringUTFChars(jsCompName, 0);
        if (cCompName) {
          compSig = "L" + std::string(cCompName) + ";";
          for (size_t i = 0; i < compSig.length(); ++i)
            if (compSig[i] == '.')
              compSig[i] = '/';
          env->ReleaseStringUTFChars(jsCompName, cCompName);
        }
      }

      f_footer = env->GetFieldID(tabCls, "footer", compSig.c_str());
      if (!f_footer) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_footer = env->GetFieldID(tabCls, "field_175245_I", compSig.c_str());
      }

      if (!f_footer) {
        // scan
        jint fCount = 0;
        jfieldID *pF = nullptr;
        lc->jvmti->GetClassFields(tabCls, &fCount, &pF);
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(tabCls, pF[i], &n, &s, nullptr);
          if (n && s && std::string(s) == compSig) {
            f_footer = pF[i];
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pF)
          lc->jvmti->Deallocate((unsigned char *)pF);
      }
    }

    jclass tmpComp = lc->GetClass("net.minecraft.util.IChatComponent");
    if (!tmpComp)
      return;
    if (tmpComp) {
      compCls = (jclass)env->NewGlobalRef(tmpComp);
      m_getUnf = env->GetMethodID(compCls, "getUnformattedText",
                                  "()Ljava/lang/String;");
      if (!m_getUnf) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getUnf =
            env->GetMethodID(compCls, "func_150260_c", "()Ljava/lang/String;");
      }
      if (!m_getUnf) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getUnf = env->GetMethodID(compCls, "c", "()Ljava/lang/String;");
      }
    }
  }

  jobject serverData =
      f_currServer ? env->GetObjectField(mcObj, f_currServer) : nullptr;
  if (serverData) {
    jstring motd = f_serverMOTD
                       ? (jstring)env->GetObjectField(serverData, f_serverMOTD)
                       : nullptr;
    if (motd) {
      const char *motdUtf = env->GetStringUTFChars(motd, 0);
      if (motdUtf) {
        std::string motdStr = motdUtf;
        if (motdStr.find("Portal") != std::string::npos ||
            motdStr.find("Lobby") != std::string::npos) {
          appearsToBeLobby = true;
          g_explicitLobbySignal = true;
          detectionReason = "MOTD (Lobby Keywords)";
        }
        env->ReleaseStringUTFChars(motd, motdUtf);
      }
      env->DeleteLocalRef(motd);
    }
    env->DeleteLocalRef(serverData);
  }

  jobject gui = f_gui ? env->GetObjectField(mcObj, f_gui) : nullptr;

  if (gui) {
    jobject tab = f_tab ? env->GetObjectField(gui, f_tab) : nullptr;
    if (tab) {
      // again brute force
      std::string allTabText = "";
      jint fCount = 0;
      jfieldID *pF = nullptr;
      if (lc->jvmti->GetClassFields(tabCls, &fCount, &pF) == JVMTI_ERROR_NONE) {
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(tabCls, pF[i], &n, &s, nullptr);
          if (n && s && std::string(s) == compSig) {
            jobject comp = env->GetObjectField(tab, pF[i]);
            if (comp) {
              jstring js = m_getUnf
                               ? (jstring)env->CallObjectMethod(comp, m_getUnf)
                               : nullptr;
              if (js) {
                const char *utf = env->GetStringUTFChars(js, 0);
                if (utf) {
                  std::string raw = utf;
                  for (size_t k = 0; k < raw.length(); ++k) {
                    if ((unsigned char)raw[k] == 0xC2 && k + 1 < raw.length() &&
                        (unsigned char)raw[k + 1] == 0xA7) {
                      k += 2;
                      continue;
                    }
                    if ((unsigned char)raw[k] == 0xA7) {
                      k += 1;
                      continue;
                    }
                    allTabText += raw[k];
                  }
                  allTabText += " ";
                  env->ReleaseStringUTFChars(js, utf);
                }
                env->DeleteLocalRef(js);
              }
              env->DeleteLocalRef(comp);
            }
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        lc->jvmti->Deallocate((unsigned char *)pF);
      }

      std::string footerClean = allTabText;
      if (now - g_lastDetectionLogTick >= 3000) {
        if (Config::isGlobalDebugEnabled()) {
          std::string preview = footerClean;
          if (preview.length() > 100)
            preview = preview.substr(0, 100) + "...";
          Logger::log(Config::DebugCategory::GUI, "Combined Tab Text: '%s'",
                      preview.c_str());
        }
      }

      std::string lowerFooter = footerClean;
      std::transform(lowerFooter.begin(), lowerFooter.end(),
                     lowerFooter.begin(), ::tolower);

      bool foundFinalKills =
          (lowerFooter.find("final kills") != std::string::npos);
      bool foundBedsBroken =
          (lowerFooter.find("beds broken") != std::string::npos ||
           lowerFooter.find("beds b") != std::string::npos);
      bool foundKills = (lowerFooter.find("kills:") != std::string::npos ||
                         lowerFooter.find("kills :") != std::string::npos);

      if (foundFinalKills || foundBedsBroken ||
          (foundKills && !foundBedsBroken)) {
        hasStrictGameKeywords = true;
      }

      size_t srvPos = footerClean.find("Server: ");
      if (srvPos != std::string::npos) {
        std::string srv = footerClean.substr(srvPos + 8);
        size_t space = srv.find_first_of(" \n\r");
        if (space != std::string::npos)
          srv = srv.substr(0, space);
        detectedServer = srv;
      }
      env->DeleteLocalRef(tab);
    }
    env->DeleteLocalRef(gui);
  }

  static jfieldID s_f_world = nullptr;
  static jclass s_worldCls = nullptr;
  static jmethodID s_m_getSB = nullptr;
  static jclass s_sbCls = nullptr;
  static jmethodID s_m_getObj = nullptr;
  static jclass s_objCls = nullptr;
  static jmethodID s_m_getDisp = nullptr;

  if (!s_f_world && mcCls) {
    s_f_world = env->GetFieldID(
        mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      s_f_world =
          env->GetFieldID(mcCls, "field_71441_e",
                          "Lnet/minecraft/client/multiplayer/WorldClient;");
    }
    if (!s_f_world) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      s_f_world =
          env->GetFieldID(mcCls, "field_71441_e",
                          "Lnet/minecraft/client/multiplayer/WorldClient;");
    }
    if (!s_f_world) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      s_f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
    }

    jclass tmpWorld =
        lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
    if (!tmpWorld)
      return;
    if (tmpWorld) {
      s_worldCls = (jclass)env->NewGlobalRef(tmpWorld);
      s_m_getSB = env->GetMethodID(s_worldCls, "getScoreboard",
                                   "()Lnet/minecraft/scoreboard/Scoreboard;");
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_96441_U",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_96441_as",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_72967_aN",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_72883_A",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "Z", "()Lauo;");
      }
    }

    jclass tmpSb = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
    if (!tmpSb)
      return;
    if (tmpSb) {
      s_sbCls = (jclass)env->NewGlobalRef(tmpSb);
      s_m_getObj =
          env->GetMethodID(s_sbCls, "getObjectiveInDisplaySlot",
                           "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      if (!s_m_getObj) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getObj =
            env->GetMethodID(s_sbCls, "func_96539_a",
                             "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      }
      if (!s_m_getObj) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getObj = env->GetMethodID(s_sbCls, "a", "(I)Lauk;");
      }
    }

    jclass tmpObj = lc->GetClass("net.minecraft.scoreboard.ScoreObjective");
    if (!tmpObj)
      return;
    if (tmpObj) {
      s_objCls = (jclass)env->NewGlobalRef(tmpObj);
      s_m_getDisp =
          env->GetMethodID(s_objCls, "getDisplayName", "()Ljava/lang/String;");
      if (!s_m_getDisp) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getDisp =
            env->GetMethodID(s_objCls, "func_96678_d", "()Ljava/lang/String;");
      }
      if (!s_m_getDisp) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getDisp = env->GetMethodID(s_objCls, "d", "()Ljava/lang/String;");
      }
    }
  }

  jobject world = s_f_world ? env->GetObjectField(mcObj, s_f_world) : nullptr;
  if (world) {
    jobject sb = s_m_getSB ? env->CallObjectMethod(world, s_m_getSB) : nullptr;
    if (sb) {
      jobject obj =
          s_m_getObj ? env->CallObjectMethod(sb, s_m_getObj, 1) : nullptr;
      if (obj) {
        jstring dispJ = s_m_getDisp
                            ? (jstring)env->CallObjectMethod(obj, s_m_getDisp)
                            : nullptr;
        if (dispJ) {
          const char *utf = env->GetStringUTFChars(dispJ, 0);
          if (utf) {
            std::string sbTitle = utf;
            if (now - g_lastDetectionLogTick >= 10000) {
              if (g_lastDetectedModeName != "SCOREBOARD") {
                Logger::log(Config::DebugCategory::GameDetection,
                            "Raw Scoreboard: %s", sbTitle.c_str());
                g_lastDetectedModeName = "SCOREBOARD";
                g_lastDetectionLogTick = now;
              }
            }
            std::string sbClean;
            for (size_t i = 0; i < sbTitle.length(); ++i) {
              if ((unsigned char)sbTitle[i] == 0xC2 &&
                  i + 1 < sbTitle.length() &&
                  (unsigned char)sbTitle[i + 1] == 0xA7) {
                i += 2;
                continue;
              }
              if ((unsigned char)sbTitle[i] == 0xA7) {
                i += 1;
                continue;
              }
              sbClean += (char)toupper(sbTitle[i]);
            }

            if (sbClean.find("BED WARS") != std::string::npos ||
                sbClean.find("SKYWARS") != std::string::npos ||
                sbClean.find("DUELS") != std::string::npos ||
                sbClean.find("WARS") != std::string::npos ||
                sbClean.find("THE BRIDGE") != std::string::npos ||
                sbClean.find("TNT") != std::string::npos ||
                sbClean.find("MURDER") != std::string::npos ||
                sbClean.find("GAMES") != std::string::npos) {

              bool isLobbyTitle =
                  (sbClean.find("LOBBY") != std::string::npos ||
                   sbClean.find("WAITING") != std::string::npos ||
                   sbClean.find("STARTING") != std::string::npos);

              if (hasStrictGameKeywords && !isLobbyTitle) {
                appearsToBeLobby = false;
                detectionReason = "Tab Footer Keywords (Game)";
              } else {
                appearsToBeLobby = true;
                if (isLobbyTitle) {
                  detectionReason = "Scoreboard (Lobby/Waiting Keywords)";
                } else {
                  detectionReason = "No Game Keywords in Tab (Lobby)";
                }
              }
            } else if (sbClean == "HYPIXEL" ||
                       sbClean.find("LOBBY") != std::string::npos) {
              appearsToBeLobby = true;
              detectionReason = (sbClean.find("LOBBY") != std::string::npos)
                                    ? "Scoreboard (Lobby Keyword)"
                                    : "Scoreboard (Generic HYPIXEL)";
            }
            env->ReleaseStringUTFChars(dispJ, utf);
          }
          env->DeleteLocalRef(dispJ);
        }
        env->DeleteLocalRef(obj);
      }
      env->DeleteLocalRef(sb);
    }
    env->DeleteLocalRef(world);
  }

  if (detectedServer != "unknown") {
    std::string srvLower = detectedServer;
    std::transform(srvLower.begin(), srvLower.end(), srvLower.begin(),
                   ::tolower);
    if (srvLower.find("lobby") != std::string::npos ||
        srvLower.find("mega") != std::string::npos) {
      appearsToBeLobby = true;
      detectionReason = "Server ID (Lobby)";
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "LOBBY (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection, "Server: %s (LOBBY)",
                    detectedServer.c_str());
        g_lastDetectedModeName = "LOBBY (Server ID)";
        g_lastDetectionLogTick = now;
      }
    } else if (srvLower.find("mini") != std::string::npos ||
               srvLower.find("bed") != std::string::npos) {
      appearsToBeLobby = false;
      detectionReason = "Server ID (Game)";
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "GAME (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection, "Server: %s (GAME)",
                    detectedServer.c_str());
        g_lastDetectedModeName = "GAME (Server ID)";
        g_lastDetectionLogTick = now;
      }
    } else {
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "UNKNOWN (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection,
                    "Server: %s (UNKNOWN)", detectedServer.c_str());
        g_lastDetectedModeName = "UNKNOWN (Server ID)";
        g_lastDetectionLogTick = now;
      }
    }
  }

  if (hasStrictGameKeywords &&
      detectionReason != "Scoreboard (Lobby Keyword)" &&
      detectionReason != "Server ID (Lobby)") {
    appearsToBeLobby = false;
    detectionReason = "Tab Keywords Priority (Game)";
  }

  bool detectedLobby = appearsToBeLobby;
  if (detectedLobby) {
    g_lobbyGraceTicks++;
  } else {
    g_lobbyGraceTicks = 0;
  }

  if (now - g_lastDetectionLogTick >= 3000) {
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Current Mode: %s | Reason: %s",
                  (detectedLobby ? "LOBBY" : "GAME"), detectionReason.c_str());
    }
    g_lastDetectionLogTick = now;
  }

  bool shouldBeInGame = g_inHypixelGame;
  if (g_lobbyGraceTicks >= 10 || g_explicitLobbySignal) {
    shouldBeInGame = false;
    g_explicitLobbySignal = false;
  } else if (g_lobbyGraceTicks == 0 && hasStrictGameKeywords) {
    shouldBeInGame = true;
  } else if (g_lobbyGraceTicks == 0 && !hasStrictGameKeywords) {
    shouldBeInGame = false;
  }

  if (shouldBeInGame != g_inHypixelGame) {
    bool transitionToGame = shouldBeInGame && !g_inHypixelGame;
    g_inHypixelGame = shouldBeInGame;
    if (g_inHypixelGame) {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Detected Hypixel GAME session (Confirmed) - RESETTING FOR "
                  "FRESH START");

      {
        std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
        ChatInterceptor::g_playerStatsMap.clear();
      }
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_persistentStatsCache.clear();
      }
      {
        std::lock_guard<std::mutex> lock(g_uuidMapMutex);
        g_playerUuidMap.clear();
      }

      g_processedPlayers.clear();
      {
        std::lock_guard<std::mutex> lock(g_alertedMutex);
        g_alertedPlayers.clear();
      }
      g_chatPrintedPlayers.clear();
      g_manualPushedPlayers.clear();
      g_forceChatOutputPlayers.clear();

      // clear caches mmm
      Urchin::clearCache();
      Seraph::clearCache();

      if (Config::isGlobalDebugEnabled()) {
        Render::NotificationManager::getInstance()->add(
            "System", "Game Started: Stats Reset",
            Render::NotificationType::Success);
      }
      syncTeamColors();
    } else {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Detected Hypixel LOBBY session (Confirmed)");
      if (Config::isGlobalDebugEnabled()) {
        Render::NotificationManager::getInstance()->add(
            "System", "Lobby Session Detected",
            Render::NotificationType::Warning);
      }
      resetGameCache();
    }
  }

  static jclass iterCls = nullptr, npiCls = nullptr, profCls = nullptr,
                uuidCls = nullptr, cctCls = nullptr, collCls = nullptr;
  static jmethodID m_iter = nullptr, m_has = nullptr, m_next = nullptr,
                   m_setDisp = nullptr;
  static jmethodID m_getProf = nullptr, m_getName = nullptr, m_getId = nullptr,
                   m_uuidToString = nullptr, cctInit = nullptr;
  static jfieldID f_gpName = nullptr;

  if (!iterCls) {
    collCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.Collection"));
    iterCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.Iterator"));
    npiCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.client.network.NetworkPlayerInfo"));
    profCls = (jclass)env->NewGlobalRef(
        lc->GetClass("com.mojang.authlib.GameProfile"));
    uuidCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.UUID"));
    cctCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.util.ChatComponentText"));

    m_iter = env->GetMethodID(collCls, "iterator", "()Ljava/util/Iterator;");
    m_has = env->GetMethodID(iterCls, "hasNext", "()Z");
    m_next = env->GetMethodID(iterCls, "next", "()Ljava/lang/Object;");

    m_setDisp = env->GetMethodID(npiCls, "setDisplayName",
                                 "(Lnet/minecraft/util/IChatComponent;)V");
    if (!m_setDisp) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_setDisp = env->GetMethodID(npiCls, "func_178859_a",
                                   "(Lnet/minecraft/util/IChatComponent;)V");
    }
    if (!m_setDisp) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_setDisp = env->GetMethodID(npiCls, "a", "(Leu;)V");
    }
    m_getProf = env->GetMethodID(npiCls, "getGameProfile",
                                 "()Lcom/mojang/authlib/GameProfile;");
    if (!m_getProf) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getProf = env->GetMethodID(npiCls, "func_178845_a",
                                   "()Lcom/mojang/authlib/GameProfile;");
    }
    if (!m_getProf) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getProf =
          env->GetMethodID(npiCls, "a", "()Lcom/mojang/authlib/GameProfile;");
    }

    m_getName = env->GetMethodID(profCls, "getName", "()Ljava/lang/String;");
    m_getId = env->GetMethodID(profCls, "getId", "()Ljava/util/UUID;");
    f_gpName = env->GetFieldID(profCls, "name", "Ljava/lang/String;");

    m_uuidToString =
        env->GetMethodID(uuidCls, "toString", "()Ljava/lang/String;");
    cctInit = env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
  }

  if (!iterCls || !m_iter || !m_has || !m_next) {
    env->DeleteLocalRef(col);
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
    return;
  }

  world =
      (mcObj && s_f_world) ? env->GetObjectField(mcObj, s_f_world) : nullptr;

  g_jCache.init(env);

  jclass worldCls = g_jCache.worldCls;
  jmethodID m_getSB = g_jCache.m_getScoreboard;
  jclass sbCls = g_jCache.sbCls;
  jmethodID m_getObj = g_jCache.m_getObjectiveInDisplaySlot;
  jmethodID m_getObjByName = g_jCache.m_getObjective;
  jmethodID m_getScore = g_jCache.m_getValueFromObjective;
  jclass scoreCls = g_jCache.scoreCls;
  jmethodID m_getVal = g_jCache.m_getScorePoints;
  jmethodID m_setVal = g_jCache.m_setScorePoints;
  f_gpName = g_jCache.f_gpName;

  std::string currentSortMode = Config::getSortMode();
  std::vector<std::string> currentNames;

  static ULONGLONG lastExtraction = 0;
  bool doExtraction = (now - lastExtraction >= 50);

  if (doExtraction && m_has && m_next) {
    lastExtraction = now;
    int processedCount = 0;
    iter = env->CallObjectMethod(col, m_iter);
    int extractionsThisFrame = 0;
    if (iter) {
      while (env->CallBooleanMethod(iter, m_has)) {
        if (lc->CheckException())
          break;
        if (env->PushLocalFrame(50) < 0)
          break;

        jobject info = env->CallObjectMethod(iter, m_next);
        if (info) {
          jobject prof =
              m_getProf ? env->CallObjectMethod(info, m_getProf) : nullptr;
          if (prof) {
            jstring jname =
                m_getName ? (jstring)env->CallObjectMethod(prof, m_getName)
                          : nullptr;
            if (jname) {
              const char *nameUtf = env->GetStringUTFChars(jname, 0);
              std::string name(nameUtf);
              bool isObfuscated = (name.find("\xC2\xA7k") != std::string::npos);
              while (true) {
                size_t pos = name.find("\xC2\xA7");
                if (pos == std::string::npos)
                  break;
                if (pos + 3 <= name.length()) {
                  name.erase(pos, 3);
                } else {
                  name.erase(pos);
                  break;
                }
              }
              if (!isObfuscated) {
                currentNames.push_back(name);
              }
              env->ReleaseStringUTFChars(jname, nameUtf);

              if (Config::isNickedBypass() && extractionsThisFrame < 4) {
                bool needsUuid = false;
                {
                  std::lock_guard<std::mutex> lock(g_uuidMapMutex);
                  needsUuid =
                      (g_playerUuidMap.find(name) == g_playerUuidMap.end());
                }
                if (needsUuid) {
                  jobject guid = env->CallObjectMethod(prof, m_getId);
                  if (guid) {
                    jstring jUuid =
                        (jstring)env->CallObjectMethod(guid, m_uuidToString);
                    if (jUuid) {
                      const char *uUtf = env->GetStringUTFChars(jUuid, 0);
                      if (uUtf) {
                        std::string uuidStr = uUtf;
                        env->ReleaseStringUTFChars(jUuid, uUtf);
                        {
                          std::lock_guard<std::mutex> lock(g_uuidMapMutex);
                          g_playerUuidMap[name] = uuidStr;
                        }
                        extractionsThisFrame++;
                      }
                      env->DeleteLocalRef(jUuid);
                    }
                    env->DeleteLocalRef(guid);
                  }
                }
              }
            }
          }
        }

        processedCount++;
        env->PopLocalFrame(nullptr);
        if (processedCount > 500)
          break; // sanity
      }
      if (iter)
        env->DeleteLocalRef(iter);
    }

    {
      std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
      bool needsImmediateTeamSync = false;
      for (const auto &name : currentNames) {
        if (g_playerTeamColor.find(name) == g_playerTeamColor.end()) {
          needsImmediateTeamSync = true;
          break;
        }
      }

      if (!g_inPreGameLobby) {
        g_onlinePlayers = currentNames;
      }

      if (needsImmediateTeamSync && g_inHypixelGame) {
        // better team sync
        updateTeamsFromScoreboard();
        g_lastTeamScanTick = now;
      }
    }
  }

  if (doTabUpdate && m_has && m_next) {
    iter = env->CallObjectMethod(col, m_iter);
    if (!iter) {
      env->DeleteLocalRef(col);
      env->DeleteLocalRef(nh);
      env->DeleteLocalRef(mcObj);
      return;
    }

    {
      static bool s_sbDebugOnce = false;
      if (!s_sbDebugOnce && Config::isGlobalDebugEnabled()) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "§e[HEALTH-DBG] world=%p s_m_getSB=%p s_m_getObj=%p "
                 "s_f_world=%p mcObj=%p m_getSB(jcache)=%p",
                 world, s_m_getSB, s_m_getObj, s_f_world, mcObj,
                 g_jCache.m_getScoreboard);
        ChatSDK::showPrefixed(buf);
        s_sbDebugOnce = true;
      }
    }
    jobject scoreboard = (world && s_m_getSB)
                             ? env->CallObjectMethod(world, s_m_getSB)
                             : nullptr;
    env->ExceptionClear();

    jobject tabObj = nullptr;
    if (scoreboard && s_m_getObj) {
      tabObj = env->CallObjectMethod(scoreboard, s_m_getObj, 0);
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (env->ExceptionCheck())
      env->ExceptionClear();

    int processedTab = 0;
    while (env->CallBooleanMethod(iter, m_has)) {
      if (lc->CheckException())
        break;

      if (env->PushLocalFrame(100) < 0)
        break;

      jobject info = env->CallObjectMethod(iter, m_next);
      if (info && m_getProf && m_getName) {
        jobject prof = env->CallObjectMethod(info, m_getProf);
        if (prof) {
          jstring jn = (jstring)env->CallObjectMethod(prof, m_getName);
          if (jn) {
            const char *utf = env->GetStringUTFChars(jn, 0);
            std::string name(utf ? utf : "");
            if (utf)
              env->ReleaseStringUTFChars(jn, utf);

            while (true) {
              size_t pos = name.find("\xC2\xA7");
              if (pos == std::string::npos)
                break;
              if (pos + 3 <= name.length()) {
                name.erase(pos, 3);
              } else {
                name.erase(pos);
                break;
              }
            }

            if (forceReset || !Config::isTabEnabled() || !g_inHypixelGame) {
              if (m_setDisp)
                env->CallVoidMethod(info, m_setDisp, nullptr);
              if (f_gpName) {
                jstring orig = env->NewStringUTF(name.c_str());
                env->SetObjectField(prof, f_gpName, orig);
                env->DeleteLocalRef(orig);
              }
            } else if (cctInit && m_setDisp) {
              Hypixel::PlayerStats stats;
              bool hasStats = false;
              {
                std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
                auto itS = ChatInterceptor::g_playerStatsMap.find(name);
                if (itS != ChatInterceptor::g_playerStatsMap.end()) {
                  stats = itS->second;
                  hasStats = true;
                }
              }

              std::string teamColorCode = "\xC2\xA7"
                                          "f";
              std::string currentTeam;
              std::string cName = name;

              {
                std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
                auto itTC = g_playerTeamColor.find(name);
                if (itTC != g_playerTeamColor.end() && !itTC->second.empty()) {
                  currentTeam = itTC->second;
                  teamColorCode = mcColorForTeam(currentTeam);
                }
              }

              std::string sortMetric = Config::getSortMode();
              std::transform(sortMetric.begin(), sortMetric.end(),
                             sortMetric.begin(), ::tolower);
              double sortVal = 0;
              if (hasStats) {
                if (sortMetric == "fk")
                  sortVal = (double)stats.bedwarsFinalKills;
                else if (sortMetric == "fkdr")
                  sortVal = (stats.bedwarsFinalDeaths == 0)
                                ? (double)stats.bedwarsFinalKills
                                : (double)stats.bedwarsFinalKills /
                                      stats.bedwarsFinalDeaths;
                else if (sortMetric == "wins")
                  sortVal = (double)stats.bedwarsWins;
                else if (sortMetric == "wlr")
                  sortVal =
                      (stats.bedwarsLosses == 0)
                          ? (double)stats.bedwarsWins
                          : (double)stats.bedwarsWins / stats.bedwarsLosses;
                else if (sortMetric == "star")
                  sortVal = (double)stats.bedwarsStar;
                else if (sortMetric == "ws")
                  sortVal = (double)stats.winstreak;
              }
              if (sortMetric == "team") {
                if (currentTeam == "Red")
                  sortVal = 100;
                else if (currentTeam == "Blue")
                  sortVal = 200;
                else if (currentTeam == "Green")
                  sortVal = 300;
                else if (currentTeam == "Yellow")
                  sortVal = 400;
                else if (currentTeam == "Aqua")
                  sortVal = 500;
                else if (currentTeam == "White")
                  sortVal = 600;
                else if (currentTeam == "Pink")
                  sortVal = 700;
                else if (currentTeam == "Gray" || currentTeam == "Grey")
                  sortVal = 800;
                else
                  sortVal = 999;
              }

              long rank = (long)(sortVal * 10.0);
              if (Config::isTabSortDescending())
                rank = 9999L - rank;
              if (rank < 0)
                rank = 0;
              if (rank > 9999)
                rank = 9999;
              char rankBuf[8];
              sprintf_s(rankBuf, "%04ld", rank);
              std::string calculatedPrefix = "";
              for (int i = 0; i < 4; ++i) {
                calculatedPrefix += "\xC2\xA7";
                calculatedPrefix += rankBuf[i];
              }

              std::string finalPrefix = calculatedPrefix;
              {
                std::lock_guard<std::mutex> lockR(g_stableRankMutex);
                auto itR = g_stableRankMap.find(name);
                if (hasStats) {
                  g_stableRankMap[name] = calculatedPrefix;
                  finalPrefix = calculatedPrefix;
                } else if (itR != g_stableRankMap.end()) {
                  finalPrefix = itR->second;
                } else if (processedTab > 5 && !currentTeam.empty()) {
                  g_stableRankMap[name] = calculatedPrefix;
                  finalPrefix = calculatedPrefix;
                }
              }

              std::string internalName = finalPrefix + teamColorCode + cName;
              if (f_gpName) {
                if (internalName.length() > 40) {
                  internalName = teamColorCode + cName;
                  if (internalName.length() > 40)
                    internalName = internalName.substr(0, 40);
                }
                jstring newNameObj = env->NewStringUTF(internalName.c_str());
                if (newNameObj) {
                  env->SetObjectField(prof, f_gpName, newNameObj);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  env->DeleteLocalRef(newNameObj);
                }
              }

              std::string fullTabString;
              if (hasStats) {
                if (stats.isNicked) {
                  fullTabString = teamColorCode + name +
                                  " \xC2\xA7"
                                  "4[NICKED]";
                } else {
                  fullTabString =
                      BedwarsStars::GetFormattedLevel(stats.bedwarsStar) + " " +
                      teamColorCode + name;
                  if (Config::isTagsEnabled())
                    fullTabString += stats.tagsDisplay;
                  fullTabString += " \xC2\xA7"
                                   "7: ";

                  std::string dMode = Config::getTabDisplayMode();
                  std::transform(dMode.begin(), dMode.end(), dMode.begin(),
                                 ::tolower);
                  if (dMode == "fk")
                    fullTabString +=
                        colorForFinalKills(stats.bedwarsFinalKills) +
                        std::to_string(stats.bedwarsFinalKills);
                  else if (dMode == "fkdr") {
                    double fkdr = (stats.bedwarsFinalDeaths == 0)
                                      ? (double)stats.bedwarsFinalKills
                                      : (double)stats.bedwarsFinalKills /
                                            stats.bedwarsFinalDeaths;
                    std::ostringstream ss_fkdr;
                    ss_fkdr << std::fixed << std::setprecision(2) << fkdr;
                    fullTabString += colorForFkdr(fkdr) + ss_fkdr.str();
                  } else if (dMode == "wins")
                    fullTabString += colorForWins(stats.bedwarsWins) +
                                     std::to_string(stats.bedwarsWins);
                  else if (dMode == "wlr") {
                    double wlr =
                        (stats.bedwarsLosses == 0)
                            ? (double)stats.bedwarsWins
                            : (double)stats.bedwarsWins / stats.bedwarsLosses;
                    std::ostringstream ss_wlr;
                    ss_wlr << std::fixed << std::setprecision(2) << wlr;
                    fullTabString += colorForWlr(wlr) + ss_wlr.str();
                  } else if (dMode == "star" || dMode == "lvl")
                    fullTabString += "\xC2\xA7"
                                     "6" +
                                     std::to_string(stats.bedwarsStar) +
                                     "\xC2\xA7"
                                     "e\xE2\x9C\xAF";
                  else if (dMode == "ws")
                    fullTabString += "\xC2\xA7"
                                     "d" +
                                     std::to_string(stats.winstreak) + " WS";
                  else if (dMode == "team")
                    fullTabString += currentTeam.empty()
                                         ? "\xC2\xA7"
                                           "7None"
                                         : teamColorCode + currentTeam;
                }
              } else {
                fullTabString = teamColorCode + name;
              }

              jstring jf = env->NewStringUTF(fullTabString.c_str());
              jobject component =
                  (jf) ? env->NewObject(cctCls, cctInit, jf) : nullptr;
              if (component && m_setDisp)
                env->CallVoidMethod(info, m_setDisp, component);
              if (jf)
                env->DeleteLocalRef(jf);
              if (component)
                env->DeleteLocalRef(component);

              // === HEALTH SYNC DEBUG ===
              {
                static ULONGLONG lastHealthDbg = 0;
                bool doHealthDbg = Config::isGlobalDebugEnabled() &&
                                   (now - lastHealthDbg > 3000) &&
                                   processedTab == 0;

                if (doHealthDbg) {
                  Logger::log(Config::DebugCategory::GameDetection,
                              "HealthSync check: scoreboard=%p tabObj=%p "
                              "m_getScore=%p m_getVal=%p m_setVal=%p",
                              scoreboard, tabObj, m_getScore, m_getVal,
                              m_setVal);
                }

                if (scoreboard && tabObj && m_getScore && m_getVal &&
                    m_setVal) {
                  jstring oldNameJ = env->NewStringUTF(name.c_str());
                  int scoreVal = 0;
                  bool scoreFound = false;
                  if (oldNameJ) {
                    jobject oldScore = env->CallObjectMethod(
                        scoreboard, m_getScore, oldNameJ, tabObj);
                    if (env->ExceptionCheck())
                      env->ExceptionClear();
                    if (oldScore) {
                      scoreVal = env->CallIntMethod(oldScore, m_getVal);
                      if (scoreVal > 0)
                        scoreFound = true;
                      if (doHealthDbg) {
                        Logger::log(
                            Config::DebugCategory::GameDetection,
                            "HealthSync READ: name=%s scoreVal=%d found=%d",
                            name.c_str(), scoreVal, scoreFound ? 1 : 0);
                      }
                      env->DeleteLocalRef(oldScore);
                    } else {
                      if (doHealthDbg) {
                        Logger::log(
                            Config::DebugCategory::GameDetection,
                            "HealthSync READ: oldScore is NULL for name=%s",
                            name.c_str());
                      }
                    }
                    env->DeleteLocalRef(oldNameJ);
                  }

                  if (scoreFound) {
                    jstring newNameJ = env->NewStringUTF(internalName.c_str());
                    if (newNameJ) {
                      jobject newScore = env->CallObjectMethod(
                          scoreboard, m_getScore, newNameJ, tabObj);
                      if (env->ExceptionCheck())
                        env->ExceptionClear();
                      if (newScore) {
                        env->CallVoidMethod(newScore, m_setVal, scoreVal);
                        if (env->ExceptionCheck()) {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync WRITE: EXCEPTION on "
                                        "setScorePoints!");
                          env->ExceptionClear();
                        } else {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync WRITE: OK val=%d",
                                        scoreVal);
                        }
                        env->DeleteLocalRef(newScore);
                      } else {
                        if (doHealthDbg)
                          Logger::log(Config::DebugCategory::GameDetection,
                                      "HealthSync WRITE: newScore is NULL");
                      }
                      if (g_jCache.m_onScoreUpdated) {
                        env->CallVoidMethod(
                            scoreboard, g_jCache.m_onScoreUpdated, newNameJ);
                        if (env->ExceptionCheck()) {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync BROADCAST: EXCEPTION!");
                          env->ExceptionClear();
                        } else {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync BROADCAST: OK");
                        }
                      } else {
                        if (doHealthDbg)
                          Logger::log(
                              Config::DebugCategory::GameDetection,
                              "HealthSync BROADCAST: m_onScoreUpdated is NULL");
                      }
                      env->DeleteLocalRef(newNameJ);
                    }
                  } else {
                    if (doHealthDbg) {
                      Logger::log(Config::DebugCategory::GameDetection,
                                  "HealthSync: score NOT found for %s (val=%d)",
                                  name.c_str(), scoreVal);
                    }
                  }
                } else if (doHealthDbg) {
                  Logger::log(Config::DebugCategory::GameDetection,
                              "HealthSync SKIP: sb=%d tabObj=%d getScore=%d "
                              "getVal=%d setVal=%d",
                              scoreboard ? 1 : 0, tabObj ? 1 : 0,
                              m_getScore ? 1 : 0, m_getVal ? 1 : 0,
                              m_setVal ? 1 : 0);
                }

                if (doHealthDbg)
                  lastHealthDbg = now;
              }
            } else {
              if (m_setDisp)
                env->CallVoidMethod(info, m_setDisp, nullptr);
            }
          }
        }
      }

      if (lc->CheckException()) {
        env->ExceptionClear();
      }

      env->PopLocalFrame(nullptr);
      processedTab++;
      if (processedTab > 500)
        break; // sanity
    }
    if (scoreboard)
      env->DeleteLocalRef(scoreboard);
    if (tabObj)
      env->DeleteLocalRef(tabObj);

    if (!currentNames.empty() && g_inHypixelGame) {
      if (!g_manualPushedPlayers.empty()) {
        for (const auto &mName : g_manualPushedPlayers) {
          if (std::find(currentNames.begin(), currentNames.end(), mName) ==
              currentNames.end()) {
            currentNames.push_back(mName);
          }
        }
      }

      bool changed = false;
      if (currentNames.size() != g_onlinePlayers.size()) {
        changed = true;
      } else {
        for (size_t i = 0; i < currentNames.size(); ++i) {
          if (currentNames[i] != g_onlinePlayers[i]) {
            changed = true;
            break;
          }
        }
      }
      if (changed) {
        g_onlinePlayers = currentNames;
        g_nextFetchIdx = 0;
      }
    }
  }

  if (iter)
    env->DeleteLocalRef(iter);
  env->DeleteLocalRef(col);
  env->DeleteLocalRef(nh);
  if (world)
    env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

static void detectPreGameLobby() {
  JNIEnv *env = lc->getEnv();
  if (!env || !g_initialized)
    return;

  ULONGLONG now = GetTickCount64();
  if (now - g_preGameDetectTick < 500)
    return;
  g_preGameDetectTick = now;

  if (g_inHypixelGame) {
    if (g_inPreGameLobby) {
      g_inPreGameLobby = false;
      Logger::log(Config::DebugCategory::GameDetection,
                  "Pre-game lobby ended (game started)");
      sendTeamStatsReport();
    }
    return;
  }

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return;
  }

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return;
  }

  g_jCache.init(env);
  jmethodID m_getSB = g_jCache.m_getScoreboard;
  if (!m_getSB) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject scoreboard = env->CallObjectMethod(world, m_getSB);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!scoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jmethodID m_getObj = g_jCache.m_getObjectiveInDisplaySlot;
  if (!m_getObj) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject sidebarObj = env->CallObjectMethod(scoreboard, m_getObj, 1);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!sidebarObj) {
    if (Config::isDebugging()) {
      static ULONGLONG lastSbDbg = 0;
      if (now - lastSbDbg > 3000) {
        ChatSDK::showClientMessage(
            ChatSDK::formatPrefix() + "\xC2\xA7" +
            "7[DEBUG] detectPreGameLobby: Sidebar is NULL");
        lastSbDbg = now;
      }
    }
    if (g_inPreGameLobby) {
      g_inPreGameLobby = false;
      Logger::log(Config::DebugCategory::GameDetection,
                  "Pre-game lobby ended (no sidebar)");
    }
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  static jmethodID s_m_getSorted = nullptr;
  static jmethodID s_m_getPlayerName = nullptr;
  static bool s_resolved = false;
  static std::string s_objClassName;

  if (!s_resolved) {
    jclass sbCls = g_jCache.sbCls;
    jclass scoreCls = g_jCache.scoreCls;

    jclass objRealCls = env->GetObjectClass(sidebarObj);
    if (objRealCls) {
      jclass classCls = env->FindClass("java/lang/Class");
      jmethodID m_getName =
          env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
      if (m_getName) {
        jstring nameJ = (jstring)env->CallObjectMethod(objRealCls, m_getName);
        if (nameJ) {
          const char *nameUtf = env->GetStringUTFChars(nameJ, 0);
          if (nameUtf) {
            s_objClassName = nameUtf;
            for (auto &ch : s_objClassName) {
              if (ch == '.')
                ch = '/';
            }
            env->ReleaseStringUTFChars(nameJ, nameUtf);
          }
          env->DeleteLocalRef(nameJ);
        }
      }
      env->DeleteLocalRef(classCls);
      env->DeleteLocalRef(objRealCls);
    }

    if (sbCls && !s_objClassName.empty()) {
      std::string dynSig = "(L" + s_objClassName + ";)Ljava/util/Collection;";

      const char *knownNames[] = {"getSortedScores", "func_96534_i", nullptr};
      const char *sigs[] = {
          "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;",
          dynSig.c_str(), nullptr};

      for (int s = 0; sigs[s] && !s_m_getSorted; s++) {
        for (int n = 0; knownNames[n] && !s_m_getSorted; n++) {
          s_m_getSorted = env->GetMethodID(sbCls, knownNames[n], sigs[s]);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getSorted = nullptr;
          }
        }
      }

      if (!s_m_getSorted) {
        for (char c = 'a'; c <= 'z' && !s_m_getSorted; c++) {
          char name[2] = {c, 0};
          s_m_getSorted = env->GetMethodID(sbCls, name, dynSig.c_str());
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getSorted = nullptr;
          }
        }
      }
    }

    if (scoreCls) {
      const char *scoreNames[] = {"getPlayerName", "func_96653_e", nullptr};
      for (int n = 0; scoreNames[n] && !s_m_getPlayerName; n++) {
        s_m_getPlayerName =
            env->GetMethodID(scoreCls, scoreNames[n], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getPlayerName = nullptr;
        }
      }

      if (!s_m_getPlayerName) {
        for (char c = 'a'; c <= 'z' && !s_m_getPlayerName; c++) {
          char name[2] = {c, 0};
          s_m_getPlayerName =
              env->GetMethodID(scoreCls, name, "()Ljava/lang/String;");
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getPlayerName = nullptr;
          }
        }
      }
    }

    s_resolved = true;
  }

  static jmethodID s_m_getPlayersTeam = nullptr;
  static jmethodID s_m_getPrefix = nullptr;
  static jmethodID s_m_getSuffix = nullptr;
  static bool s_teamRes = false;

  if (!s_teamRes) {
    jclass sbCls = g_jCache.sbCls;
    if (sbCls) {
      const char *names[] = {"getPlayersTeam", "func_96509_i", "h", "i",
                             nullptr};
      const char *sigs[] = {
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;",
          "(Ljava/lang/String;)Laul;", "(Ljava/lang/String;)Lauq;", nullptr};
      for (int s = 0; sigs[s] && !s_m_getPlayersTeam; s++) {
        for (int n = 0; names[n] && !s_m_getPlayersTeam; n++) {
          s_m_getPlayersTeam = env->GetMethodID(sbCls, names[n], sigs[s]);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getPlayersTeam = nullptr;
          } else {
            Logger::info("Resolved getPlayersTeam with %s %s", names[n],
                         sigs[s]);
          }
        }
      }
    }

    jclass ptCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
    if (!ptCls) {
      ptCls = env->FindClass("aul");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    if (!ptCls) {
      ptCls = env->FindClass("auq");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (ptCls) {
      const char *prefNames[] = {"getColorPrefix", "func_96668_e", "e",
                                 nullptr};
      const char *sufNames[] = {"getColorSuffix", "func_96663_f", "f", nullptr};

      for (int i = 0; prefNames[i] && !s_m_getPrefix; i++) {
        s_m_getPrefix =
            env->GetMethodID(ptCls, prefNames[i], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getPrefix = nullptr;
        }
      }

      for (int i = 0; sufNames[i] && !s_m_getSuffix; i++) {
        s_m_getSuffix =
            env->GetMethodID(ptCls, sufNames[i], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getSuffix = nullptr;
        }
      }
    }
    s_teamRes = true;
  }

  bool wasPreGame = g_inPreGameLobby;
  bool foundMap = false, foundPlayers = false, foundMode = false;

  if (s_m_getSorted && s_m_getPlayerName) {
    jobject scoresCollection =
        env->CallObjectMethod(scoreboard, s_m_getSorted, sidebarObj);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      scoresCollection = nullptr;
    }

    if (scoresCollection) {
      jclass collCls = env->FindClass("java/util/Collection");
      jmethodID m_iterator =
          env->GetMethodID(collCls, "iterator", "()Ljava/util/Iterator;");
      jclass iterCls = env->FindClass("java/util/Iterator");
      jmethodID m_hasNext = env->GetMethodID(iterCls, "hasNext", "()Z");
      jmethodID m_next =
          env->GetMethodID(iterCls, "next", "()Ljava/lang/Object;");

      if (m_iterator && m_hasNext && m_next) {
        jobject iter = env->CallObjectMethod(scoresCollection, m_iterator);
        int lineCount = 0;
        while (iter && env->CallBooleanMethod(iter, m_hasNext) &&
               lineCount < 20) {
          jobject scoreObj = env->CallObjectMethod(iter, m_next);
          if (scoreObj) {
            jstring pnJ =
                (jstring)env->CallObjectMethod(scoreObj, s_m_getPlayerName);
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              pnJ = nullptr;
            }

            if (pnJ) {
              std::string rawLine = "";
              std::string prefRaw = "", nameRaw = "", suffRaw = "";

              const char *pnUtf = env->GetStringUTFChars(pnJ, 0);
              if (pnUtf) {
                nameRaw = pnUtf;
                env->ReleaseStringUTFChars(pnJ, pnUtf);
              }

              if (s_m_getPlayersTeam && s_m_getPrefix && s_m_getSuffix) {
                jobject teamObj =
                    env->CallObjectMethod(scoreboard, s_m_getPlayersTeam, pnJ);
                if (env->ExceptionCheck())
                  env->ExceptionClear();

                if (teamObj) {
                  jstring prefJ =
                      (jstring)env->CallObjectMethod(teamObj, s_m_getPrefix);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  if (prefJ) {
                    const char *prefUtf = env->GetStringUTFChars(prefJ, 0);
                    if (prefUtf) {
                      prefRaw = prefUtf;
                      env->ReleaseStringUTFChars(prefJ, prefUtf);
                    }
                    env->DeleteLocalRef(prefJ);
                  }

                  jstring suffJ =
                      (jstring)env->CallObjectMethod(teamObj, s_m_getSuffix);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  if (suffJ) {
                    const char *suffUtf = env->GetStringUTFChars(suffJ, 0);
                    if (suffUtf) {
                      suffRaw = suffUtf;
                      env->ReleaseStringUTFChars(suffJ, suffUtf);
                    }
                    env->DeleteLocalRef(suffJ);
                  }
                  env->DeleteLocalRef(teamObj);
                }
              }

              rawLine = prefRaw + nameRaw + suffRaw;

              std::string clean;
              for (size_t i = 0; i < rawLine.length(); ++i) {
                unsigned char c = (unsigned char)rawLine[i];
                if (c == 0xC2 && i + 1 < rawLine.length() &&
                    (unsigned char)rawLine[i + 1] == 0xA7) {
                  i += 2;
                  continue;
                }
                if (c == 0xA7) {
                  i += 1;
                  continue;
                }
                clean += (char)c;
              }

              if (clean.find("Map:") != std::string::npos)
                foundMap = true;
              if (clean.find("Players:") != std::string::npos)
                foundPlayers = true;
              if (clean.find("Mode:") != std::string::npos)
                foundMode = true;

              if (clean.length() >= 8 && isdigit((unsigned char)clean[0]) &&
                  isdigit((unsigned char)clean[1]) && clean[2] == '/' &&
                  isdigit((unsigned char)clean[3]) &&
                  isdigit((unsigned char)clean[4]) && clean[5] == '/') {
                static std::string lastServerLine = "";
                if (clean != lastServerLine) {
                  lastServerLine = clean;
                  g_chatPrintedPlayers.clear();
                }
              }

              env->DeleteLocalRef(pnJ);
            }
            env->DeleteLocalRef(scoreObj);
          }
          lineCount++;
        }
        if (iter)
          env->DeleteLocalRef(iter);
      }
      if (collCls)
        env->DeleteLocalRef(collCls);
      if (iterCls)
        env->DeleteLocalRef(iterCls);
      env->DeleteLocalRef(scoresCollection);
    }
  }

  bool isPreGame = (foundMap && foundPlayers && foundMode);

  if (Config::isDebugging()) {
    static ULONGLONG lastPreDbg = 0;
    if (now - lastPreDbg > 5000) {
      ChatSDK::showClientMessage(
          ChatSDK::formatPrefix() + "\xC2\xA7" +
          "e[DEBUG] PreGame Detection Results: Map=" +
          (foundMap ? "Yes" : "No") +
          " Players=" + (foundPlayers ? "Yes" : "No") +
          " Mode=" + (foundMode ? "Yes" : "No") +
          " Result=" + (isPreGame ? "PreGame" : "NotPreGame"));
      lastPreDbg = now;
    }
  }

  if (isPreGame && !wasPreGame) {
    g_inPreGameLobby = true;
    ChatInterceptor::clearAllCaches();
    Logger::log(Config::DebugCategory::GameDetection,
                "Pre-game lobby DETECTED (sidebar has Map/Players/Mode)");
    Render::NotificationManager::getInstance()->add(
        "System", "Pre-Game Lobby Detected", Render::NotificationType::Success);
  } else if (!isPreGame && wasPreGame) {
    g_inPreGameLobby = false;
    ChatInterceptor::clearAllCaches();
    Logger::log(Config::DebugCategory::GameDetection, "Pre-game lobby ended");
  }

  g_inPreGameLobby = isPreGame;

  env->DeleteLocalRef(sidebarObj);
  env->DeleteLocalRef(scoreboard);
  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

static std::string getUserProfileDir() {
  char *up = nullptr;
  size_t sz = 0;
  std::string out;
  if (_dupenv_s(&up, &sz, "USERPROFILE") == 0 && up)
    out = up;
  if (up)
    free(up);
  return out;
}

static std::string getAppDataDir() {
  char *ad = nullptr;
  size_t sz = 0;
  std::string out;
  if (_dupenv_s(&ad, &sz, "APPDATA") == 0 && ad)
    out = ad;
  if (ad)
    free(ad);
  return out;
}

static std::vector<std::string> getLogDirectoryCandidates() {
  std::vector<std::string> candidates;

  // lunar
  std::string up = getUserProfileDir();
  if (!up.empty()) {
    std::string lunar = up + "\\.lunarclient\\profiles\\lunar\\1.8\\logs";
    DWORD attr = GetFileAttributesA(lunar.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(lunar);
  }

  // badlion/vanilla
  std::string ad = getAppDataDir();
  if (!ad.empty()) {
    std::string blmc = ad + "\\.minecraft\\logs\\blclient\\minecraft";
    DWORD attrBl = GetFileAttributesA(blmc.c_str());
    if (attrBl != INVALID_FILE_ATTRIBUTES &&
        (attrBl & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(blmc);

    // standart
    std::string mc = ad + "\\.minecraft\\logs";
    DWORD attr = GetFileAttributesA(mc.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(mc);
  }

  return candidates;
}

static std::string findNewestLogFile(const std::string &dir) {
  WIN32_FIND_DATAA fd{};
  std::string pattern = dir + "\\*.log";
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE)
    return std::string();
  FILETIME best{};
  std::string bestName;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) {
        best = fd.ftLastWriteTime;
        bestName = fd.cFileName;
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  if (bestName.empty())
    return std::string();
  return dir + "\\" + bestName;
}

static bool ensureLogOpen() {
  std::vector<std::string> candidates = getLogDirectoryCandidates();
  if (candidates.empty())
    return false;

  std::string absoluteBestFile;
  FILETIME absoluteBestTime = {0, 0};

  for (const auto &dir : candidates) {
    std::string newestInDir = findNewestLogFile(dir);
    if (newestInDir.empty())
      continue;

    HANDLE hFile =
        CreateFileA(newestInDir.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      FILETIME ftWrite;
      if (GetFileTime(hFile, nullptr, nullptr, &ftWrite)) {
        if (CompareFileTime(&ftWrite, &absoluteBestTime) > 0) {
          absoluteBestTime = ftWrite;
          absoluteBestFile = newestInDir;
        }
      }
      CloseHandle(hFile);
    }
  }

  if (absoluteBestFile.empty())
    return false;

  if (g_logFilePath != absoluteBestFile) {
    if (g_logHandle != INVALID_HANDLE_VALUE) {
      CloseHandle(g_logHandle);
      g_logHandle = INVALID_HANDLE_VALUE;
    }
    g_logFilePath = absoluteBestFile;
    g_logOffset = 0;
    g_logBuf.clear();
    Logger::info("Newest log detected across all clients: %s",
                 absoluteBestFile.c_str());
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::General, "Switched to Log File: %s",
                  absoluteBestFile.c_str());
    }
  }

  if (g_logHandle == INVALID_HANDLE_VALUE) {
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::General, "Opening Log: %s",
                  absoluteBestFile.c_str());
    }
    g_logHandle =
        CreateFileA(g_logFilePath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_logHandle == INVALID_HANDLE_VALUE)
      return false;
    LARGE_INTEGER sz{};
    if (GetFileSizeEx(g_logHandle, &sz))
      g_logOffset = (long long)sz.QuadPart; // tail from end
  }
  return true;
}

static void parsePlayersFromOnlineLine(const std::string &joined) {
  if (!g_inHypixelGame && !g_inPreGameLobby) {
    g_onlinePlayers.clear();
    return;
  }
  std::string listStr = joined.substr(joined.find("ONLINE:") + 7);
  while (!listStr.empty() && listStr.front() == ' ')
    listStr.erase(listStr.begin());
  while (!listStr.empty() && listStr.back() == ' ')
    listStr.pop_back();
  std::vector<std::string> names;
  size_t start = 0;
  for (;;) {
    size_t comma = listStr.find(',', start);
    std::string token =
        listStr.substr(start, comma == std::string::npos ? std::string::npos
                                                         : (comma - start));
    while (!token.empty() && token.front() == ' ')
      token.erase(token.begin());
    while (!token.empty() && token.back() == ' ')
      token.pop_back();
    if (!token.empty())
      names.push_back(token);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  if (!names.empty()) {
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::string> prev = g_onlinePlayers;
    std::sort(prev.begin(), prev.end());
    if (sorted == prev)
      return;

    {
      std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
      g_onlinePlayers = names;
    }
  }
}

static void resetGameCache() {
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    ChatInterceptor::g_playerStatsMap.clear();
    ChatInterceptor::g_playerTeamColor.clear();
  }
  {
    std::lock_guard<std::mutex> lockR(g_stableRankMutex);
    g_stableRankMap.clear();
  }
  g_processedPlayers.clear();
  g_onlinePlayers.clear();
  g_teamReportSent = false;
  g_playerTeamColor.clear();
  g_playerFetchRetries.clear();
  {
    std::lock_guard<std::mutex> lock(g_alertedMutex);
    g_alertedPlayers.clear();
  }
  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_queuedPlayers.clear();
  }
  {
    std::lock_guard<std::mutex> aLock(g_activeFetchesMutex);
    g_activeFetches.clear();
  }
  {
    std::lock_guard<std::mutex> lockE(g_eliminatedMutex);
    g_eliminatedPlayers.clear();
  }
  g_lastResetTick = GetTickCount64();
  Logger::log(Config::DebugCategory::GameDetection,
              "Game cache reset performed");
}

void ChatInterceptor::clearAllCaches() {
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    ChatInterceptor::g_playerStatsMap.clear();
  }

  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_persistentStatsCache.clear();
  }

  {
    std::lock_guard<std::mutex> lock(g_uuidMapMutex);
    g_playerUuidMap.clear();
  }

  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_onlinePlayers.clear();
    g_processedPlayers.clear();
    g_queuedPlayers.clear();
  }

  {
    std::lock_guard<std::mutex> aLock(g_activeFetchesMutex);
    g_activeFetches.clear();
  }

  {
    std::lock_guard<std::mutex> pLock(g_pendingStatsMutex);
    g_pendingStatsMap.clear();
  }

  {
    std::lock_guard<std::mutex> rLock(g_retryMutex);
    g_retryUntil.clear();
    g_playerFetchRetries.clear();
  }

  {
    std::lock_guard<std::mutex> lock(g_alertedMutex);
    g_alertedPlayers.clear();
  }

  g_playerTeamColor.clear();
  g_manualPushedPlayers.clear();
  g_forceChatOutputPlayers.clear();
  g_chatPrintedPlayers.clear();
  g_stableRankMap.clear();

  {
    std::lock_guard<std::mutex> lockE(g_eliminatedMutex);
    g_eliminatedPlayers.clear();
  }

  Urchin::clearCache();
  Seraph::clearCache();

  Logger::log(Config::DebugCategory::General,
              "All player caches cleared via ChatInterceptor::clearAllCaches.");
}

static void cleanupStaleStats() {
  std::vector<std::string> toPrune;
  std::vector<std::string> toResetNicked;

  {
    std::lock_guard<std::mutex> statsLock(ChatInterceptor::g_statsMutex);
    for (auto it = ChatInterceptor::g_playerStatsMap.begin();
         it != ChatInterceptor::g_playerStatsMap.end(); ++it) {
      bool found = false;
      for (const auto &p : g_onlinePlayers) {
        if (p == it->first) {
          found = true;
          break;
        }
      }
      bool isNicked = it->second.isNicked;

      if (g_inHypixelGame) {
        if (isNicked && !found) {
          toPrune.push_back(it->first);
        }
      } else {
        if (!found)
          toPrune.push_back(it->first);
      }
    }

    for (const auto &name : toPrune) {
      ChatInterceptor::g_playerStatsMap.erase(name);
    }
  }

  if (!toResetNicked.empty()) {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    std::lock_guard<std::mutex> clock(g_cacheMutex);
    for (const auto &name : toResetNicked) {
      g_processedPlayers.erase(name);
      g_persistentStatsCache.erase(name);
      g_playerFetchRetries[name] = 0;
      g_queuedPlayers.erase(name);
    }
  }

  if (!toPrune.empty()) {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    for (const auto &name : toPrune) {
      g_processedPlayers.erase(name);
      g_playerFetchRetries.erase(name);
      g_queuedPlayers.erase(name);
    }
  }

  Logger::log(Config::DebugCategory::General, "Stale stats cleanup performed");
}

static void syncTeamColors() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  g_jCache.init(env);

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
  if (!mcObj)
    return;

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

  if (world) {
    jmethodID m_getScoreboard = g_jCache.m_getScoreboard;
    jobject scoreboard = m_getScoreboard
                             ? env->CallObjectMethod(world, m_getScoreboard)
                             : nullptr;
    env->ExceptionClear();
    if (scoreboard) {
      if (g_jCache.m_getPlayersTeam && g_jCache.m_getPrefix) {
        std::vector<std::string> namesToSync;
        {
          std::lock_guard<std::mutex> stLock(ChatInterceptor::g_statsMutex);
          for (const auto &pair : ChatInterceptor::g_playerStatsMap) {
            namesToSync.push_back(pair.first);
          }
        }

        std::unordered_map<std::string, std::string> resolvedTeams;
        for (const auto &name : namesToSync) {
          std::string team = resolveTeamForNameEx(
              env, name, scoreboard, g_jCache.m_getPlayersTeam,
              g_jCache.teamCls, g_jCache.m_getPrefix);
          if (!team.empty()) {
            resolvedTeams[name] = team;
          }
        }

        {
          std::lock_guard<std::mutex> stLock2(ChatInterceptor::g_statsMutex);
          for (const auto &name : namesToSync) {
            auto it = resolvedTeams.find(name);
            std::string team;
            if (it != resolvedTeams.end()) {
              team = it->second;
            } else {
              auto itT = g_playerTeamColor.find(name);
              if (itT != g_playerTeamColor.end())
                team = itT->second;
            }

            if (!team.empty()) {
              auto statIt = ChatInterceptor::g_playerStatsMap.find(name);
              if (statIt != ChatInterceptor::g_playerStatsMap.end()) {
                statIt->second.teamColor = team;
              }
              g_playerTeamColor[name] = team;
            }
          }
        }
      }
      env->DeleteLocalRef(scoreboard);
    }
    env->DeleteLocalRef(world);
  }
  env->DeleteLocalRef(mcObj);
}

#pragma warning(push)
#pragma warning(disable : 26110 26117)
static void syncTags() {
  if (!Config::isTagsEnabled())
    return;

  std::string activeS = Config::getActiveTagService();
  auto getAbbr = [](const std::string &raw) -> std::string {
    std::string t = raw;
    for (auto &c : t)
      c = toupper(c);
    if (t.find("BLATANT") != std::string::npos)
      return "\xC2\xA7"
             "4[BC]";
    if (t.find("CLOSET") != std::string::npos)
      return "\xC2\xA7"
             "4[CC]";
    if (t.find("CHEATER") != std::string::npos)
      return "\xC2\xA7"
             "5[C]";
    if (t.find("CONFIRMED") != std::string::npos)
      return "\xC2\xA7"
             "5[C]";
    if (t.find("CAUTION") != std::string::npos)
      return "\xC2\xA7"
             "e[!]";
    if (t.find("SUSPICIOUS") != std::string::npos)
      return "\xC2\xA7"
             "6[?]";
    if (t.find("SNIPER") != std::string::npos)
      return "\xC2\xA7"
             "6[S]";
    return "";
  };

  std::vector<std::pair<std::string, std::string>> playersNeedingTags;
  {
    std::lock_guard<std::mutex> tagLock(ChatInterceptor::g_statsMutex);
    for (auto &pair : ChatInterceptor::g_playerStatsMap) {
      bool needsUrchin = (activeS == "Urchin" || activeS == "Both");
      bool needsSeraph = (activeS == "Seraph" || activeS == "Both");

      for (const auto &rt : pair.second.rawTags) {
        if (rt.find("URCHIN") == 0)
          needsUrchin = false;
        if (rt.find("SERAPH") == 0)
          needsSeraph = false;
      }

      if (needsUrchin || needsSeraph) {
        playersNeedingTags.push_back({pair.first, pair.second.uuid});
      }
    }
  }

  if (playersNeedingTags.empty())
    return;

  std::vector<std::tuple<std::string, std::string, std::vector<std::string>>>
      updates;
  for (const auto &p : playersNeedingTags) {
    std::string tagStr;
    std::vector<std::string> rTags;
    bool foundAny = false;

    bool needsUrchin = (activeS == "Urchin" || activeS == "Both");
    bool needsSeraph = (activeS == "Seraph" || activeS == "Both");
    {
      std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
      auto itS = ChatInterceptor::g_playerStatsMap.find(p.first);
      if (itS != ChatInterceptor::g_playerStatsMap.end()) {
        for (const auto &rt : itS->second.rawTags) {
          if (rt.find("URCHIN") == 0)
            needsUrchin = false;
          if (rt.find("SERAPH") == 0)
            needsSeraph = false;
        }
      }
    }

    if (activeS == "Urchin" || activeS == "Both") {
      if (needsUrchin) {
        auto uT = Urchin::getPlayerTags(p.first);
        rTags.push_back("URCHIN_CHECKED");
        foundAny = true;
        if (uT && !uT->tags.empty()) {
          std::string a = getAbbr(uT->tags[0].type);
          tagStr += " " + (a.empty() ? "\xC2\xA7"
                                       "4[U]"
                                     : a);
          for (const auto &t : uT->tags)
            rTags.push_back("URCHIN:" + t.type);
        }
      }
    }
    if ((activeS == "Seraph" || activeS == "Both") && !p.second.empty()) {
      if (needsSeraph) {
        auto sT = Seraph::getPlayerTags(p.first, p.second);
        rTags.push_back("SERAPH_CHECKED");
        foundAny = true;
        if (sT && !sT->tags.empty()) {
          std::string a = getAbbr(sT->tags[0].type);
          tagStr += " " + (a.empty() ? "\xC2\xA7"
                                       "4[S]"
                                     : a);
          for (const auto &t : sT->tags)
            rTags.push_back("SERAPH:" + t.type);
        }
      }
    }

    if (foundAny) {
      updates.push_back({p.first, tagStr, rTags});
    }
  }

  if (!updates.empty()) {
    std::lock_guard<std::mutex> tagLock2(ChatInterceptor::g_statsMutex);
    for (const auto &u : updates) {
      auto it = ChatInterceptor::g_playerStatsMap.find(std::get<0>(u));
      if (it != ChatInterceptor::g_playerStatsMap.end()) {
        for (const auto &newTag : std::get<2>(u)) {
          bool exists = false;
          for (const auto &oldTag : it->second.rawTags) {
            if (oldTag == newTag) {
              exists = true;
              break;
            }
          }
          if (!exists)
            it->second.rawTags.push_back(newTag);
        }

        std::string newDisplay = "";
        std::set<std::string> seenTags;

        for (const auto &rt : it->second.rawTags) {
          if (seenTags.find(rt) != seenTags.end())
            continue;
          seenTags.insert(rt);

          if (rt.find("URCHIN:") == 0) {
            std::string type = rt.substr(7);
            std::string abbr = getAbbr(type);
            newDisplay += " " + (abbr.empty() ? ("\xC2\xA7"
                                                 "7[" +
                                                 type + "]")
                                              : abbr);
          } else if (rt.find("SERAPH:") == 0) {
            std::string type = rt.substr(7);
            std::string abbr = getAbbr(type);
            newDisplay += " " + (abbr.empty() ? ("\xC2\xA7"
                                                 "7[" +
                                                 type + "]")
                                              : abbr);
          }
        }
        it->second.tagsDisplay = newDisplay;
      }
    }
  }
}
#pragma warning(pop)

static void fetchWorker(std::string name, std::string forcedUuid);

static void tailLogOnce() {
  if (!ensureLogOpen())
    return;
  LARGE_INTEGER pos{};
  pos.QuadPart = g_logOffset;
  SetFilePointerEx(g_logHandle, pos, nullptr, FILE_BEGIN);
  char buf[4096]; // this shit crashed the whole thing
  DWORD read = 0;
  if (!ReadFile(g_logHandle, buf, sizeof(buf), &read, nullptr) || read == 0)
    return;
  g_logOffset += read;
  g_logBuf.append(buf, buf + read);

  size_t nl;
  while ((nl = g_logBuf.find('\n')) != std::string::npos) {
    std::string line = g_logBuf.substr(0, nl);
    g_logBuf.erase(0, nl + 1);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.find("[CHAT]") == std::string::npos)
      continue;
    size_t p = line.find("[CHAT]");
    std::string chat = (p != std::string::npos) ? line.substr(p + 6) : line;

    if (Config::isPreGameChatStatsEnabled()) {
      if (g_inPreGameLobby) {
        std::string cleanChat;
        for (size_t i = 0; i < chat.length(); ++i) {
          unsigned char c = (unsigned char)chat[i];
          if (c == 0xC2 && i + 1 < chat.length() &&
              (unsigned char)chat[i + 1] == 0xA7) {
            i += 2;
            continue;
          }
          if (c == 0xA7) {
            i += 1;
            continue;
          }
          cleanChat += (char)c;
        }

        ULONGLONG nowDbg = GetTickCount64();

        if (Config::isDebugging()) {
          static ULONGLONG lastParseCheck = 0;
          if (nowDbg - lastParseCheck >
              100) { // log frequently but with caution
            bool isOVson = (cleanChat.find("[OVson]") != std::string::npos);
            bool isTo = (cleanChat.find("To ") == 0);
            bool isFrom = (cleanChat.find("From ") == 0);
            if (isOVson || isTo || isFrom) {
              ChatSDK::showClientMessage(
                  ChatSDK::formatPrefix() + "\xC2\xA7" +
                  "7[DEBUG] Skipped Line (is DM/Internal): " +
                  cleanChat.substr(0, (std::min)((int)cleanChat.size(), 20)));
            }
          }
        }
        // another ignore check
        if (cleanChat.find("[OVson]") == std::string::npos &&
            cleanChat.find("To ") != 0 && cleanChat.find("From ") != 0) {
          size_t firstColon = cleanChat.find(": ");
          if (firstColon != std::string::npos && firstColon > 0) {
            std::string prefix = cleanChat.substr(0, firstColon);

            size_t pStart = prefix.find_first_not_of(' ');
            size_t pEnd = prefix.find_last_not_of(' ');
            if (pStart == std::string::npos)
              continue;
            prefix = prefix.substr(pStart, pEnd - pStart + 1);

            std::string username;
            size_t firstBracket = prefix.find('[');

            if (firstBracket != std::string::npos) {
              if (firstBracket > 0) {
                continue;
              }

              size_t lastBracket = prefix.find_last_of(']');
              if (lastBracket == std::string::npos)
                continue;

              username = prefix.substr(lastBracket + 1);
              size_t uStart = username.find_first_not_of(' ');
              if (uStart != std::string::npos) {
                username = username.substr(uStart);
              }

              if (username.find(' ') != std::string::npos || username.empty()) {
                continue;
              }
            } else {
              if (prefix.find(' ') != std::string::npos) {
                continue;
              }
              username = prefix;
            }

            bool valid = (username.length() >= 3 && username.length() <= 16);
            for (char c : username) {
              if (!isalnum((unsigned char)c) && c != '_') {
                valid = false;
                break;
              }
            }

            static ULONGLONG lastParseDbg = 0;
            if (Config::isDebugging() && (nowDbg - lastParseDbg > 1000)) {
              ChatSDK::showClientMessage(
                  ChatSDK::formatPrefix() + "\xC2\xA7" +
                  "e[DEBUG] PreGame Chat Parsed. User: " + username +
                  " Valid: " + (valid ? "Yes" : "No"));
              lastParseDbg = nowDbg;
            }

            if (valid) {
              if (g_chatPrintedPlayers.find(username) ==
                  g_chatPrintedPlayers.end()) {
                g_chatPrintedPlayers.insert(username);

                if (std::find(g_manualPushedPlayers.begin(),
                              g_manualPushedPlayers.end(),
                              username) == g_manualPushedPlayers.end()) {
                  g_manualPushedPlayers.push_back(username);
                }
                if (std::find(g_onlinePlayers.begin(), g_onlinePlayers.end(),
                              username) == g_onlinePlayers.end()) {
                  g_onlinePlayers.push_back(username);
                }

                g_forceChatOutputPlayers.insert(username);

                {
                  std::lock_guard<std::mutex> lockA(g_activeFetchesMutex);
                  if (g_activeFetches.find(username) == g_activeFetches.end()) {
                    g_activeFetches.insert(username);
                    std::thread(fetchWorker, username, "").detach();
                  }
                }
              } else {
                static ULONGLONG lastAlreadyDbg = 0;
                if (Config::isDebugging() && (nowDbg - lastAlreadyDbg > 2000)) {
                  ChatSDK::showClientMessage(
                      ChatSDK::formatPrefix() + "\xC2\xA7" +
                      "7[DEBUG] Chat Processed Already: " + username);
                  lastAlreadyDbg = nowDbg;
                }
              }
            } else if (Config::isDebugging()) {
              static ULONGLONG lastInvalidDbg = 0;
              if (nowDbg - lastInvalidDbg > 3000) {
                ChatSDK::showClientMessage(
                    ChatSDK::formatPrefix() + "\xC2\xA7" +
                    "c[DEBUG] Invalid Username Parsed: " + username);
                lastInvalidDbg = nowDbg;
              }
            }
          }
        }
      } else if (Config::isDebugging()) {
        static ULONGLONG lastLobbyWarn = 0;
        if (GetTickCount64() - lastLobbyWarn > 10000) {
          ChatSDK::showClientMessage(
              ChatSDK::formatPrefix() + "\xC2\xA7" +
              "7[DEBUG] Chat skipped: g_inPreGameLobby is FALSE");
          lastLobbyWarn = GetTickCount64();
        }
      }
    }

    detectTeamsFromLine(chat);
    detectFinalKillsFromLine(chat);
    detectBedDestructionFromLine(chat);
    Logic::AutoGG::handleChat(chat);

    if (chat.find("ONLINE:") != std::string::npos) {
      if (line != g_lastOnlineLine) {
        g_lastOnlineLine = line;
        Logger::log(Config::DebugCategory::GameDetection,
                    "Detected ONLINE list, parsing players...");
        parsePlayersFromOnlineLine(chat);
        g_nextFetchIdx = 0;
        g_processedPlayers.clear();
      }
    }
  }
}

static void fetchWorker(std::string name, std::string forcedUuid = "") {
  ThreadTracker::increment();
  auto _cleanup = []() { ThreadTracker::decrement(); };
  struct CleanupGuard {
    std::function<void()> f;
    ~CleanupGuard() {
      if (f)
        f();
    }
  } guard{_cleanup};

  if (!g_initialized)
    return;
  if (name.empty() || name.length() > 16)
    return;
  for (char c : name) {
    if (!isalnum(c) && c != '_')
      return;
  }

  const std::string apiKey = Config::getApiKey();
  const bool keyless = Config::isKeylessModeEnabled();

  if (apiKey.empty() && !keyless)
    return;

  bool cacheFound = false;
  Hypixel::PlayerStats cachedData;
  ULONGLONG now = GetTickCount64();

  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_persistentStatsCache.find(name);
    if (it != g_persistentStatsCache.end()) {
      if (now - it->second.timestamp < 600000) {
        cachedData = it->second.stats;
        cacheFound = true;
      }
    }
  }

  if (cacheFound) {
    std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
    g_pendingStatsMap[name] = cachedData;
  }

  bool fetchError = false;
  std::string uuidToF;
  Hypixel::PlayerStats fetchedStats;

  double startTime = TimeUtil::getTime();

  if (!cacheFound) {
    double apiStart = TimeUtil::getTime();
    std::optional<std::string> uuid =
        forcedUuid.empty() ? Hypixel::getUuidByName(name)
                           : std::optional<std::string>(forcedUuid);
    if (uuid) {
      uuidToF = *uuid;
      std::optional<Hypixel::PlayerStats> statsOpt;

      if (keyless) {
        static std::atomic<int> abyssFailCount{0};
        static ULONGLONG lastAbyssSuggest = 0;

        statsOpt = AbyssService::getPlayerStats(*uuid);

        if (!statsOpt) {
          int fails = ++abyssFailCount;
          if (fails >= 5 &&
              (GetTickCount64() - lastAbyssSuggest > 300000)) { // every 5 mins
            ChatSDK::showPrefixed(
                "§cAbyss API is failing. §eSuggestion: Use a personal Hypixel "
                "API key (.api new <key>) for better reliability.");
            lastAbyssSuggest = GetTickCount64();
            abyssFailCount = 0;
          }
        }
      } else {
        statsOpt = Hypixel::getPlayerStats(apiKey, *uuid);
      }

      double apiEnd = TimeUtil::getTime();
      float lastApiLat = (float)(apiEnd - apiStart) * 1000.0f;
      if (ChatInterceptor::g_apiLatency == 0.0f)
        ChatInterceptor::g_apiLatency = lastApiLat;
      else
        ChatInterceptor::g_apiLatency =
            ChatInterceptor::g_apiLatency * 0.9f + lastApiLat * 0.1f;

      if (statsOpt) {
        fetchedStats = *statsOpt;

        {
          std::lock_guard<std::mutex> lock(g_cacheMutex);
          if (g_persistentStatsCache.size() < MAX_STATS_CACHE_SIZE) {
            g_persistentStatsCache[name] =
                CachedStats(fetchedStats, GetTickCount64());
          }
        }

        std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
        g_pendingStatsMap[name] = fetchedStats;
      } else
        fetchError = true;
    } else
      fetchError = true;
  } else {
  }

  bool shouldFetchTags = false;
  if (Config::isTagsEnabled()) {
    if (cacheFound &&
        (cachedData.tagsDisplay.empty() && cachedData.rawTags.empty()))
      shouldFetchTags = true;
    if (!cacheFound && !fetchError)
      shouldFetchTags = true;
  }

  if (shouldFetchTags) {
    Hypixel::PlayerStats &targetStats = cacheFound ? cachedData : fetchedStats;
    std::string currentUuid = cacheFound ? targetStats.uuid : uuidToF;

    if (cacheFound && currentUuid.empty()) {
      auto u = Hypixel::getUuidByName(name);
      if (u)
        currentUuid = *u;
    }

    std::string tagStr;
    std::vector<std::string> rTags;

    std::string activeS = Config::getActiveTagService();
    auto getAbbr = [](const std::string &raw) -> std::string {
      std::string t = raw;
      for (auto &c : t)
        c = toupper(c);
      if (t.find("BLATANT") != std::string::npos)
        return "\xC2\xA7"
               "4[BC]";
      if (t.find("CLOSET") != std::string::npos)
        return "\xC2\xA7"
               "4[CC]";
      if (t.find("CONFIRMED") != std::string::npos)
        return "\xC2\xA7"
               "4[C]";
      if (t.find("CHEATER") != std::string::npos)
        return "\xC2\xA7"
               "4[C]";
      if (t.find("SNIPER") != std::string::npos)
        return "\xC2\xA7"
               "6[S]";
      return "";
    };

    if (activeS == "Urchin" || activeS == "Both") {
      auto uT = Urchin::getPlayerTags(name, true);
      rTags.push_back("URCHIN_CHECKED");
      if (uT && !uT->tags.empty()) {
        std::string a = getAbbr(uT->tags[0].type);
        tagStr += " " + (a.empty() ? "\xC2\xA7"
                                     "4[U]"
                                   : a);
        for (const auto &t : uT->tags)
          rTags.push_back("URCHIN:" + t.type);
      }
    }
    if ((activeS == "Seraph" || activeS == "Both") && !currentUuid.empty()) {
      auto sT = Seraph::getPlayerTags(name, currentUuid, true);
      rTags.push_back("SERAPH_CHECKED");
      if (sT && !sT->tags.empty()) {
        std::string a = getAbbr(sT->tags[0].type);
        tagStr += " " + (a.empty() ? "\xC2\xA7"
                                     "4[S]"
                                   : a);
        for (const auto &t : sT->tags)
          rTags.push_back("SERAPH:" + t.type);
      }
    }

    targetStats.tagsDisplay = tagStr;
    targetStats.rawTags = rTags;
    if (cacheFound) {
      std::lock_guard<std::mutex> lock(g_cacheMutex);
      g_persistentStatsCache[name] = {cachedData, now};
    }
  }

  if (cacheFound) {
    std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
    g_pendingStatsMap[name] = cachedData;
    std::lock_guard<std::mutex> lockQ(g_queueMutex);
    g_processedPlayers.insert(name);
  } else if (!fetchError) {
    {
      std::lock_guard<std::mutex> lock(g_cacheMutex);
      g_persistentStatsCache[name] = {fetchedStats, now};
    }
    std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
    g_pendingStatsMap[name] = fetchedStats;

    std::lock_guard<std::mutex> lockQ(g_queueMutex);
    g_processedPlayers.insert(name);
  }

  if (fetchError && !cacheFound) {
    std::lock_guard<std::mutex> lock(g_retryMutex);
    int count = ++g_playerFetchRetries[name];
    if (count < 5) {
      g_retryUntil[name] = now + 2000;
    } else {
      Hypixel::PlayerStats nickedStats;
      nickedStats.isNicked = true;
      {
        std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
        g_pendingStatsMap[name] = nickedStats;
      }
      // cache
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_persistentStatsCache[name] = {nickedStats, now};
      }
      std::lock_guard<std::mutex> lockQ(g_queueMutex);
      g_processedPlayers.insert(name);
      fetchError = false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_activeFetchesMutex);
    g_activeFetches.erase(name);
  }
}

void ChatInterceptor::initialize() {
  g_initialized = true;
  g_bootstrapStartTick = (ULONGLONG)GetTickCount64();
}

void ChatInterceptor::shutdown() {
  if (!g_initialized)
    return;
  g_initialized = false;
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    ChatInterceptor::g_playerStatsMap.clear();
  }
  g_onlinePlayers.clear();
  g_processedPlayers.clear();
  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_queuedPlayers.clear();
  }
  g_activeFetches.clear();
  g_playerTeamColor.clear();

  JNIEnv *env = lc->getEnv();
  if (env) {
    g_jCache.cleanup(env);
  }
}

void ChatInterceptor::setMode(int mode) { g_mode = mode; }

static const int MAX_CONCURRENT_FETCHES = 15;

static void queuePlayersForFetching() {
  ULONGLONG now = GetTickCount64();
  if (!g_inHypixelGame && !g_inPreGameLobby)
    return;
  if (g_inHypixelGame && g_lobbyGraceTicks > 0)
    return;
  if (g_onlinePlayers.empty())
    return;

  std::string mode = Config::getOverlayMode();
  if (mode == "invisible" && !Config::isTabEnabled())
    return;

  std::lock_guard<std::mutex> qLock(g_queueMutex);
  std::lock_guard<std::mutex> rLock(g_retryMutex);
  std::lock_guard<std::mutex> aLock(g_activeFetchesMutex);

  int spawnedThisTick = 0;

  for (const auto &name : g_onlinePlayers) {
    if ((int)g_activeFetches.size() >= MAX_CONCURRENT_FETCHES)
      break;
    if (spawnedThisTick >= 4)
      break;

    if (g_processedPlayers.find(name) != g_processedPlayers.end())
      continue;

    if (g_activeFetches.find(name) != g_activeFetches.end())
      continue;

    auto it = g_retryUntil.find(name);
    if (it != g_retryUntil.end() && now < it->second)
      continue;

    std::string uuidToUse = "";
    if (Config::isNickedBypass()) {
      std::lock_guard<std::mutex> lock(g_uuidMapMutex);
      auto itU = g_playerUuidMap.find(name);
      if (itU != g_playerUuidMap.end())
        uuidToUse = itU->second;
    }

    g_activeFetches.insert(name);
    std::thread(fetchWorker, name, uuidToUse).detach();
    spawnedThisTick++;
  }
}

static void processPendingStats() {
  std::string name;
  Hypixel::PlayerStats stats;
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
    if (!g_pendingStatsMap.empty()) {
      auto it = g_pendingStatsMap.begin();
      name = it->first;
      stats = it->second;
      found = true;
      g_pendingStatsMap.erase(it);
    }
  }

  if (found) {
    bool forceOutput = false;
    {
      auto itForce = g_forceChatOutputPlayers.find(name);
      if (itForce != g_forceChatOutputPlayers.end()) {
        forceOutput = true;
        g_forceChatOutputPlayers.erase(itForce);
      }
    }

    bool online = false;
    for (const auto &p : g_onlinePlayers) {
      if (p == name) {
        online = true;
        break;
      }
    }
    if (!online && !forceOutput)
      return;

    double fkdr =
        (stats.bedwarsFinalDeaths == 0)
            ? (double)stats.bedwarsFinalKills
            : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
    std::ostringstream fkdrSs;
    fkdrSs << std::fixed << std::setprecision(2) << fkdr;

    double wlr = (stats.bedwarsLosses == 0)
                     ? (double)stats.bedwarsWins
                     : (double)stats.bedwarsWins / stats.bedwarsLosses;
    std::ostringstream wlrSs;
    wlrSs << std::fixed << std::setprecision(2) << wlr;

    std::string team;
    auto itT = g_playerTeamColor.find(name);
    if (itT != g_playerTeamColor.end())
      team = itT->second;
    else {
      team = resolveTeamForName(name);
      if (!team.empty())
        g_playerTeamColor[name] = team;
    }

    const char *tcol = mcColorForTeam(team);
    const char *black = "\xC2\xA7"
                        "0";
    const char *white = "\xC2\xA7"
                        "f";

    std::string msg;
    if (stats.isNicked) {
      const char *nameColor = (team == "Gray") ? "\xC2\xA7"
                                                 "8"
                                               : (team.empty() ? white : tcol);
      msg = nameColor + name +
            " \xC2\xA7"
            "4[NICKED]";
    } else {
      msg += BedwarsStars::GetFormattedLevel(stats.bedwarsStar);
      msg += " ";

      if (forceOutput || g_inPreGameLobby) {
        auto getRankColor = [](const std::string &col) {
          if (col == "RED")
            return "\xC2\xA7"
                   "c";
          if (col == "GOLD")
            return "\xC2\xA7"
                   "6";
          if (col == "GREEN")
            return "\xC2\xA7"
                   "a";
          if (col == "YELLOW")
            return "\xC2\xA7"
                   "e";
          if (col == "LIGHT_PURPLE")
            return "\xC2\xA7"
                   "d";
          if (col == "WHITE")
            return "\xC2\xA7"
                   "f";
          if (col == "BLUE")
            return "\xC2\xA7"
                   "9";
          if (col == "DARK_GREEN")
            return "\xC2\xA7"
                   "2";
          if (col == "DARK_RED")
            return "\xC2\xA7"
                   "4";
          if (col == "DARK_AQUA")
            return "\xC2\xA7"
                   "3";
          if (col == "DARK_PURPLE")
            return "\xC2\xA7"
                   "5";
          if (col == "DARK_GRAY")
            return "\xC2\xA7"
                   "8";
          if (col == "BLACK")
            return "\xC2\xA7"
                   "0";
          if (col == "DARK_BLUE")
            return "\xC2\xA7"
                   "1";
          return "\xC2\xA7"
                 "c";
        };

        std::string rankDisplay = "\xC2\xA7"
                                  "7";
        if (!stats.prefix.empty()) {
          rankDisplay = stats.prefix + " ";
        } else if (!stats.rank.empty() && stats.rank != "NORMAL") {
          if (stats.rank == "ADMIN")
            rankDisplay = "\xC2\xA7"
                          "c[ADMIN] ";
          else if (stats.rank == "YOUTUBER")
            rankDisplay = "\xC2\xA7"
                          "c[\xC2\xA7"
                          "fYOUTUBE\xC2\xA7"
                          "c] ";
          else if (stats.rank == "MOD")
            rankDisplay = "\xC2\xA7"
                          "2[MOD] ";
          else if (stats.rank == "HELPER")
            rankDisplay = "\xC2\xA7"
                          "9[HELPER] ";
          else
            rankDisplay = "\xC2\xA7"
                          "7[" +
                          stats.rank + "] ";
        } else if (stats.monthlyPackageRank == "SUPERSTAR") {
          std::string pc = getRankColor(stats.rankPlusColor);
          rankDisplay = "\xC2\xA7"
                        "6[MVP" +
                        pc +
                        "++\xC2\xA7"
                        "6] ";
        } else if (stats.newPackageRank == "MVP_PLUS") {
          std::string pc = getRankColor(stats.rankPlusColor);
          rankDisplay = "\xC2\xA7"
                        "b[MVP" +
                        pc +
                        "+\xC2\xA7"
                        "b] ";
        } else if (stats.newPackageRank == "MVP") {
          rankDisplay = "\xC2\xA7"
                        "b[MVP] ";
        } else if (stats.newPackageRank == "VIP_PLUS") {
          rankDisplay = "\xC2\xA7"
                        "a[VIP\xC2\xA7"
                        "6+\xC2\xA7"
                        "a] ";
        } else if (stats.newPackageRank == "VIP") {
          rankDisplay = "\xC2\xA7"
                        "a[VIP] ";
        }

        msg += rankDisplay + name;
        if (!stats.tagsDisplay.empty()) {
          msg += stats.tagsDisplay;
        }

        if (g_mode == 0) {
          msg += std::string(" \xC2\xA7"
                             "7[\xC2\xA7"
                             "fFKDR\xC2\xA7"
                             "7] ") +
                 colorForFkdr(fkdr) + fkdrSs.str();
          msg += std::string(" \xC2\xA7"
                             "7[\xC2\xA7"
                             "fFK\xC2\xA7"
                             "7] ") +
                 colorForFinalKills(stats.bedwarsFinalKills) +
                 std::to_string(stats.bedwarsFinalKills);
          msg += std::string(" \xC2\xA7"
                             "7[\xC2\xA7"
                             "fWins\xC2\xA7"
                             "7] ") +
                 colorForWins(stats.bedwarsWins) +
                 std::to_string(stats.bedwarsWins);
          msg += std::string(" \xC2\xA7"
                             "7[\xC2\xA7"
                             "fWLR\xC2\xA7"
                             "7] ") +
                 colorForWlr(wlr) + wlrSs.str();
        } else {
          msg += " STATS FOUND";
        }
      } else {
        const char *tInit = teamInitial(team);
        if (!team.empty()) {
          msg += black + std::string("[") + tcol + tInit + black +
                 std::string("] ");
        }

        const char *nameColor = (team == "Gray")
                                    ? "\xC2\xA7"
                                      "8"
                                    : (team.empty() ? white : tcol);
        msg += nameColor + name;

        if (g_mode == 0) {
          msg += std::string(" ") + black + "[" + white + "FKDR" + black +
                 "] " + colorForFkdr(fkdr) + fkdrSs.str();
          msg += std::string(" ") + black + "[" + white + "FK" + black + "] " +
                 colorForFinalKills(stats.bedwarsFinalKills) +
                 std::to_string(stats.bedwarsFinalKills);
          msg += std::string(" ") + black + "[" + white + "W" + black + "] " +
                 colorForWins(stats.bedwarsWins) +
                 std::to_string(stats.bedwarsWins);
          msg += std::string(" ") + black + "[" + white + "WLR" + black + "] " +
                 colorForWlr(wlr) + wlrSs.str();
        } else {
          msg += " STATS FOUND";
        }
      }
    }

    if (stats.bedwarsStar == 0 && stats.bedwarsFinalKills == 0 &&
        stats.bedwarsWins == 0) {
      stats.isNicked = true;
    }

    {
      std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
      stats.teamColor = team;
      ChatInterceptor::g_playerStatsMap[name] = stats;
    }

    if (forceOutput || Config::getOverlayMode() == "chat") {
      ChatSDK::showClientMessage(ChatSDK::formatPrefix() + msg);
    }

    if (Config::isTagsEnabled() && !stats.rawTags.empty()) {
      for (const auto &tag : stats.rawTags) {
        if (tag.find("URCHIN:") == 0) {
          std::string type = tag.substr(7);
          std::string check = type;
          for (auto &c : check)
            c = toupper(c);
          if (check.find("BLATANT") != std::string::npos ||
              check.find("SNIPER") != std::string::npos ||
              check.find("CHEATER") != std::string::npos) {
            ChatSDK::showClientMessage(ChatSDK::formatPrefix() +
                                       "\xC2\xA7"
                                       "cALERT: \xC2\xA7"
                                       "f" +
                                       name +
                                       " is tagged as \xC2\xA7"
                                       "l" +
                                       type +
                                       "\xC2\xA7"
                                       "r!");
            Render::NotificationManager::getInstance()->add(
                "Urchin Alert", name + " is a " + type,
                Render::NotificationType::Warning);
            break;
          }
        } else if (tag.find("SERAPH:") == 0) {
          std::string type = tag.substr(7);
          std::string msg = ChatSDK::formatPrefix() + "\xC2\xA7" +
                            "4SERAPH ALERT: \xC2\xA7" + "f" + name +
                            " is blacklisted: \xC2\xA7" + "l" + type +
                            "\xC2\xA7" + "r!";

          if (name == "alperenyancar") {
            msg +=
                " \xC2\xA7"
                "fThis player has been tagged as a sniper during overlay "
                "debugging "
                "due to Seraph mod Zifro's massive ego, terrible gameplay, and "
                "retardedness. Seraph admins and mods are corrupt and abuse "
                "their "
                "power.";

            Render::NotificationManager::getInstance()->add(
                "Seraph Alert", "Player tagged by a corrupt Seraph mod",
                Render::NotificationType::Error);
          } else {
            Render::NotificationManager::getInstance()->add(
                "Seraph Alert", name + " is blacklisted (" + type + ")",
                Render::NotificationType::Error);
          }

          ChatSDK::showClientMessage(msg);
        }
      }
    }

    Logger::info("Stats processed for %s", name.c_str());
    g_processedPlayers.insert(name);
  }
}

void ChatInterceptor::poll() {
  JNIEnv *env = lc->getEnv();
  if (!g_initialized || !env)
    return;
  if (lc->CheckException())
    return;

  ULONGLONG now = GetTickCount64();
  if (g_lastChatReadTick == 0 || (now - g_lastChatReadTick) >= 20) {
    g_lastChatReadTick = now;
    tailLogOnce();

    updateTabListStats();
    detectPreGameLobby();

    if (g_lastTeamScanTick == 0 ||
        (now - g_lastTeamScanTick) >=
            (g_inHypixelGame && (now - g_lastResetTick < 10000) ? 200 : 1000)) {
      g_lastTeamScanTick = now;
      updateTeamsFromScoreboard();
    }

    static ULONGLONG lastCleanup = 0;
    if (lastCleanup == 0 || (now - lastCleanup) >= 10000) {
      lastCleanup = now;
      cleanupStaleStats();
      pruneStatsCache();
    }

    static ULONGLONG lastSync = 0;
    if (lastSync == 0 || (now - lastSync) >= 5000) {
      lastSync = now;
      syncTeamColors();
    }

    static ULONGLONG lastTagSync = 0;
    if (lastTagSync == 0 || (now - lastTagSync) >= 2000) {
      lastTagSync = now;
      syncTags();
    }

    if (Config::isBedDefenseEnabled()) {
      BedDefense::BedDefenseManager::getInstance()->tick();
    }

    queuePlayersForFetching();
    processPendingStats();
  }

  double startTime = TimeUtil::getTime();

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc)
    return;

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_screen = lc->GetFieldID(mcCls, "currentScreen",
                                     "Lnet/minecraft/client/gui/GuiScreen;",
                                     "field_71462_r", "m", "Laxu;");
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = lc->GetFieldID(mcCls, "currentScreen",
                              "Lnet/minecraft/client/gui/GuiScreen;",
                              "field_71462_r", "ay", "Laxu;");
  }

  if (!f_screen) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject screen = env->GetObjectField(mcObj, f_screen);
  if (!screen) {
    env->DeleteLocalRef(mcObj);
    return;
  }

  env->DeleteLocalRef(mcObj);
  env->DeleteLocalRef(screen);

  double endTime = TimeUtil::getTime();

  float rawLatency = (float)((endTime - startTime) * 1000.0);
  if (ChatInterceptor::g_jniLatency == 0.0f) {
    ChatInterceptor::g_jniLatency = rawLatency;
  } else {
    ChatInterceptor::g_jniLatency =
        (ChatInterceptor::g_jniLatency * 0.9f) + (rawLatency * 0.1f);
  }
}

bool ChatInterceptor::isInGame(const std::string &name) {
  std::lock_guard<std::mutex> lock(g_statsMutex);
  return g_playerStatsMap.count(name) > 0;
}

bool ChatInterceptor::shouldAlert(const std::string &name) {
  if (!isInGame(name))
    return false;
  std::lock_guard<std::mutex> lock(g_alertedMutex);
  if (g_alertedPlayers.count(name))
    return false;
  g_alertedPlayers.insert(name);
  return true;
}
bool ChatInterceptor::isInHypixelGame() { return g_inHypixelGame; }
bool ChatInterceptor::isInPreGameLobby() { return g_inPreGameLobby; }

int ChatInterceptor::getGameMode() { return g_mode; }

int ChatInterceptor::getProcessedCount() {
  return (int)g_processedPlayers.size();
}
int ChatInterceptor::getActiveFetchCount() {
  std::lock_guard<std::mutex> lock(g_activeFetchesMutex);
  return (int)g_activeFetches.size();
}
int ChatInterceptor::getPendingStatsCount() {
  std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
  return (int)g_pendingStatsMap.size();
}
float ChatInterceptor::getApiLatency() { return g_apiLatency; }
float ChatInterceptor::getScanSpeed() { return g_scanSpeed; }

bool ChatInterceptor::handleEnterKeyPress() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return false;

  jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                           "Lnet/minecraft/client/Minecraft;",
                                           "field_71432_P", "S", "Lave;");
  if (!mcObj)
    return false;

  jobject screen = lc->GetObjectField(mcObj, "currentScreen",
                                      "Lnet/minecraft/client/gui/GuiScreen;",
                                      "field_71462_r", "m", "Laxu;");
  if (!screen) {
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jclass screenCls = env->GetObjectClass(screen);
  jfieldID f_input = lc->GetFieldID(screenCls, "inputField",
                                    "Lnet/minecraft/client/gui/GuiTextField;",
                                    "field_146415_a", "a", "Lavw;");
  if (!f_input) {
    // try fallback signature search
    f_input = lc->FindFieldBySignature(
        screenCls, "Lnet/minecraft/client/gui/GuiTextField;");
    if (!f_input)
      f_input = lc->FindFieldBySignature(screenCls, "Lavw;");
  }

  if (!f_input) {
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jobject input = env->GetObjectField(screen, f_input);
  if (!input) {
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jclass tfCls = env->GetObjectClass(input);
  jmethodID getText = lc->GetMethodID(tfCls, "getText", "()Ljava/lang/String;",
                                      "func_146179_b", "b");
  if (!getText) {
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jstring jtxt = (jstring)env->CallObjectMethod(input, getText);
  if (!jtxt) {
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  const char *utf = env->GetStringUTFChars(jtxt, 0);
  std::string text = utf ? utf : "";
  if (utf)
    env->ReleaseStringUTFChars(jtxt, utf);

  if (text.empty()) {
    env->DeleteLocalRef(jtxt);
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  size_t pos = 0;
  while (pos < text.size()) {
    unsigned char c = (unsigned char)text[pos];
    if (c <= 32) {
      pos++;
      continue;
    }
    if (c == 0xA7) {
      pos += 2;
      continue;
    }
    break;
  }

  const std::string &cp = ::Config::getCommandPrefix();
  bool isCommandPrefix = (!cp.empty() && text.substr(pos, cp.length()) == cp);

  if (isCommandPrefix && pos + cp.length() < text.size() &&
      text.substr(pos + cp.length(), cp.length()) == cp) {
    isCommandPrefix = false;
  }

  if (pos < text.size() && isCommandPrefix && Config::isCommandsEnabled()) {
    std::string cmdText = text.substr(pos);

    jmethodID mSetText = lc->GetMethodID(
        tfCls, "setText", "(Ljava/lang/String;)V", "func_146180_a", "a");
    if (mSetText) {
      jstring empty = env->NewStringUTF("");
      env->CallVoidMethod(input, mSetText, empty);
      env->DeleteLocalRef(empty);
    }

    CommandRegistry::instance().tryDispatch(cmdText);

    jmethodID mDisplay = lc->GetMethodID(
        mcCls, "displayGuiScreen", "(Lnet/minecraft/client/gui/GuiScreen;)V",
        "func_147108_a", "a", "(Laxu;)V");
    if (mDisplay)
      env->CallVoidMethod(mcObj, mDisplay, nullptr);

    env->DeleteLocalRef(jtxt);
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return true;
  }

  if (Config::isChatBypasserEnabled()) {
    std::string textFromPos = text.substr(pos);
    static const std::vector<std::pair<std::string, int>> bypassableCommands = {
        {"/shout ", 1}, {"/ac ", 1}, {"/pc ", 1},  {"/msg ", 2},
        {"/r ", 1},     {"/w ", 2},  {"/tell ", 2}};

    bool handledAsBypass = false;
    std::string bypassedText;

    if (!textFromPos.empty() && textFromPos[0] == '/') {
      for (const auto &cmdPair : bypassableCommands) {
        if (textFromPos.size() >= cmdPair.first.size()) {
          std::string lower = textFromPos.substr(0, cmdPair.first.size());
          for (auto &ch : lower)
            ch = (char)tolower((unsigned char)ch);

          if (lower == cmdPair.first) {
            size_t currentPos = 0;
            for (int i = 0; i < cmdPair.second; ++i) {
              currentPos = textFromPos.find_first_not_of(" ", currentPos);
              if (currentPos == std::string::npos)
                break;
              currentPos = textFromPos.find_first_of(" ", currentPos);
              if (currentPos == std::string::npos)
                break;
            }

            std::string prefix =
                text.substr(0, pos) + textFromPos.substr(0, currentPos);
            std::string msgPart = (currentPos != std::string::npos)
                                      ? textFromPos.substr(currentPos)
                                      : "";
            bypassedText = prefix + (Config::isSmartChatBypassEnabled()
                                         ? ChatBypasser::smartProcess(msgPart)
                                         : ChatBypasser::process(msgPart));
            handledAsBypass = true;
            break;
          }
        }
      }
    } else {
      bypassedText = Config::isSmartChatBypassEnabled()
                         ? ChatBypasser::smartProcess(text)
                         : ChatBypasser::process(text);
      handledAsBypass = true;
    }

    if (handledAsBypass) {
      jmethodID mSetText = lc->GetMethodID(
          tfCls, "setText", "(Ljava/lang/String;)V", "func_146180_a", "a");
      if (mSetText) {
        jstring empty = env->NewStringUTF("");
        env->CallVoidMethod(input, mSetText, empty);
        env->DeleteLocalRef(empty);

        ChatSDK::sendClientChat(bypassedText);

        jmethodID mDisplay =
            lc->GetMethodID(mcCls, "displayGuiScreen",
                            "(Lnet/minecraft/client/gui/GuiScreen;)V",
                            "func_147108_a", "a", "(Laxu;)V");
        if (mDisplay)
          env->CallVoidMethod(mcObj, mDisplay, nullptr);

        env->DeleteLocalRef(jtxt);
        env->DeleteLocalRef(tfCls);
        env->DeleteLocalRef(input);
        env->DeleteLocalRef(screenCls);
        env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mcObj);
        return true;
      }
    }
  }

  env->DeleteLocalRef(jtxt);
  env->DeleteLocalRef(tfCls);
  env->DeleteLocalRef(input);
  env->DeleteLocalRef(screenCls);
  env->DeleteLocalRef(screen);
  env->DeleteLocalRef(mcObj);
  return false;
}
