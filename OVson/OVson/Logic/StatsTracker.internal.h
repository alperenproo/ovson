#pragma once
#include "../Java.h"
#include "../Services/Hypixel.h"
#include "StatsTracker.h"
#include <Windows.h>
#include <atomic>
#include <jni.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OVson {

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

  void init(JNIEnv *env);
  void cleanup(JNIEnv *env);
};
extern JCache g_jCache;

struct CachedStats {
  Hypixel::PlayerStats stats;
  ULONGLONG timestamp = 0;
  CachedStats() : timestamp(0) {}
  CachedStats(const Hypixel::PlayerStats &s, ULONGLONG t)
      : stats(s), timestamp(t) {}
};
extern std::unordered_map<std::string, CachedStats> g_persistentStatsCache;
extern std::mutex g_cacheMutex;
inline constexpr size_t MAX_STATS_CACHE_SIZE = 500;
inline constexpr ULONGLONG STATS_CACHE_EXPIRY_MS = 600000;

extern bool g_initialized;
extern int g_mode; // 0 bedwars, 1 skywars, 2 duels
extern bool g_inHypixelGame;
extern bool g_inPreGameLobby;

extern std::string g_lastOnlineLine;
extern std::vector<std::string> g_onlinePlayers;
extern size_t g_nextFetchIdx;
extern std::string g_logsDir;
extern std::string g_logFilePath;
extern HANDLE g_logHandle;
extern long long g_logOffset;
extern std::string g_logBuf;

extern std::unordered_map<std::string, std::string> g_pendingTabNames;
extern std::unordered_map<std::string, std::string> g_stableRankMap;
extern std::mutex g_stableRankMutex;

extern bool g_lastInGameStatus;
extern std::string g_lastDetectedModeName;
extern int g_lobbyGraceTicks;
extern bool g_explicitLobbySignal;
extern ULONGLONG g_lastImmediateTeamProbeTick;
extern ULONGLONG g_lastTeamScanTick;
extern ULONGLONG g_lastChatReadTick;
extern ULONGLONG g_lastResetTick;
extern ULONGLONG g_lastDetectionLogTick;
extern ULONGLONG g_bootstrapStartTick;
extern ULONGLONG g_preGameDetectTick;
extern std::string g_localTeam;
extern std::string g_localName;
extern std::unordered_map<std::string, int> g_teamProbeTries;
extern bool g_teamReportSent;

extern std::unordered_set<std::string> g_processedPlayers;
extern std::unordered_set<std::string> g_queuedPlayers;
extern std::unordered_set<std::string> g_alertedPlayers;
extern std::mutex g_alertedMutex;
extern std::mutex g_queueMutex;
extern std::unordered_set<std::string> g_activeFetches;
extern std::mutex g_activeFetchesMutex;
extern std::vector<std::string> g_manualPushedPlayers;
extern std::unordered_set<std::string> g_forceChatOutputPlayers;
extern std::unordered_set<std::string> g_chatPrintedPlayers;
extern std::unordered_map<std::string, ULONGLONG> g_retryUntil;
extern std::mutex g_retryMutex;
extern std::unordered_map<std::string, int> g_playerFetchRetries;
extern std::unordered_map<std::string, int> g_player500Retries;
extern std::unordered_set<std::string> g_eliminatedPlayers;
extern std::mutex g_eliminatedMutex;
extern std::unordered_map<std::string, std::string> g_playerUuidMap;
extern std::mutex g_uuidMapMutex;

void updateTeamsFromScoreboard();
std::string resolveTeamForName(const std::string &name);
std::string resolveTeamForNameEx(JNIEnv *env, const std::string &name,
                                 jobject scoreboard, jmethodID m_getPlayersTeam,
                                 jclass teamCls, jmethodID m_getPrefix);
void setTeamColorSticky(const std::string &name, const std::string &newTeam);
bool isRealBedwarsTeam(const std::string &t);
std::string teamFromColorCode(char code);
void detectTeamsFromLine(const std::string &chat);
const char *mcColorForTeam(const std::string &team);
const char *teamInitial(const std::string &team);

void detectPreGameLobby();
void detectFinalKillsFromLine(const std::string &chat);
void detectBedDestructionFromLine(const std::string &chat);
void updateTabListStats();
void syncTeamColors();
void syncTags();
void resetGameCache();
void cleanupStaleStats();
void pruneStatsCache();
void sendTeamStatsReport();
void fetchWorker(std::string name, std::string forcedUuid = "");
void fetchWorkerBody(const std::string &name, const std::string &forcedUuid);
void queuePlayersForFetching();
void processPendingStats();
std::string getUserProfileDir();
std::string getAppDataDir();
std::vector<std::string> getLogDirectoryCandidates();
std::string findNewestLogFile(const std::string &dir);
bool ensureLogOpen();
void parsePlayersFromOnlineLine(const std::string &joined);
void tailLogOnce();
void pollBody();

} // namespace OVson
