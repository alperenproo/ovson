#pragma once
#include <Windows.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const std::vector<uint8_t> &embeddedDllBytes();
bool embeddedDllHasUninjectHandler();

void setActiveSlot(int slot, const std::wstring &customPath);

std::vector<uint8_t> activeDllBytes();

using ProgressFn = std::function<void(int pct, const std::wstring &stage)>;
bool injectPid(DWORD pid, const ProgressFn &cb);
bool uninjectPid(DWORD pid, DWORD *lastError = nullptr);
