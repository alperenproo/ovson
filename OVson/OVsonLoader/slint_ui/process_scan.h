#pragma once
#include <Windows.h>
#include <string>
#include <vector>
enum class McLauncher {
  Unknown,
  Vanilla,
  Lunar,
  Badlion,
  Forge,
  Optifine,
  PolyMC,
};

struct MinecraftProcess {
  DWORD pid = 0;
  std::wstring windowTitle;
  std::wstring exeName;
  bool injected = false;

  McLauncher  launcher = McLauncher::Unknown;
  std::wstring exePath;          // full path of the running .exe / javaw
  std::wstring mcVersion;        // "1.8.9", "1.20.4", "" if unknown
  uint64_t     privateMemBytes = 0;   // GetProcessMemoryInfo.PrivateUsage
  uint32_t     uptimeSeconds  = 0;    // since process creation
};

std::vector<MinecraftProcess> findMinecraftProcesses();
bool isAlreadyInjected(DWORD pid);
bool hasStaleOldDll(DWORD pid);

const char *launcherLabel(McLauncher l);

uint32_t launcherColor(McLauncher l);
