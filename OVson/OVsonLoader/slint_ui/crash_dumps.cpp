#include "crash_dumps.h"
#include <ShlObj.h>
#include <shellapi.h>
#include <algorithm>

#pragma comment(lib, "Shell32.lib")

namespace CrashDumps {
namespace {

std::wstring localAppData() {
  wchar_t buf[MAX_PATH] = L"";
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                               SHGFP_TYPE_CURRENT, buf))) {
    return L"";
  }
  return buf;
}

} // namespace

std::wstring dumpsDir() {
  std::wstring base = localAppData();
  if (base.empty()) return L"";
  base += L"\\OVson";
  CreateDirectoryW(base.c_str(), nullptr);
  base += L"\\dumps";
  CreateDirectoryW(base.c_str(), nullptr);
  return base;
}

std::vector<Entry> scan() {
  std::vector<Entry> out;
  std::wstring dir = dumpsDir();
  if (dir.empty()) return out;
  std::wstring pat = dir + L"\\OVson_*.dmp";
  WIN32_FIND_DATAW fd{};
  HANDLE h = FindFirstFileW(pat.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    Entry e;
    e.fileName  = fd.cFileName;
    e.path      = dir + L"\\" + e.fileName;
    e.sizeBytes = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    e.writeTime = fd.ftLastWriteTime;
    out.push_back(std::move(e));
  } while (FindNextFileW(h, &fd));
  FindClose(h);

  std::sort(out.begin(), out.end(),
            [](const Entry &a, const Entry &b) {
              return CompareFileTime(&a.writeTime, &b.writeTime) > 0;
            });
  return out;
}

int deleteAll() {
  int n = 0;
  for (const auto &e : scan()) {
    if (DeleteFileW(e.path.c_str())) ++n;
  }
  return n;
}

void openInExplorer(const std::wstring &entry) {
  std::wstring dir = dumpsDir();
  if (dir.empty()) return;
  if (entry.empty()) {
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
  } else {
    std::wstring arg = L"/select,\"" + entry + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(),
                  nullptr, SW_SHOWNORMAL);
  }
}

} // namespace CrashDumps
