#pragma once
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace Hypixel {
inline int calculateNetworkLevel(double exp) {
  if (exp <= 0.0)
    return 1;
  return (int)std::floor((std::sqrt(2.0 * exp + 30625.0) / 50.0) - 2.5);
}
struct PlayerStats {
  std::string tagsDisplay;
  std::string uuid;
  std::string displayName;
  int networkLevel = 0;
  int bedwarsStar = 0;
  int bedwarsFinalKills = 0;
  int bedwarsFinalDeaths = 0;
  int bedwarsKills = 0;
  int bedwarsDeaths = 0;
  int bedwarsBedsBroken = 0;
  int bedwarsBedsLost = 0;
  int bedwarsWins = 0;
  int bedwarsLosses = 0;
  int inGameHealth = 20;
  bool healthKnown = false;   // true once a real scoreboard HP was read
  int winstreak = 0;
  int lastPing = -1;
  int auroraPing = -1;
  int auroraPingRecentAvg = -1;
  std::string teamColor;
  bool isNicked = false;
  bool isFresh = false;
  bool isFetched = false;
  bool areTagsFetched = false;
  std::vector<std::string> rawTags;

  std::string prefix;
  std::string rank;
  std::string monthlyPackageRank;
  std::string newPackageRank;
  std::string packageRank;
  std::string rankPlusColor;

  std::string activeStar;
  std::string activePrestigeScheme;
  std::string activePrestigeBracket;

  int ironCollected = 0;
  int goldCollected = 0;
  int diamondCollected = 0;
  int emeraldCollected = 0;
  int resourcesCollected = 0;

  int slumberTickets = 0;

  std::string activeDeathCry;
  std::string activeKillEffect;
  std::string activeVictoryDance;
  std::string activeProjectileTrail;
  std::string activeIslandTopper;
  std::string activeGlyph;
  std::string activeBedDestroy;

  std::string quickBuy;
  std::string favoriteSlots;
};

inline bool isFreshAccount(const PlayerStats &s) {
  if (s.isNicked) return false;
  if (!s.isFetched) return false;
  if (s.displayName.empty()) return false;
  if (s.isFresh) return true;
  if (s.bedwarsStar <= 1 && s.bedwarsFinalKills == 0 && s.bedwarsWins == 0) {
    return true;
  }
  return false;
}

std::optional<std::string> getUuidByName(const std::string &name);
std::optional<std::string> getUuidByName(const std::string &name, std::string *outExactName);
std::optional<PlayerStats> getPlayerStats(const std::string &apiKey,
                                          const std::string &uuid);
} // namespace Hypixel
