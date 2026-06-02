#include "settings.h"
#include <Windows.h>

namespace Settings {
namespace {

constexpr const wchar_t *kRoot = L"Software\\OVson\\Loader";
constexpr const wchar_t *kRunKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t *kRunValue = L"OVsonLoader";

DWORD regGetDword(HKEY key, const wchar_t *name, DWORD fallback) {
    DWORD value = fallback;
    DWORD size  = sizeof(value);
    DWORD type  = 0;
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE *>(&value), &size) !=
            ERROR_SUCCESS ||
        type != REG_DWORD) {
        return fallback;
    }
    return value;
}

std::wstring regGetString(HKEY key, const wchar_t *name) {
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) !=
            ERROR_SUCCESS ||
        type != REG_SZ || size == 0) {
        return L"";
    }
    std::wstring out(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE *>(out.data()), &size) !=
        ERROR_SUCCESS) {
        return L"";
    }
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

void regSetDword(HKEY key, const wchar_t *name, DWORD v) {
    RegSetValueExW(key, name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&v), sizeof(v));
}

void regSetString(HKEY key, const wchar_t *name, const std::wstring &v) {
    DWORD bytes = (DWORD)((v.size() + 1) * sizeof(wchar_t));
    RegSetValueExW(key, name, 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(v.c_str()), bytes);
}

} // namespace

Values load() {
    Values v;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRoot, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return v;
    }
    v.autoInject       = regGetDword(key, L"AutoInject",       0) != 0;
    v.startWithWindows = regGetDword(key, L"StartWithWindows", 0) != 0;
    v.minimizeToTray   = regGetDword(key, L"MinimizeToTray",   1) != 0;
    v.autoCheckUpdates = regGetDword(key, L"AutoCheckUpdates", 1) != 0;
    v.dllSlot          = (int)regGetDword(key, L"DllSlot", 0);
    if (v.dllSlot < 0 || v.dllSlot > 2) v.dllSlot = 0;
    v.customDllPath    = regGetString(key, L"CustomDllPath");
    RegCloseKey(key);
    return v;
}

void save(const Values &v) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRoot, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                        &key, nullptr) != ERROR_SUCCESS) {
        return;
    }
    regSetDword(key, L"AutoInject",       v.autoInject       ? 1 : 0);
    regSetDword(key, L"StartWithWindows", v.startWithWindows ? 1 : 0);
    regSetDword(key, L"MinimizeToTray",   v.minimizeToTray   ? 1 : 0);
    regSetDword(key, L"AutoCheckUpdates", v.autoCheckUpdates ? 1 : 0);
    regSetDword(key, L"DllSlot",          (DWORD)v.dllSlot);
    regSetString(key, L"CustomDllPath",   v.customDllPath);
    RegCloseKey(key);

    applyStartWithWindows(v.startWithWindows);
}

void applyStartWithWindows(bool enable) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE,
                      &key) != ERROR_SUCCESS) {
        return;
    }
    if (enable) {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
            std::wstring quoted = L"\"";
            quoted += exePath;
            quoted += L"\"";
            regSetString(key, kRunValue, quoted);
        }
    } else {
        RegDeleteValueW(key, kRunValue);
    }
    RegCloseKey(key);
}

} // namespace Settings
