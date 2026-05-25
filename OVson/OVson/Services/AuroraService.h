#pragma once
#include <optional>
#include <string>
#include <vector>

namespace Aurora {

struct PlayerMatch {
  std::string name;
  int distance;
};

struct QueryResult {
  bool success;
  std::vector<PlayerMatch> data;
};

struct PingEntry {
  int min;
  int avg;
  int max;
  long long timestamp;
};

struct PingResult {
  bool success;
  std::vector<PingEntry> data;
};

std::optional<QueryResult> queryStats(const std::string &type,
                                      const std::string &value, int range,
                                      int maxResults,
                                      const std::string &apiKey);

std::optional<PingResult> queryPingHistory(const std::string &uuid,
                                           const std::string &apiKey);

} // namespace Aurora
