#include "log_tail.h"
#include <ShlObj.h>
#include <chrono>

#pragma comment(lib, "Shell32.lib")

LogTail::LogTail() = default;
LogTail::~LogTail() { stop(); }

void LogTail::start(const std::wstring &path, LineFn onLine,
                    SessionFn onSession) {
    stop();
    fixedPath  = path;
    cb         = std::move(onLine);
    sessionCb  = std::move(onSession);
    stopFlag.store(false);
    worker = std::thread([this]() { run(); });
}

void LogTail::stop() {
    stopFlag.store(true);
    if (worker.joinable()) worker.join();
    lastSize = 0;
    pending.clear();
    {
        std::lock_guard<std::mutex> lk(pathMtx);
        activePath.clear();
    }
}

std::wstring LogTail::currentPath() const {
    std::lock_guard<std::mutex> lk(pathMtx);
    return activePath;
}

static DWORD pidFromLogName(const wchar_t *name) {
    if (_wcsnicmp(name, L"OVson_", 6) != 0) return 0;
    DWORD pid = 0;
    const wchar_t *p = name + 6;
    while (*p >= L'0' && *p <= L'9') {
        pid = pid * 10 + (*p - L'0');
        ++p;
    }
    if (_wcsicmp(p, L".log") != 0) return 0;
    return pid;
}

static bool isPidAlive(DWORD pid) {
    if (pid == 0) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

std::wstring LogTail::newestLog() {
    wchar_t appData[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                 SHGFP_TYPE_CURRENT, appData))) {
        return L"";
    }
    std::wstring dir = appData;
    dir += L"\\OVson\\logs";
    std::wstring pat = dir + L"\\OVson_*.log";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";

    std::wstring aliveBest;
    FILETIME     aliveBestTime{};
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        DWORD pid = pidFromLogName(fd.cFileName);
        if (pid == 0 || !isPidAlive(pid)) continue;
        if (aliveBest.empty() ||
            CompareFileTime(&fd.ftCreationTime, &aliveBestTime) > 0) {
            aliveBest     = full;
            aliveBestTime = fd.ftCreationTime;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    return aliveBest;
}

void LogTail::run() {
    while (!stopFlag.load()) {
        std::wstring path = fixedPath;
        if (path.empty()) path = newestLog();

        bool sessionChanged = false;
        {
            std::lock_guard<std::mutex> lk(pathMtx);
            if (path != activePath) {
                activePath = path;
                lastSize   = 0;
                pending.clear();
                sessionChanged = true;
            }
        }
        if (sessionChanged && sessionCb) {
            sessionCb(path);
        }

        if (!path.empty()) {
            HANDLE f = CreateFileW(path.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE
                                       | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz{};
                if (GetFileSizeEx(f, &sz)) {
                    uint64_t size = (uint64_t)sz.QuadPart;
                    if (size < lastSize) {
                        lastSize = 0;
                        pending.clear();
                    }
                    if (size > lastSize) {
                        LARGE_INTEGER off{};
                        off.QuadPart = (LONGLONG)lastSize;
                        SetFilePointerEx(f, off, nullptr, FILE_BEGIN);
                        uint64_t toRead = size - lastSize;
                        if (toRead > (1 << 20)) toRead = (1 << 20);
                        std::string buf(toRead, '\0');
                        DWORD got = 0;
                        if (ReadFile(f, buf.data(), (DWORD)toRead,
                                     &got, nullptr) &&
                            got > 0) {
                            buf.resize(got);
                            pending += buf;
                            lastSize += got;
                            size_t start = 0;
                            for (size_t i = 0; i < pending.size(); ++i) {
                                if (pending[i] == '\n') {
                                    std::string line =
                                        pending.substr(start, i - start);
                                    if (!line.empty() &&
                                        line.back() == '\r') {
                                        line.pop_back();
                                    }
                                    if (cb && !line.empty()) cb(line);
                                    start = i + 1;
                                }
                            }
                            pending.erase(0, start);
                        }
                    }
                }
                CloseHandle(f);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}
