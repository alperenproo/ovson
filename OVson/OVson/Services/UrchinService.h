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

std::optional<PlayerTags> getPlayerTags(const std::string &username,
                                        bool wait = false);
void clearCache();
bool hasAnyTags(const std::string &username);
} // namespace Urchin
