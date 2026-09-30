#include "process_scan.h"
#include <Psapi.h>
#include <TlHelp32.h>
#include <algorithm>
#include <cwctype>
#include <regex>

#pragma comment(lib, "Psapi.lib")

namespace {

struct WindowSearch {
  DWORD targetPid;
  HWND best;
};

static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lp) {
  auto *s = reinterpret_cast<WindowSearch *>(lp);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != s->targetPid)
    return TRUE;
  if (!IsWindowVisible(hwnd))
    return TRUE;
  if (GetWindow(hwnd, GW_OWNER) != nullptr)
    return TRUE;
  if (GetWindowTextLengthW(hwnd) <= 0)
    return TRUE;
  s->best = hwnd;
  return FALSE;
}

static std::wstring titleForPid(DWORD pid) {
  WindowSearch s{pid, nullptr};
  EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&s));
  if (!s.best)
    return L"";
  wchar_t buf[512];
  int n = GetWindowTextW(s.best, buf, 512);
  return n > 0 ? std::wstring(buf, n) : std::wstring();
}

static bool looksLikeMinecraft(const std::wstring &title) {
  if (title.empty())
    return false;
  std::wstring lo;
  lo.reserve(title.size());
  for (wchar_t c : title)
    lo.push_back((wchar_t)towlower(c));
  return lo.find(L"minecraft") != std::wstring::npos ||
         lo.find(L"lunar") != std::wstring::npos ||
         lo.find(L"badlion") != std::wstring::npos;
}

static bool ovsonModuleLoaded(DWORD /*pid*/) {
  return false;
}

static std::wstring exePathForPid(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return L"";
  wchar_t buf[MAX_PATH];
  DWORD sz = MAX_PATH;
  std::wstring out;
  if (QueryFullProcessImageNameW(h, 0, buf, &sz)) {
    out.assign(buf, sz);
  }
  CloseHandle(h);
  return out;
}

static uint64_t privateMemForPid(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return 0;
  PROCESS_MEMORY_COUNTERS_EX pmc{};
  pmc.cb = sizeof(pmc);
  uint64_t out = 0;
  if (GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc))) {
    out = pmc.PrivateUsage;
  }
  CloseHandle(h);
  return out;
}

static uint32_t uptimeForPid(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return 0;
  FILETIME create, exit, kernel, user, now;
  uint32_t secs = 0;
  if (GetProcessTimes(h, &create, &exit, &kernel, &user)) {
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER c, n;
    c.LowPart  = create.dwLowDateTime;  c.HighPart  = create.dwHighDateTime;
    n.LowPart  = now.dwLowDateTime;     n.HighPart  = now.dwHighDateTime;
    if (n.QuadPart > c.QuadPart) {
      secs = (uint32_t)((n.QuadPart - c.QuadPart) / 10000000ULL);
    }
  }
  CloseHandle(h);
  return secs;
}

static std::wstring versionFromTitle(const std::wstring &title) {
  if (title.empty()) return L"";
  for (size_t i = 0; i < title.size(); ++i) {
    if (iswdigit(title[i])) {
      size_t start = i;
      size_t dotCount = 0;
      size_t j = i;
      while (j < title.size() && (iswdigit(title[j]) || title[j] == L'.')) {
        if (title[j] == L'.') dotCount++;
        j++;
      }
      if (dotCount >= 1 && (j - start) >= 3) {
        while (j > start && !iswdigit(title[j - 1])) j--;
        if (j > start) return title.substr(start, j - start);
      }
      i = j;
    }
  }
  return L"";
}

static McLauncher detectLauncher(const std::wstring &exePath,
                                 const std::wstring &title) {
  auto contains = [](const std::wstring &hay, const wchar_t *needle) {
    if (hay.empty()) return false;
    std::wstring h; h.reserve(hay.size());
    for (wchar_t c : hay) h.push_back((wchar_t)towlower(c));
    std::wstring n;
    for (const wchar_t *p = needle; *p; ++p) n.push_back((wchar_t)towlower(*p));
    return h.find(n) != std::wstring::npos;
  };

  if (contains(exePath, L"lunarclient") || contains(title, L"lunar")) {
    return McLauncher::Lunar;
  }
  if (contains(exePath, L"badlion") || contains(title, L"badlion")) {
    return McLauncher::Badlion;
  }
  if (contains(exePath, L"polymc") || contains(exePath, L"prismlauncher") ||
      contains(exePath, L"multimc")) {
    return McLauncher::PolyMC;
  }
  if (contains(title, L"forge") || contains(exePath, L"forge")) {
    return McLauncher::Forge;
  }
  if (contains(title, L"optifine") || contains(exePath, L"optifine")) {
    return McLauncher::Optifine;
  }
  if (contains(title, L"minecraft") || contains(exePath, L".minecraft")) {
    return McLauncher::Vanilla;
  }
  return McLauncher::Unknown;
}

} // namespace

static HANDLE openOVsonEvent(DWORD access, const wchar_t *kind, DWORD pid) {
  wchar_t buf[96];
  const wchar_t *prefixes[] = { L"Local\\", L"", L"Global\\" };
  for (const wchar_t *pfx : prefixes) {
    wsprintfW(buf, L"%s%s_%lu", pfx, kind, pid);
    HANDLE h = OpenEventW(access, FALSE, buf);
    if (h) return h;
  }
  return nullptr;
}

static bool hasLocalEvents(DWORD pid) {
  HANDLE alive = openOVsonEvent(SYNCHRONIZE, L"OVsonAlive", pid);
  if (!alive) return false;
  CloseHandle(alive);
  return true;
}

bool isAlreadyInjected(DWORD pid) {
  if (hasLocalEvents(pid)) return true;
  return ovsonModuleLoaded(pid);
}

bool hasStaleOldDll(DWORD pid) {
  if (hasLocalEvents(pid)) return false;
  return ovsonModuleLoaded(pid);
}

static void safeScanCore(std::vector<MinecraftProcess> &out) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, L"javaw.exe") == 0 ||
          _wcsicmp(pe.szExeFile, L"java.exe") == 0) {
        std::wstring t = titleForPid(pe.th32ProcessID);
        if (looksLikeMinecraft(t)) {
          MinecraftProcess mp;
          mp.pid = pe.th32ProcessID;
          mp.windowTitle = t;
          mp.exeName = pe.szExeFile;
          mp.injected = isAlreadyInjected(pe.th32ProcessID);
          mp.exePath        = exePathForPid(pe.th32ProcessID);
          mp.privateMemBytes = privateMemForPid(pe.th32ProcessID);
          mp.uptimeSeconds  = uptimeForPid(pe.th32ProcessID);
          mp.mcVersion      = versionFromTitle(t);
          mp.launcher       = detectLauncher(mp.exePath, t);
          out.push_back(std::move(mp));
        }
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
}

std::vector<MinecraftProcess> findMinecraftProcesses() {
  std::vector<MinecraftProcess> out;
  try {
    safeScanCore(out);
    std::sort(out.begin(), out.end(),
              [](const MinecraftProcess &a, const MinecraftProcess &b) {
                return a.pid < b.pid;
              });
  } catch (...) {
  }
  return out;
}

const char *launcherLabel(McLauncher l) {
  switch (l) {
  case McLauncher::Vanilla:  return "Vanilla";
  case McLauncher::Lunar:    return "Lunar";
  case McLauncher::Badlion:  return "Badlion";
  case McLauncher::Forge:    return "Forge";
  case McLauncher::Optifine: return "Optifine";
  case McLauncher::PolyMC:   return "PolyMC";
  default:                   return "Unknown";
  }
}

uint32_t launcherColor(McLauncher l) {
  switch (l) {
  case McLauncher::Vanilla:  return 0xFF6FB85A;  // green
  case McLauncher::Lunar:    return 0xFF7C8CFF;  // periwinkle :3 blue
  case McLauncher::Badlion:  return 0xFFE74C3C;  // red-orange
  case McLauncher::Forge:    return 0xFFA67353;  // brown
  case McLauncher::Optifine: return 0xFFD08AC4;  // pink
  case McLauncher::PolyMC:   return 0xFF1ABC9C;  // teal
  default:                   return 0xFF888B92;  // grey
  }
}
