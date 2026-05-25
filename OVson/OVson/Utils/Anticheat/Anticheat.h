#pragma once
#include <string>
namespace Anticheat {
    void initialize();
    void shutdown();
    void clearAllPlayers();
    bool isPlayerFlagged(const std::string& name);
    bool isPlayerSneaking(const std::string& name);
    size_t getTrackedPlayersSnapshot(std::string &outJoined);
    void *getWorldPlayerEntitiesFieldID();
} // namespace Anticheat
