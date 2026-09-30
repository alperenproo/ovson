#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/Logger.h"
#include "../Utils/ColoredHitboxes.h"

#include <Windows.h>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include "../Render/RenderHook.h"

namespace OVson {

bool areAllGameStatsReady() {
  if (!g_inHypixelGame || g_inPreGameLobby)
    return false;

  if (g_onlinePlayers.empty())
    return false;

  {
    std::lock_guard<std::mutex> aLock(g_activeFetchesMutex);
    if (!g_activeFetches.empty())
      return false;
  }

  {
    std::lock_guard<std::mutex> pLock(g_pendingStatsMutex);
    if (!g_pendingStatsMap.empty())
      return false;
  }

  {
    std::lock_guard<std::mutex> qLock(g_queueMutex);
    std::lock_guard<std::recursive_mutex> sLock(g_statsMutex);

    int validPlayers = 0;
    for (const auto &rawName : g_onlinePlayers) {
      if (rawName.empty() || rawName.length() > 16)
        continue;
      bool validChar = true;
      for (char c : rawName) {
        if (!isalnum((unsigned char)c) && c != '_') {
          validChar = false;
          break;
        }
      }
      if (!validChar)
        continue;

      validPlayers++;

      bool processed = (g_processedPlayers.find(rawName) != g_processedPlayers.end());
      bool hasStats = (g_playerStatsMap.find(rawName) != g_playerStatsMap.end());

      if (!processed && !hasStats && g_isNicked && !g_realUsername.empty() && rawName == g_activeNick) {
        if (g_processedPlayers.find(g_realUsername) != g_processedPlayers.end() ||
            g_playerStatsMap.find(g_realUsername) != g_playerStatsMap.end()) {
          processed = true;
        }
      }

      if (!hasStats) {
        std::string cleanName;
        for (char c : rawName) cleanName += (char)tolower((unsigned char)c);
        if (g_playerStatsMap.find(cleanName) != g_playerStatsMap.end()) {
          hasStats = true;
        }
      }

      if (!processed && !hasStats) {
        return false;
      }
    }

    if (validPlayers == 0)
      return false;
  }

  return true;
}

void sendTeamStatsReport(bool force, std::string channelOverride) {
  if (Config::isGlobalDebugEnabled() || force) {
    Logger::info("[OVson DEBUG] sendTeamStatsReport triggered. force=%d g_teamReportSent=%d", force, g_teamReportSent);
  }

  if (!force) {
    if (g_teamReportSent || !Config::isTeamReportEnabled())
      return;
  }

  std::string myName = !g_localName.empty() ? g_localName : getRealLocalUsername();
  if (myName.empty() && g_isNicked && !g_activeNick.empty()) myName = g_activeNick;
  if (!myName.empty()) {
    std::string tabTeam = resolveTeamFromTabList(myName);
    if (isRealBedwarsTeam(tabTeam)) {
      g_localTeam = tabTeam;
      setTeamColorSticky(myName, tabTeam, true);
      g_helmetTeamSet.insert(myName);
    } else if (g_localTeam.empty()) {
      std::string resolved = resolveTeamForName(myName);
      if (isRealBedwarsTeam(resolved)) {
        g_localTeam = resolved;
      }
    }
  }

  struct TeamEntry {
    std::string teamName;
    int totalFk = 0;
    int totalFd = 0;
    int totalWins = 0;
    int validStatsCount = 0;
    int nickedCount = 0;
    float avgFkdr = 0.0f;
    float avgWins = 0.0f;
    float avgFk = 0.0f;
  };

  std::unordered_map<std::string, TeamEntry> teamMap;
  std::unordered_set<std::string> seenPlayersLower;

  {
    std::lock_guard<std::recursive_mutex> statsLock(g_statsMutex);

    std::vector<std::string> playerList;
    if (!g_onlinePlayers.empty()) {
      playerList = g_onlinePlayers;
    } else {
      for (const auto &pair : g_playerStatsMap) {
        playerList.push_back(pair.first);
      }
    }

    std::string realLocal = getRealLocalUsername();

    for (const auto &rawName : playerList) {
      if (rawName.empty() || rawName.length() > 16)
        continue;
      bool validChar = true;
      for (char c : rawName) {
        if (!isalnum((unsigned char)c) && c != '_') {
          validChar = false;
          break;
        }
      }
      if (!validChar)
        continue;

      std::string lowerName = rawName;
      for (char &c : lowerName) c = (char)tolower((unsigned char)c);

      if (seenPlayersLower.count(lowerName))
        continue;
      seenPlayersLower.insert(lowerName);

      bool isLocalPlayer = (!g_localName.empty() && rawName == g_localName) ||
                           (!realLocal.empty() && rawName == realLocal) ||
                           (g_isNicked && !g_activeNick.empty() && rawName == g_activeNick);

      if (isLocalPlayer) {
        if (!realLocal.empty()) {
          std::string lReal = realLocal;
          for (char &c : lReal) c = (char)tolower((unsigned char)c);
          seenPlayersLower.insert(lReal);
        }
        if (!g_activeNick.empty()) {
          std::string lNick = g_activeNick;
          for (char &c : lNick) c = (char)tolower((unsigned char)c);
          seenPlayersLower.insert(lNick);
        }
      }

      std::string team = "Unknown";
      if (isLocalPlayer) {
        if (!g_localTeam.empty() && isRealBedwarsTeam(g_localTeam)) {
          team = g_localTeam;
        } else {
          std::string tabResolved = resolveTeamFromTabList(rawName);
          if (isRealBedwarsTeam(tabResolved)) {
            team = tabResolved;
            g_localTeam = tabResolved;
            setTeamColorSticky(rawName, tabResolved, true);
            g_helmetTeamSet.insert(rawName);
          }
        }
      } else {
        team = resolveTeamForName(rawName);
        if (team.empty() || team == "Unknown") {
          std::string tabResolved = resolveTeamFromTabList(rawName);
          if (isRealBedwarsTeam(tabResolved)) {
            team = tabResolved;
            setTeamColorSticky(rawName, tabResolved);
          }
        }
      }

      if (team.empty() || team == "Unknown") {
        auto itT = g_playerTeamColor.find(rawName);
        if (itT != g_playerTeamColor.end() && !itT->second.empty()) {
          team = itT->second;
        } else {
          for (const auto &tp : g_playerTeamColor) {
            std::string tKeyLower = tp.first;
            for (char &c : tKeyLower) c = (char)tolower((unsigned char)c);
            if (tKeyLower == lowerName && !tp.second.empty()) {
              team = tp.second;
              break;
            }
          }
        }
      }

      if (!isRealBedwarsTeam(team))
        continue;

      Hypixel::PlayerStats stats;
      bool foundStats = false;

      std::string lookupName = rawName;
      if (isLocalPlayer && !realLocal.empty()) {
        lookupName = realLocal;
      }

      auto itS = g_playerStatsMap.find(lookupName);
      if (itS != g_playerStatsMap.end()) {
        stats = itS->second;
        foundStats = true;
      } else {
        std::string lLookup = lookupName;
        for (char &c : lLookup) c = (char)tolower((unsigned char)c);
        auto itSL = g_playerStatsMap.find(lLookup);
        if (itSL != g_playerStatsMap.end()) {
          stats = itSL->second;
          foundStats = true;
        } else if (isLocalPlayer && !g_activeNick.empty()) {
          auto itSN = g_playerStatsMap.find(g_activeNick);
          if (itSN != g_playerStatsMap.end()) {
            stats = itSN->second;
            foundStats = true;
          }
        }
      }

      TeamEntry &entry = teamMap[team];
      entry.teamName = team;

      bool isNicked = false;
      if (foundStats) {
        if (stats.isNicked) {
          isNicked = true;
        }
      } else {
        isNicked = true;
      }

      if (isLocalPlayer && !realLocal.empty() && foundStats && !stats.isNicked) {
        isNicked = false;
      }

      if (isNicked) {
        entry.nickedCount++;
      } else if (foundStats) {
        entry.validStatsCount++;
        entry.totalFk += stats.bedwarsFinalKills;
        entry.totalFd += stats.bedwarsFinalDeaths;
        entry.totalWins += stats.bedwarsWins;
      }
    }
  }

  if (teamMap.empty())
    return;

  std::vector<TeamEntry> teamList;
  for (auto &pair : teamMap) {
    TeamEntry &entry = pair.second;
    if (entry.validStatsCount > 0) {
      entry.avgFkdr = (entry.totalFd > 0) ? (float)entry.totalFk / (float)entry.totalFd : (float)entry.totalFk;
      entry.avgWins = (float)entry.totalWins / (float)entry.validStatsCount;
      entry.avgFk = (float)entry.totalFk / (float)entry.validStatsCount;
    } else {
      entry.avgFkdr = 0.0f;
      entry.avgWins = 0.0f;
      entry.avgFk = 0.0f;
    }
    teamList.push_back(entry);
  }

  std::sort(teamList.begin(), teamList.end(), [](const TeamEntry &a, const TeamEntry &b) {
    if (a.avgFkdr != b.avgFkdr)
      return a.avgFkdr > b.avgFkdr;
    return a.avgWins > b.avgWins;
  });

  std::string channel = channelOverride.empty() ? Config::getTeamReportChannel() : channelOverride;

  if (!force) {
    g_teamReportSent = true;
    Logger::info("Automated All-Team Stats Report triggered to %s (%d teams)", channel.c_str(), (int)teamList.size());
  }

  std::thread([teamList, channel]() {
    for (const auto &t : teamList) {
      std::ostringstream oss;
      if (!channel.empty()) {
        oss << channel << " ";
      }
      oss << "[" << t.teamName << "] ";

      if (t.validStatsCount == 0 && t.nickedCount > 0) {
        oss << "Nicked Team (" << t.nickedCount << " nicked)";
      } else {
        oss << "FKDR: " << std::fixed << std::setprecision(2) << t.avgFkdr
            << " | Wins: " << (int)t.avgWins
            << " | FK: " << (int)t.avgFk;
        if (t.nickedCount > 0) {
          oss << " (" << t.nickedCount << " nicked)";
        }
      }

      std::string msg = oss.str();
      RenderHook::enqueueTask([msg]() { ChatSDK::sendClientChat(msg); });
      Sleep(600);
    }
  }).detach();
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
    std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
    g_playerStatsMap.clear();
    g_playerTeamColor.clear();
  }
  OVson::Utils::clearHitboxColorCache();
  {
    std::lock_guard<std::mutex> lockR(g_stableRankMutex);
    g_stableRankMap.clear();
  }
  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_processedPlayers.clear();
    g_queuedPlayers.clear();
  }
  {
    std::lock_guard<std::mutex> pLock(g_pendingStatsMutex);
    g_pendingStatsMap.clear();
  }
  g_onlinePlayers.clear();
  g_teamReportSent = false;
  g_localTeam.clear();
  g_helmetTeamSet.clear();
  g_playerTeamColor.clear();
  g_playerFetchRetries.clear();
  g_player500Retries.clear();
  {
    std::lock_guard<std::mutex> lock(g_alertedMutex);
    g_alertedPlayers.clear();
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
    std::lock_guard<std::recursive_mutex> statsLock(g_statsMutex);
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
