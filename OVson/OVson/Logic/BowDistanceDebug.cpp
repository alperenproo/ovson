#include "BowDistanceDebug.h"
#include "../Utils/Logger.h"
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace BowDistanceDebug {

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

    FILE* f = nullptr;
    fopen_s(&f, s_logFilePath.c_str(), "a");
    if (f) {
        std::string ts = getTimestamp();
        fprintf(f, "\n=== [BowDistanceDebug Session Started at %s] ===\n", ts.c_str());
        fclose(f);
    }
    s_inited = true;
}

const std::string& getLogFilePath() {
    if (!s_inited) init();
    return s_logFilePath;
}

void log(const char* fmt, ...) {

}

void logRelease(double x, double y, double z, ULONGLONG time) {
    log("[RELEASE] Bow shot recorded at (%.2f, %.2f, %.2f) at tick %llu", x, y, z, time);
}

void logChat(const std::string& unformatted, const std::string& rawJson, bool matched, const std::string& targetName, double dist) {
    if (matched) {
        log("[CHAT-MATCH] Target: '%s' | Calculated Distance: %.1f blocks | Text: '%s'", targetName.c_str(), dist, unformatted.c_str());
    } else {
        log("[CHAT-SKIP] No match or expired. Text: '%s' | RawJson len: %zu", unformatted.c_str(), rawJson.length());
    }
}

} // namespace BowDistanceDebug
