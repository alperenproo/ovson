#pragma once
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Urchin {
struct Tag {
  std::string type; // e.g., "closet_cheater", "blatant_cheater"
  std::string reason;
};

struct PlayerTags {
  std::string uuid;
  std::vector<Tag> tags;
};

struct MonthlyStats {
  std::string uuid;
  std::string displayName;
  int64_t fromTimestamp = 0;
  std::string fromReadable;
  int finalKills = 0;
  int finalDeaths = 0;
  int wins = 0;
  int losses = 0;
  int bedsBroken = 0;
  int bedsLost = 0;
  int kills = 0;
  int deaths = 0;
  double fkdr = 0.0;
  double wlr = 0.0;
  bool hasData = false;
};

std::optional<PlayerTags> getPlayerTags(const std::string &username,
                                        bool wait = false);
std::optional<MonthlyStats> getMonthlyStats(const std::string &player,
                                            bool wait = false);
void clearCache();
void clearMonthlyCache();
bool hasAnyTags(const std::string &username);

} // namespace Urchin
