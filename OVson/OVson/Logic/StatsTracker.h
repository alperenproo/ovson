#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Services/Hypixel.h"

namespace OVson {

void initialize();
void poll();

void requestStatsForVisiblePlayer(const std::string &name,
                                  const std::string &forcedUuid = "");
void shutdown();
bool handleEnterKeyPress();

// 0 = bedwars, 1 = skywars, 2 = duels
void setMode(int mode);

extern std::unordered_map<std::string, Hypixel::PlayerStats> g_playerStatsMap;
extern std::mutex g_statsMutex;

extern std::unordered_map<std::string, std::string> g_nickToRealMap;
extern std::mutex g_nickMapMutex;

extern std::unordered_map<std::string, std::string> g_playerTeamColor;

bool isInGame(const std::string &name);
bool shouldAlert(const std::string &name);
bool isInHypixelGame();
bool isInPreGameLobby();
bool shouldAutoFetchTags();
bool isChatOpen();
int getGameMode();
void clearAllCaches();

extern float g_jniLatency;
extern float g_apiLatency;
extern float g_scanSpeed;

extern std::string g_tabFooterText;
extern std::mutex g_footerMutex;

extern std::unordered_map<std::string, Hypixel::PlayerStats> g_pendingStatsMap;
extern std::mutex g_pendingStatsMutex;

int getProcessedCount();
int getActiveFetchCount();
int getPendingStatsCount();
float getApiLatency();
float getScanSpeed();

} // namespace OVson
