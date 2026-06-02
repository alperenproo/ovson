#include "injector.h"
#include "process_scan.h"
#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

static std::once_flag g_dllOnce;
static std::vector<uint8_t> g_dllBytes;

static std::atomic<int> g_activeSlot{0};
static std::mutex       g_customPathMtx;
static std::wstring     g_customPath;
static std::once_flag   g_betaOnce;
static std::vector<uint8_t> g_betaBytes;

static const std::vector<uint8_t> &loadEmbeddedResource(int resId,
                                                       std::vector<uint8_t> &dst) {
  HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(resId),
                            MAKEINTRESOURCEW(10));
  if (!res) return dst;
  HGLOBAL h = LoadResource(nullptr, res);
  if (!h) return dst;
  DWORD sz = SizeofResource(nullptr, res);
  void *data = LockResource(h);
  if (!data || sz == 0) return dst;
  dst.assign((const uint8_t *)data, (const uint8_t *)data + sz);
  return dst;
}

const std::vector<uint8_t> &embeddedDllBytes() {
  std::call_once(g_dllOnce, []() { loadEmbeddedResource(1, g_dllBytes); });
  return g_dllBytes;
}

static const std::vector<uint8_t> &betaDllBytes() {
  std::call_once(g_betaOnce, []() { loadEmbeddedResource(2, g_betaBytes); });
  return g_betaBytes;
}

void setActiveSlot(int slot, const std::wstring &customPath) {
  if (slot < 0 || slot > 2) slot = 0;
  g_activeSlot.store(slot, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lk(g_customPathMtx);
  g_customPath = customPath;
}

static std::vector<uint8_t> readFileBytes(const std::wstring &path) {
  std::vector<uint8_t> out;
  if (path.empty()) return out;
  FILE *f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return out;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz > 0 && sz < (64 * 1024 * 1024)) {
    out.resize((size_t)sz);
    size_t rd = fread(out.data(), 1, (size_t)sz, f);
    if (rd != (size_t)sz) out.clear();
  }
  fclose(f);
  return out;
}

std::vector<uint8_t> activeDllBytes() {
  int slot = g_activeSlot.load(std::memory_order_relaxed);
  if (slot == 1) {
    const auto &beta = betaDllBytes();
    if (!beta.empty()) return beta;
    return embeddedDllBytes();
  }
  if (slot == 2) {
    std::wstring path;
    {
      std::lock_guard<std::mutex> lk(g_customPathMtx);
      path = g_customPath;
    }
    auto bytes = readFileBytes(path);
    if (!bytes.empty()) return bytes;
    return {};
  }
  return embeddedDllBytes();
}

bool embeddedDllHasUninjectHandler() {
  const auto &bytes = embeddedDllBytes();
  if (bytes.empty()) return false;
  static const uint8_t kPattern[] = {
    'O',0,'V',0,'s',0,'o',0,'n',0,
    'U',0,'n',0,'i',0,'n',0,'j',0,'e',0,'c',0,'t',0,
  };
  const size_t plen = sizeof(kPattern);
  if (bytes.size() < plen) return false;
  for (size_t i = 0; i + plen <= bytes.size(); ++i) {
    if (memcmp(bytes.data() + i, kPattern, plen) == 0)
      return true;
  }
  return false;
}

static void sweepStaleTempDlls() {
  wchar_t tempDir[MAX_PATH];
  if (!GetTempPathW(MAX_PATH, tempDir)) return;
  std::wstring base = tempDir;
  const wchar_t *patterns[] = {
      L"OVson_*.dll",
      L"MinHook_*.dll",
      L"ovson_slint_cpp.dll"
  };
  for (const wchar_t *pat : patterns) {
    std::wstring full_pat = base + pat;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(full_pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    do {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      std::wstring full = base + fd.cFileName;
      DeleteFileW(full.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
}

static std::wstring writeTempDll(DWORD pid) {
  std::vector<uint8_t> bytes = activeDllBytes();
  if (bytes.empty())
    return L"";
  wchar_t tempDir[MAX_PATH];
  if (!GetTempPathW(MAX_PATH, tempDir))
    return L"";
  std::wstring path = std::wstring(tempDir) + L"OVson_" +
                      std::to_wstring(pid) + L"_" +
                      std::to_wstring(GetTickCount64()) + L".dll";
  FILE *f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f)
    return L"";
  size_t wr = fwrite(bytes.data(), 1, bytes.size(), f);
  fclose(f);
  if (wr != bytes.size()) {
    DeleteFileW(path.c_str());
    return L"";
  }
  return path;
}

static bool loadLibraryInject(DWORD pid, const wchar_t *dllPath) {
  DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
  HANDLE proc = OpenProcess(access, FALSE, pid);
  if (!proc)
    return false;

  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  FARPROC loadLib = k32 ? GetProcAddress(k32, "LoadLibraryW") : nullptr;
  if (!loadLib) {
    CloseHandle(proc);
    return false;
  }

  SIZE_T bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
  LPVOID remotePath =
      VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT, PAGE_READWRITE);
  if (!remotePath) {
    CloseHandle(proc);
    return false;
  }

  SIZE_T written = 0;
  if (!WriteProcessMemory(proc, remotePath, dllPath, bytes, &written) ||
      written != bytes) {
    VirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  HANDLE th = CreateRemoteThread(proc, nullptr, 0,
                                 (LPTHREAD_START_ROUTINE)loadLib,
                                 remotePath, 0, nullptr);
  if (!th) {
    VirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  WaitForSingleObject(th, 10000);
  DWORD exitCode = 0;
  GetExitCodeThread(th, &exitCode);
  CloseHandle(th);
  VirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
  CloseHandle(proc);
  return exitCode != 0;
}

bool injectPid(DWORD pid, const ProgressFn &cb) {
  auto step = [&](int p, const wchar_t *s) {
    if (cb)
      cb(p, std::wstring(s));
  };

  step(5, L"Preparing");

  sweepStaleTempDlls();

  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h)
    return false;
  CloseHandle(h);

  if (hasStaleOldDll(pid)) {
    step(100, L"Old DLL still loaded — restart Minecraft");
    return false;
  }

  if (isAlreadyInjected(pid)) {
    step(100, L"Already injected");
    return true;
  }

  step(15, L"Writing payload");
  std::wstring dllPath = writeTempDll(pid);
  if (dllPath.empty()) {
    step(100, L"Failed");
    return false;
  }

  wchar_t hintName[64];
  wsprintfW(hintName, L"Local\\OVsonLoaderHint_%lu", pid);
  HANDLE hint = CreateEventW(nullptr, TRUE, TRUE, hintName);
  if (hint) {
    SetEvent(hint);
    static std::mutex                   s_mtx;
    static std::map<DWORD, HANDLE>      g_hintEvents;
    std::lock_guard<std::mutex> lk(s_mtx);
    auto it = g_hintEvents.find(pid);
    if (it != g_hintEvents.end()) {
      CloseHandle(it->second);
      g_hintEvents.erase(it);
    }
    g_hintEvents[pid] = hint;
    std::thread([pid, &mtxRef = s_mtx, &mapRef = g_hintEvents]() {
      HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, pid);
      if (!proc) return;
      WaitForSingleObject(proc, INFINITE);
      CloseHandle(proc);
      std::lock_guard<std::mutex> lk(mtxRef);
      auto it = mapRef.find(pid);
      if (it != mapRef.end()) {
        CloseHandle(it->second);
        mapRef.erase(it);
      }
    }).detach();
  }

  step(40, L"Injecting");
  bool ok = loadLibraryInject(pid, dllPath.c_str());
  if (!ok) {
    DeleteFileW(dllPath.c_str());
    step(100, L"Failed");
    return false;
  }

  step(80, L"Waiting for init");
  const wchar_t *initPrefixes[] = { L"Local\\", L"", L"Global\\" };
  HANDLE evAlive = nullptr;
  for (int i = 0; i < 30 && !evAlive; i++) {
    for (const wchar_t *pfx : initPrefixes) {
      wchar_t evName[96];
      wsprintfW(evName, L"%sOVsonAlive_%lu", pfx, pid);
      evAlive = OpenEventW(SYNCHRONIZE, FALSE, evName);
      if (evAlive) break;
    }
    if (!evAlive) Sleep(100);
  }
  if (evAlive)
    CloseHandle(evAlive);

  if (!DeleteFileW(dllPath.c_str())) {
    MoveFileExW(dllPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  }
  step(100, L"Done");
  return true;
}

bool uninjectPid(DWORD pid, DWORD *lastError) {
  const wchar_t *prefixes[] = { L"Local\\", L"", L"Global\\" };
  HANDLE ev = nullptr;
  DWORD firstErr = 0;
  for (const wchar_t *pfx : prefixes) {
    wchar_t evName[96];
    wsprintfW(evName, L"%sOVsonUninject_%lu", pfx, pid);
    ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, evName);
    if (ev) break;
    if (!firstErr) firstErr = GetLastError();
  }
  if (!ev) {
    if (lastError) *lastError = firstErr;
    return false;
  }
  SetEvent(ev);
  CloseHandle(ev);
  if (lastError) *lastError = 0;

  wchar_t aliveName[64];
  wsprintfW(aliveName, L"Local\\OVsonAlive_%lu", pid);
  for (int i = 0; i < 10; ++i) {
    HANDLE alive = OpenEventW(SYNCHRONIZE, FALSE, aliveName);
    if (!alive)
      return true;
    CloseHandle(alive);
    Sleep(100);
  }
  return true;
}
