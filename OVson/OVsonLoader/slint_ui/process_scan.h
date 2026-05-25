#pragma once
#include <Windows.h>
#include <string>
#include <vector>

struct MinecraftProcess {
  DWORD pid = 0;
  std::wstring windowTitle;
  std::wstring exeName;
  bool injected = false;
};

std::vector<MinecraftProcess> findMinecraftProcesses();
bool isAlreadyInjected(DWORD pid);
bool hasStaleOldDll(DWORD pid);
