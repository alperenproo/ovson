#include "HitboxDebug.h"
#include "../Utils/Logger.h"
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace HitboxDebug {

static std::mutex s_logMutex;
static std::string s_logFilePath;
static bool s_inited = false;

static std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream ss;
    struct tm timeinfo;
    localtime_s(&timeinfo, &in_time_t);
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void init() {

}

const std::string& getLogFilePath() {
    if (!s_inited) init();
    return s_logFilePath;
}

void log(const char* fmt, ...) {

}

void logTransform(const std::string& className, bool success) {
    log("[TRANSFORM] Class '%s': %s", className.c_str(), success ? "SUCCESS" : "FAILED/SKIPPED");
}

void logEntityCheck(const std::string& entityClass, bool isPlayer, bool shouldRender) {
    log("[ENTITY-FILTER] Class '%s' -> isPlayer: %s | shouldRender: %s", 
        entityClass.c_str(), isPlayer ? "YES" : "NO", shouldRender ? "YES" : "NO");
}

static std::unordered_map<std::string, uint32_t> s_loggedPlayerColors;
static std::mutex s_loggedColorsMutex;

void logColorResolve(const std::string& playerName, uint32_t color, const char* source) {
    if (playerName.empty()) return;
    {
        std::lock_guard<std::mutex> lock(s_loggedColorsMutex);
        auto it = s_loggedPlayerColors.find(playerName);
        if (it != s_loggedPlayerColors.end() && it->second == color) {
            return;
        }
        s_loggedPlayerColors[playerName] = color;
    }
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    log("[COLOR] Player '%s' -> RGB(0x%06X, R:%u, G:%u, B:%u) via %s",
        playerName.c_str(), color & 0xFFFFFF, r, g, b, source ? source : "Unknown");
}

void logBoxMatch(double w, double h, double d, double worldX, double worldY, double worldZ, int playerCount, const char* bestPlayer, double bestDist, uint32_t color, const char* source) {
    if (color == (uint32_t)-1 || color == 0xFFFFFF) {
        log("[BOX-MATCH] bb=%.2fx%.2fx%.2f at(%.1f,%.1f,%.1f) players=%d bestPlayer='%s' dist=%.2f -> UNRESOLVED/WHITE",
            w, h, d, worldX, worldY, worldZ, playerCount, bestPlayer ? bestPlayer : "None", bestDist);
    } else {
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        log("[BOX-MATCH] bb=%.2fx%.2fx%.2f at(%.1f,%.1f,%.1f) players=%d bestPlayer='%s' dist=%.2f -> COLOR(0x%06X, R:%u, G:%u, B:%u) via %s",
            w, h, d, worldX, worldY, worldZ, playerCount, bestPlayer ? bestPlayer : "None", bestDist, color & 0xFFFFFF, r, g, b, source ? source : "Unknown");
    }
}

void logResolveStep(const char* playerName, const char* stepName, const char* result) {
    log("[RESOLVE-STEP] Player '%s' -> %s: %s",
        playerName ? playerName : "null", stepName ? stepName : "unknown", result ? result : "none");
}

} // namespace HitboxDebug

