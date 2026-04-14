#pragma once
#include "Hypixel.h"
#include <optional>
#include <string>

namespace AbyssService {
    std::optional<Hypixel::PlayerStats> getPlayerStats(const std::string& uuid);
}
