#pragma once

#include <string>
#include <cstdint>
#include <Windows.h>

namespace HitboxDebug {
    void init();
    void log(const char* fmt, ...);
    void logTransform(const std::string& className, bool success);
    void logEntityCheck(const std::string& entityClass, bool isPlayer, bool shouldRender);
    void logColorResolve(const std::string& playerName, uint32_t color, const char* source);
    void logBoxMatch(double w, double h, double d, double worldX, double worldY, double worldZ, int playerCount, const char* bestPlayer, double bestDist, uint32_t color, const char* source);
    void logResolveStep(const char* playerName, const char* stepName, const char* result);
    const std::string& getLogFilePath();
}
