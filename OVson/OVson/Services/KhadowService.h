#pragma once
#include <optional>
#include <string>

namespace Khadow {

struct AnticheatInfo {
  bool        urchinBlacklisted = false;
  std::string urchinType;
  std::string urchinReason;
  bool        seraphBlacklisted = false;
  std::string seraphType;
  std::string seraphReason;
};

std::optional<AnticheatInfo> getPlayerAnticheat(const std::string &username,
                                                bool wait = false);
void clearCache();

} // namespace Khadow
