#pragma once
#include <optional>
#include <string>
#include <vector>

namespace Hypixel {
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
  bool isFetched = false;
  bool areTagsFetched = false;
  std::vector<std::string> rawTags;

  std::string prefix;
  std::string rank;
  std::string monthlyPackageRank;
  std::string newPackageRank;
  std::string rankPlusColor;
};

std::optional<std::string> getUuidByName(const std::string &name);
std::optional<PlayerStats> getPlayerStats(const std::string &apiKey,
                                          const std::string &uuid);
} // namespace Hypixel
