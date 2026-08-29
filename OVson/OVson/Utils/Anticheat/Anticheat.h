#pragma once
#include <cstdint>
#include <string>
namespace Anticheat {
    void initialize();
    void shutdown();
    void clearAllPlayers();
    bool isPlayerFlagged(const std::string& name);
    bool isPlayerSneaking(const std::string& name);
    size_t getTrackedPlayersSnapshot(std::string &outJoined);
    void *getWorldPlayerEntitiesFieldID();
    void observeConfirmedBlockHit(int attackerEntityId, std::uint64_t atMs,
                                  double distance, bool blockingKnown,
                                  bool blocking, bool holdingSword,
                                  bool healthConfirmed,
                                  bool velocityConfirmed);
    void resetAutoBlockEvidence();
    void abLog(const char *fmt, ...);
} // namespace Anticheat
