#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Render/NotificationManager.h"
#include "../Services/AbyssService.h"
#include "../Render/RenderHook.h"
#include "../Services/AuroraService.h"
#include "../Services/Hypixel.h"
#include "../Services/KhadowService.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/Logger.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/ThreadTracker.h"
#include "../Utils/Timer.h"

#include <Windows.h>
#include <atomic>
#include <cctype>
#include <functional>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace OVson {

static const int MAX_CONCURRENT_FETCHES = 15;

static std::mutex g_renderRequestMutex;
static std::unordered_map<std::string, ULONGLONG> g_renderRequestLast;

void requestStatsForVisiblePlayer(const std::string &name) {
  if (name.empty() || name.length() > 16) return;
  for (char c : name) {
    if (!isalnum((unsigned char)c) && c != '_') return;
  }
  {
    std::lock_guard<std::mutex> lk(g_statsMutex);
    if (g_playerStatsMap.find(name) != g_playerStatsMap.end()) return;
  }
  ULONGLONG now = GetTickCount64();
  {
    std::lock_guard<std::mutex> lk(g_renderRequestMutex);
    auto it = g_renderRequestLast.find(name);
    if (it != g_renderRequestLast.end() && now - it->second < 30000)
      return;
    g_renderRequestLast[name] = now;
  }
  std::thread(fetchWorker, name, std::string("")).detach();
}

void fetchWorker(std::string name, std::string forcedUuid) {
  ThreadTracker::increment();
  auto _cleanup = []() { ThreadTracker::decrement(); };
  struct CleanupGuard {
    std::function<void()> f;
    ~CleanupGuard() {
      if (f)
        f();
    }
  } guard{_cleanup};

  SafeGuard::installSehTranslator();
  SafeGuard::run("fetchWorker", [&]() { fetchWorkerBody(name, forcedUuid); });
}

void fetchWorkerBody(const std::string &name, const std::string &forcedUuid) {
  if (!g_initialized)
    return;
  if (name.empty() || name.length() > 16)
    return;
  for (char c : name) {
    if (!isalnum((unsigned char)c) && c != '_')
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
        static std::atomic<ULONGLONG> lastAbyssSuggest{0};

        statsOpt = AbyssService::getPlayerStats(*uuid);
        AbyssService::LastError lastErr = AbyssService::lastError();
        Logger::info(
            "Abyss fetch for %s uuid=%s -> %s (lastErr=%d)", name.c_str(),
            uuid->c_str(),
            statsOpt ? "ok" : "nullopt", (int)lastErr);

        if (lastErr == AbyssService::LastError::HttpFailure) {
          int fails = ++abyssFailCount;
          ULONGLONG nowMs = GetTickCount64();
          ULONGLONG lastShown = lastAbyssSuggest.load();
          Logger::info("Abyss HTTP failure #%d for %s (since last warn: %llu ms)",
                       fails, name.c_str(),
                       (unsigned long long)(nowMs - lastShown));
          if (fails >= 5 && (nowMs - lastShown > 300000)) {
            lastAbyssSuggest.store(nowMs);
            abyssFailCount = 0;
            Logger::info("Abyss: posting 'API failing' notice to chat "
                         "(5 consecutive HTTP failures)");
            RenderHook::enqueueTask([]() {
              ChatSDK::showPrefixed(
                  "§cAbyss API is failing. §eSuggestion: Use a personal "
                  "Hypixel API key (.api new <key>) for better reliability.");
            });
          }
        } else {
          abyssFailCount = 0;
        }
      } else {
        statsOpt = Hypixel::getPlayerStats(apiKey, *uuid);
      }

      double apiEnd = TimeUtil::getTime();
      float lastApiLat = (float)(apiEnd - apiStart) * 1000.0f;
      if (g_apiLatency == 0.0f)
        g_apiLatency = lastApiLat;
      else
        g_apiLatency = g_apiLatency * 0.9f + lastApiLat * 0.1f;

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
  }

  std::string auroraKey = Config::getAuroraApiKey();
  if (!auroraKey.empty() && !fetchError && isInHypixelGame() &&
      !isInPreGameLobby()) {
    std::string currentUuid = cacheFound ? cachedData.uuid : uuidToF;
    if (!currentUuid.empty()) {
      auto pingRes = Aurora::queryPingHistory(currentUuid, auroraKey);
      if (pingRes && pingRes->success && !pingRes->data.empty()) {
        Hypixel::PlayerStats &targetStats =
            cacheFound ? cachedData : fetchedStats;

        targetStats.auroraPing = pingRes->data[0].avg;

        int sum = 0, count = 0;
        for (size_t i = 0; i < pingRes->data.size() && i < 5; ++i) {
          sum += pingRes->data[i].avg;
          count++;
        }
        if (count > 0)
          targetStats.auroraPingRecentAvg = sum / count;

        std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
        g_pendingStatsMap[name] = targetStats;
      }
    }
  }

  bool shouldFetchTags = false;
  if (OVson::shouldAutoFetchTags()) {
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
        c = (char)toupper((unsigned char)c);
      if (t.find("BLATANT") != std::string::npos)
        return "\xC2\xA7"
               "4[BC]";
      if (t.find("CLOSET") != std::string::npos)
        return "\xC2\xA7"
               "4[CC]";
      if (t.find("CONFIRMED") != std::string::npos)
        return "\xC2\xA7"
               "5[C]";
      if (t.find("CHEATER") != std::string::npos)
        return "\xC2\xA7"
               "5[C]";
      if (t.find("CAUTION") != std::string::npos)
        return "\xC2\xA7"
               "e[!]";
      if (t.find("SNIPER") != std::string::npos)
        return "\xC2\xA7"
               "6[S]";
      return "";
    };

    if (activeS == "Khadow") {
      auto kh = Khadow::getPlayerAnticheat(name, true);
      rTags.push_back("URCHIN_CHECKED");
      rTags.push_back("SERAPH_CHECKED");
      if (kh) {
        if (kh->urchinBlacklisted) {
          std::string a = getAbbr(kh->urchinType);
          tagStr += " " + (a.empty() ? "\xC2\xA7" "4[U]" : a);
          rTags.push_back("URCHIN:" + kh->urchinType + "\x1F" +
                          kh->urchinReason);
        }
        if (kh->seraphBlacklisted) {
          std::string a = getAbbr(kh->seraphType);
          tagStr += " " + (a.empty() ? "\xC2\xA7" "4[S]" : a);
          rTags.push_back("SERAPH:" + kh->seraphType + "\x1F" +
                          kh->seraphReason);
        }
      }
    }
    if (activeS == "Urchin" || activeS == "Both") {
      auto uT = Urchin::getPlayerTags(name, true);
      rTags.push_back("URCHIN_CHECKED");
      if (uT && !uT->tags.empty()) {
        std::string a = getAbbr(uT->tags[0].type);
        tagStr += " " + (a.empty() ? "\xC2\xA7"
                                     "4[U]"
                                   : a);
        for (const auto &t : uT->tags)
          rTags.push_back("URCHIN:" + t.type + "\x1F" + t.reason);
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
          rTags.push_back("SERAPH:" + t.type + "\x1F" + t.reason);
      }
    }

    targetStats.tagsDisplay = tagStr;
    targetStats.rawTags = rTags;
    targetStats.areTagsFetched = true;
    if (cacheFound) {
      std::lock_guard<std::mutex> lock(g_cacheMutex);
      g_persistentStatsCache[name] = {cachedData, now};
    }
  }

  if (cacheFound) {
    {
      std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
      g_pendingStatsMap[name] = cachedData;
    }
    {
      std::lock_guard<std::mutex> lockQ(g_queueMutex);
      g_processedPlayers.insert(name);
    }
  } else if (!fetchError) {
    {
      std::lock_guard<std::mutex> lock(g_cacheMutex);
      g_persistentStatsCache[name] = {fetchedStats, now};
    }
    {
      std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
      g_pendingStatsMap[name] = fetchedStats;
    }
    {
      std::lock_guard<std::mutex> lockQ(g_queueMutex);
      g_processedPlayers.insert(name);
    }
  }

  if (fetchError && !cacheFound) {
    bool shouldNick = false;
    {
      std::lock_guard<std::mutex> lock(g_retryMutex);
      int count = ++g_playerFetchRetries[name];
      if (count < 5) {
        g_retryUntil[name] = now + 2000;
      } else {
        shouldNick = true;
      }
    }
    if (shouldNick) {
      Hypixel::PlayerStats nickedStats;
      nickedStats.isNicked = true;
      nickedStats.isFetched = true;
      {
        std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
        g_pendingStatsMap[name] = nickedStats;
      }
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_persistentStatsCache[name] = {nickedStats, now};
      }
      {
        std::lock_guard<std::mutex> lockQ(g_queueMutex);
        g_processedPlayers.insert(name);
      }
      fetchError = false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_activeFetchesMutex);
    g_activeFetches.erase(name);
  }
}

void queuePlayersForFetching() {
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

void processPendingStats() {
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

  if (!found)
    return;

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
      setTeamColorSticky(name, team);
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
      if (!stats.tagsDisplay.empty())
        msg += stats.tagsDisplay;

      if (g_mode == 0) {
        msg += std::string(" \xC2\xA7"
                           "7[\xC2\xA7"
                           "fFKDR\xC2\xA7"
                           "7] ") +
               StatColors::getMcColor(StatColors::StatType::FKDR, fkdr) +
               fkdrSs.str();
        msg += std::string(" \xC2\xA7"
                           "7[\xC2\xA7"
                           "fFK\xC2\xA7"
                           "7] ") +
               StatColors::getMcColor(StatColors::StatType::FinalKills,
                                      (double)stats.bedwarsFinalKills) +
               std::to_string(stats.bedwarsFinalKills);
        msg += std::string(" \xC2\xA7"
                           "7[\xC2\xA7"
                           "fWins\xC2\xA7"
                           "7] ") +
               StatColors::getMcColor(StatColors::StatType::Wins,
                                      (double)stats.bedwarsWins) +
               std::to_string(stats.bedwarsWins);
        msg += std::string(" \xC2\xA7"
                           "7[\xC2\xA7"
                           "fWLR\xC2\xA7"
                           "7] ") +
               StatColors::getMcColor(StatColors::StatType::WLR, wlr) +
               wlrSs.str();
      } else {
        msg += " STATS FOUND";
      }
    } else {
      const char *tInit = teamInitial(team);
      if (!team.empty()) {
        msg +=
            black + std::string("[") + tcol + tInit + black + std::string("] ");
      }

      const char *nameColor = (team == "Gray") ? "\xC2\xA7"
                                                 "8"
                                               : (team.empty() ? white : tcol);
      msg += nameColor + name;

      if (g_mode == 0) {
        msg += std::string(" ") + black + "[" + white + "FKDR" + black + "] " +
               StatColors::getMcColor(StatColors::StatType::FKDR, fkdr) +
               fkdrSs.str();
        msg += std::string(" ") + black + "[" + white + "FK" + black + "] " +
               StatColors::getMcColor(StatColors::StatType::FinalKills,
                                      (double)stats.bedwarsFinalKills) +
               std::to_string(stats.bedwarsFinalKills);
        msg += std::string(" ") + black + "[" + white + "W" + black + "] " +
               StatColors::getMcColor(StatColors::StatType::Wins,
                                      (double)stats.bedwarsWins) +
               std::to_string(stats.bedwarsWins);
        msg += std::string(" ") + black + "[" + white + "WLR" + black + "] " +
               StatColors::getMcColor(StatColors::StatType::WLR, wlr) +
               wlrSs.str();
      } else {
        msg += " STATS FOUND";
      }
    }
  }

  if (stats.bedwarsStar == 0 && stats.bedwarsFinalKills == 0 &&
      stats.bedwarsWins == 0) {
    stats.isNicked = true;
  }

  std::string cleanName;
  for (size_t i = 0; i < name.length(); ++i) {
    char c = name[i];
    if (c >= 'A' && c <= 'Z')
      cleanName += (char)(c + 32);
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
      cleanName += c;
  }

  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    stats.teamColor = team;
    if (!cleanName.empty())
      g_playerStatsMap[cleanName] = stats;
    g_playerStatsMap[name] = stats;
  }

  if (forceOutput || Config::getOverlayMode() == "chat") {
    std::string text = ChatSDK::formatPrefix() + msg;
    RenderHook::enqueueTask([text]() {
      ChatSDK::showClientMessage(text);
    });
  }

  if (Config::isTagsEnabled() && !stats.rawTags.empty()) {
    auto splitTagPayload = [](const std::string &payload)
        -> std::pair<std::string, std::string> {
      auto sep = payload.find('\x1F');
      if (sep == std::string::npos) return { payload, "" };
      return { payload.substr(0, sep), payload.substr(sep + 1) };
    };
    auto trimRedundantPrefix = [](std::string reason,
                                  const std::string &type) -> std::string {
      if (reason.empty() || type.empty()) return reason;
      auto iequals = [](const std::string &a, const std::string &b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
          if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return false;
        return true;
      };
      if (reason.size() > type.size() &&
          iequals(reason.substr(0, type.size()), type)) {
        reason.erase(0, type.size());
        while (!reason.empty() &&
               (reason.front() == ':' || reason.front() == ' '))
          reason.erase(reason.begin());
      }
      return reason;
    };
    bool urchinAlerted = false;
    bool seraphAlerted = false;
    for (const auto &tag : stats.rawTags) {
      if (!urchinAlerted && tag.find("URCHIN:") == 0 &&
          OVson::shouldAlert(name + ":URCHIN")) {
        auto [type, reason] = splitTagPayload(tag.substr(7));
        reason = trimRedundantPrefix(reason, type);
        std::string umsg = ChatSDK::formatPrefix() + "\xC2\xA7" +
                           "4URCHIN ALERT: \xC2\xA7" + "f" + name +
                           " is tagged as \xC2\xA7" + "l" + type +
                           "\xC2\xA7" + "r!";
        if (!reason.empty()) {
          umsg += " \xC2\xA7" "7(reason: \xC2\xA7" "f" + reason +
                  "\xC2\xA7" "7)";
        }
        RenderHook::enqueueTask([umsg]() {
          ChatSDK::showClientMessage(umsg);
        });
        Render::NotificationManager::getInstance()->add(
            "Urchin Alert", name + " is tagged " + type,
            Render::NotificationType::Warning);
        urchinAlerted = true;
      } else if (!seraphAlerted && tag.find("SERAPH:") == 0 &&
                 OVson::shouldAlert(name + ":SERAPH")) {
        auto [type, reason] = splitTagPayload(tag.substr(7));
        reason = trimRedundantPrefix(reason, type);
        std::string smsg = ChatSDK::formatPrefix() + "\xC2\xA7" +
                           "4SERAPH ALERT: \xC2\xA7" + "f" + name +
                           " is blacklisted: \xC2\xA7" + "l" + type +
                           "\xC2\xA7" + "r!";
        if (!reason.empty()) {
          smsg += " \xC2\xA7" "7(reason: \xC2\xA7" "f" + reason +
                  "\xC2\xA7" "7)";
        }

        if (name == "alperenyancar") {
          smsg += " \xC2\xA7"
                  "fThis player has been tagged as a sniper during "
                  "overlay debugging due to Seraph mod Zifro's massive ego, "
                  "terrible gameplay, and retardedness. Seraph admins and "
                  "mods are corrupt and abuse their power.";

          Render::NotificationManager::getInstance()->add(
              "Seraph Alert", "Player tagged by a corrupt Seraph mod",
              Render::NotificationType::Error);
        } else {
          Render::NotificationManager::getInstance()->add(
              "Seraph Alert", name + " is blacklisted (" + type + ")",
              Render::NotificationType::Error);
        }

        RenderHook::enqueueTask([smsg]() {
          ChatSDK::showClientMessage(smsg);
        });
        seraphAlerted = true;
      }
      if (urchinAlerted && seraphAlerted) break;
    }
  }

  Logger::info("Stats processed for %s", name.c_str());
  g_processedPlayers.insert(name);
}

} // namespace OVson
