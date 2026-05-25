#pragma once
#include "Hypixel.h"
#include <optional>
#include <string>

namespace AbyssService {
enum class LastError {
  None,
  HttpFailure,
  NoPlayerData
};

std::optional<Hypixel::PlayerStats> getPlayerStats(const std::string &uuid);
LastError lastError();

} // namespace AbyssService
