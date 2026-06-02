#pragma once
#include <string>
namespace Settings {

struct Values {
    bool        autoInject       = false;
    bool        startWithWindows = false;
    bool        minimizeToTray   = true;
    bool        autoCheckUpdates = true;
    int         dllSlot          = 0;
    std::wstring customDllPath;
};

Values load();

void save(const Values &v);

void applyStartWithWindows(bool enable);

} // namespace Settings
