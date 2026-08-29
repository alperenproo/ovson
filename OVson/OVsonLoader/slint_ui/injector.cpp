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
static std::mutex g_hintMutex;
static std::map<DWORD, HANDLE> g_hintEvents;
static std::atomic<bool> g_stopping{false};

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

static HANDLE dynamicOpenProcess(DWORD access, BOOL inherit, DWORD pid) {
  typedef HANDLE(WINAPI* fnOpenProcess)(DWORD, BOOL, DWORD);
  char sOpenProcess[] = { 'O','p','e','n','P','r','o','c','e','s','s',0 };
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) return nullptr;
  auto pOpenProcess = (fnOpenProcess)GetProcAddress(k32, sOpenProcess);
  if (!pOpenProcess) return nullptr;
  return pOpenProcess(access, inherit, pid);
}

static bool loadLibraryInject(DWORD pid, const wchar_t *dllPath) {
  if (g_stopping.load(std::memory_order_acquire))
    return false;
  typedef LPVOID(WINAPI* fnVirtualAllocEx)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
  typedef BOOL(WINAPI* fnWriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
  typedef HANDLE(WINAPI* fnCreateRemoteThread)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
  typedef BOOL(WINAPI* fnVirtualFreeEx)(HANDLE, LPVOID, SIZE_T, DWORD);
  typedef BOOL(WINAPI* fnGetExitCodeThread)(HANDLE, LPDWORD);

  char sVirtualAllocEx[] = { 'V','i','r','t','u','a','l','A','l','l','o','c','E','x',0 };
  char sWriteProcessMemory[] = { 'W','r','i','t','e','P','r','o','c','e','s','s','M','e','m','o','r','y',0 };
  char sCreateRemoteThread[] = { 'C','r','e','a','t','e','R','e','m','o','t','e','T','h','r','e','a','d',0 };
  char sVirtualFreeEx[] = { 'V','i','r','t','u','a','l','F','r','e','e','E','x',0 };
  char sGetExitCodeThread[] = { 'G','e','t','E','x','i','t','C','o','d','e','T','h','r','e','a','d',0 };
  char sLoadLibraryW[] = { 'L','o','a','d','L','i','b','r','a','r','y','W',0 };

  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) return false;

  auto pVirtualAllocEx = (fnVirtualAllocEx)GetProcAddress(k32, sVirtualAllocEx);
  auto pWriteProcessMemory = (fnWriteProcessMemory)GetProcAddress(k32, sWriteProcessMemory);
  auto pCreateRemoteThread = (fnCreateRemoteThread)GetProcAddress(k32, sCreateRemoteThread);
  auto pVirtualFreeEx = (fnVirtualFreeEx)GetProcAddress(k32, sVirtualFreeEx);
  auto pGetExitCodeThread = (fnGetExitCodeThread)GetProcAddress(k32, sGetExitCodeThread);
  auto pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, sLoadLibraryW);

  if (!pVirtualAllocEx || !pWriteProcessMemory || !pCreateRemoteThread ||
      !pVirtualFreeEx || !pGetExitCodeThread || !pLoadLibraryW) {
    return false;
  }

  DWORD access = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
  HANDLE proc = dynamicOpenProcess(access, FALSE, pid);
  if (!proc)
    return false;

  SIZE_T bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
  LPVOID remotePath =
      pVirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT, PAGE_READWRITE);
  if (!remotePath) {
    CloseHandle(proc);
    return false;
  }

  SIZE_T written = 0;
  if (!pWriteProcessMemory(proc, remotePath, dllPath, bytes, &written) ||
      written != bytes) {
    pVirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  if (g_stopping.load(std::memory_order_acquire)) {
    pVirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  HANDLE th = pCreateRemoteThread(proc, nullptr, 0,
                                 pLoadLibraryW,
                                 remotePath, 0, nullptr);
  if (!th) {
    pVirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
    CloseHandle(proc);
    return false;
  }

  const DWORD waitResult = WaitForSingleObject(th, 10000);
  DWORD exitCode = 0;
  if (waitResult == WAIT_OBJECT_0)
    pGetExitCodeThread(th, &exitCode);
  CloseHandle(th);
  // If the remote thread timed out, its LoadLibrary call may still be reading
  // the path.  Leak that small target allocation rather than creating a
  // use-after-free in Minecraft.
  if (waitResult == WAIT_OBJECT_0)
    pVirtualFreeEx(proc, remotePath, 0, MEM_RELEASE);
  CloseHandle(proc);
  return waitResult == WAIT_OBJECT_0 && exitCode != 0;
}

bool injectPid(DWORD pid, const ProgressFn &cb) {
  if (g_stopping.load(std::memory_order_acquire))
    return false;
  loaderLog("injectPid started for target PID=%lu", pid);
  auto step = [&](int p, const wchar_t *s) {
    if (cb)
      cb(p, std::wstring(s));
  };

  step(5, L"Preparing");

  sweepStaleTempDlls();
  if (g_stopping.load(std::memory_order_acquire))
    return false;

  HANDLE h = dynamicOpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) {
    loaderLog("injectPid failed: cannot open process PID=%lu (err=%lu)", pid, GetLastError());
    return false;
  }
  CloseHandle(h);

  if (isAlreadyInjected(pid)) {
    loaderLog("injectPid: process PID=%lu is ALREADY injected. Skipping.", pid);
    step(100, L"Already injected");
    return true;
  }

  step(15, L"Writing payload");
  std::wstring dllPath = writeTempDll(pid);
  if (dllPath.empty()) {
    loaderLog("injectPid failed: writeTempDll returned empty path");
    step(100, L"Failed");
    return false;
  }
  if (g_stopping.load(std::memory_order_acquire)) {
    DeleteFileW(dllPath.c_str());
    return false;
  }

  wchar_t hintName[64];
  wsprintfW(hintName, L"Local\\OVsonLoaderHint_%lu", pid);
  HANDLE hint = CreateEventW(nullptr, TRUE, TRUE, hintName);
  if (hint) {
    SetEvent(hint);
    std::lock_guard<std::mutex> lk(g_hintMutex);
    auto it = g_hintEvents.find(pid);
    if (it != g_hintEvents.end()) {
      CloseHandle(it->second);
      g_hintEvents.erase(it);
    }
    g_hintEvents[pid] = hint;
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
  for (int i = 0; i < 30 && !evAlive &&
                  !g_stopping.load(std::memory_order_acquire); i++) {
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
  for (int i = 0; i < 10 &&
                  !g_stopping.load(std::memory_order_acquire); ++i) {
    HANDLE alive = OpenEventW(SYNCHRONIZE, FALSE, aliveName);
    if (!alive)
      return true;
    CloseHandle(alive);
    Sleep(100);
  }
  return true;
}

void requestInjectorShutdown() {
  g_stopping.store(true, std::memory_order_release);
}

void shutdownInjector() {
  requestInjectorShutdown();
  std::lock_guard<std::mutex> lock(g_hintMutex);
  for (const auto &entry : g_hintEvents) {
    const HANDLE hintHandle = entry.second;
    if (hintHandle) CloseHandle(hintHandle);
  }
  g_hintEvents.clear();
}
