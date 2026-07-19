#pragma once
#include "Hypixel.h"
#include <optional>
#include <string>

namespace PrismService {
enum class LastError {
  None,
  HttpFailure,
  NoPlayerData,
  RateLimited,
  InternalServerError
};

std::optional<Hypixel::PlayerStats> getPlayerStats(const std::string &uuid);
LastError lastError();

} // namespace PrismService
