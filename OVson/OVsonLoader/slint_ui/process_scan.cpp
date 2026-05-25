#include "process_scan.h"
#include <TlHelp32.h>
#include <algorithm>
#include <cwctype>

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

static bool ovsonModuleLoaded(DWORD pid) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
  if (snap == INVALID_HANDLE_VALUE)
    return false;
  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  bool found = false;
  if (Module32FirstW(snap, &me)) {
    do {
      if (_wcsnicmp(me.szModule, L"OVson", 5) == 0) {
        found = true;
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return found;
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
  HANDLE un = openOVsonEvent(SYNCHRONIZE, L"OVsonUninject", pid);
  if (!un) return false;
  CloseHandle(un);
  return true;
}

bool isAlreadyInjected(DWORD pid) {
  return hasLocalEvents(pid);
}

bool hasStaleOldDll(DWORD pid) {
  if (hasLocalEvents(pid)) return false;
  return ovsonModuleLoaded(pid);
}

std::vector<MinecraftProcess> findMinecraftProcesses() {
  std::vector<MinecraftProcess> out;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return out;
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
          out.push_back(std::move(mp));
        }
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  std::sort(out.begin(), out.end(),
            [](const MinecraftProcess &a, const MinecraftProcess &b) {
              return a.pid < b.pid;
            });
  return out;
}
