#include "FootstepDebug.h"
#include "../Utils/Logger.h"
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace FootstepDebug {

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

void logStateChange(bool enabled) {
    log("[STATE] MuteOwnSteps is now %s", enabled ? "ENABLED" : "DISABLED");
}

void logStepIntercepted(bool isLocalPlayer, bool muted) {
    log("[STEP-INTERCEPT] isLocalPlayer: %s | Muted: %s", 
        isLocalPlayer ? "YES" : "NO", muted ? "YES (Bypassed)" : "NO (Played)");
}

} // namespace FootstepDebug
