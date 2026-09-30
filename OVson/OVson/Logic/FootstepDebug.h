#pragma once

#include <string>
#include <Windows.h>

namespace FootstepDebug {
    void init();
    void log(const char* fmt, ...);
    void logStateChange(bool enabled);
    void logStepIntercepted(bool isLocalPlayer, bool muted);
    const std::string& getLogFilePath();
}
