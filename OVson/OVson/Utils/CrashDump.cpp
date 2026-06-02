#include "CrashDump.h"
#include "Logger.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <ShlObj.h>
#include <atomic>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "Shell32.lib")

namespace CrashDump {

static std::atomic<bool> s_alreadyWritten{false};

bool writeOnce(const char *tag) {
  if (s_alreadyWritten.exchange(true, std::memory_order_acq_rel))
    return false;

  if (!tag || !*tag) tag = "crash";

  wchar_t path[MAX_PATH] = L"";
  bool usedAppData = false;
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                  SHGFP_TYPE_CURRENT, path))) {
    wcscat_s(path, L"\\OVson");
    CreateDirectoryW(path, nullptr);
    wcscat_s(path, L"\\dumps");
    CreateDirectoryW(path, nullptr);
    wcscat_s(path, L"\\");
    usedAppData = true;
  } else {
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
      return false;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = 0;
  }

  wchar_t suffix[96];
  int len = MultiByteToWideChar(CP_UTF8, 0, tag, -1, nullptr, 0);
  if (len <= 0 || len > 32) return false;
  wchar_t tagW[40];
  MultiByteToWideChar(CP_UTF8, 0, tag, -1, tagW, 40);
  if (usedAppData) {
    swprintf_s(suffix, L"OVson_%ls_%lu_%llu.dmp", tagW,
               (unsigned long)GetCurrentProcessId(),
               (unsigned long long)GetTickCount64());
  } else {
    swprintf_s(suffix, L"OVson_%ls.dmp", tagW);
  }
  wcscat_s(path, suffix);

  HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    Logger::error("[CrashDump] CreateFile failed for %ls (err=%lu)",
                  path, GetLastError());
    s_alreadyWritten.store(false); // free the slot so a retry is allowed
    return false;
  }

  MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithThreadInfo |
                                        MiniDumpWithIndirectlyReferencedMemory |
                                        MiniDumpWithDataSegs);
  BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                               hFile, type, nullptr, nullptr, nullptr);
  CloseHandle(hFile);

  if (ok) {
    Logger::error("[CrashDump] Wrote dump %ls — send this file for "
                  "post-mortem analysis.", path);
  } else {
    Logger::error("[CrashDump] MiniDumpWriteDump failed (err=%lu)",
                  GetLastError());
    s_alreadyWritten.store(false);
    return false;
  }
  return true;
}

} // namespace CrashDump
