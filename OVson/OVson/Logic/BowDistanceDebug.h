#pragma once

#include <string>
#include <Windows.h>

namespace BowDistanceDebug {
    void init();
    void log(const char* fmt, ...);
    void logRelease(double x, double y, double z, ULONGLONG time);
    void logChat(const std::string& unformatted, const std::string& rawJson, bool matched, const std::string& targetName, double dist);
    const std::string& getLogFilePath();
}
