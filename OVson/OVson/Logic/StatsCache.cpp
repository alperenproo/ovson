#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/Logger.h"

#include <Windows.h>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace OVson {

void sendTeamStatsReport() {
  if (g_teamReportSent || !Config::isTeamReportEnabled() || g_localTeam.empty())
    return;

  std::vector<Hypixel::PlayerStats> teammates;
  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    for (const auto &pair : g_playerStatsMap) {
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

  const char *f = "\xC2\xA7"
                  "f";
  const char *g = "\xC2\xA7"
                  "7";

  char buf[512];
  snprintf(
      buf, sizeof(buf),
      "%s %sTeam Average %s| %s%.0f* %s| %s%.0f Wins %s| %s%.2f FKDR",
      channel.c_str(), f, g,
      StatColors::getMcColor(StatColors::StatType::Star, avgStar), avgStar, g,
      StatColors::getMcColor(StatColors::StatType::Wins, avgWins), avgWins, g,
      StatColors::getMcColor(StatColors::StatType::FKDR, teamFkdr), teamFkdr);

  ChatSDK::sendClientChat(buf);

  g_teamReportSent = true;
  Logger::info("Automated Team Average Stats Report sent to %s",
               channel.c_str());
}

void pruneStatsCache() {
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

void resetGameCache() {
  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    g_playerStatsMap.clear();
    g_playerTeamColor.clear();
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
  g_player500Retries.clear();
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
  Anticheat::clearAllPlayers();

  g_lastResetTick = GetTickCount64();
  Logger::log(Config::DebugCategory::GameDetection,
              "Game cache reset performed");
}

void cleanupStaleStats() {
  std::vector<std::string> toPrune;
  std::vector<std::string> toResetNicked;

  {
    std::lock_guard<std::mutex> statsLock(g_statsMutex);
    for (auto it = g_playerStatsMap.begin(); it != g_playerStatsMap.end();
         ++it) {
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
            bool isRealName = false;
            {
                std::lock_guard<std::mutex> lockNick(OVson::g_nickMapMutex);
                for (const auto &np : OVson::g_nickToRealMap) {
                    if (np.second == it->first) {
                        isRealName = true;
                        break;
                    }
                }
            }
            if (!isRealName)
                toPrune.push_back(it->first);
        }
      } else {
        if (!found)
          toPrune.push_back(it->first);
      }
    }

    for (const auto &name : toPrune) {
      g_playerStatsMap.erase(name);
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

} // namespace OVson
