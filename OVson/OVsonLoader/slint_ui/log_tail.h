#pragma once
#include <Windows.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class LogTail {
public:
    using LineFn    = std::function<void(const std::string &line)>;
    using SessionFn = std::function<void(const std::wstring &newPath)>;

    LogTail();
    ~LogTail();

    void start(const std::wstring &path, LineFn onLine,
               SessionFn onSession = {});
    void stop();

    std::wstring currentPath() const;

private:
    void run();
    static std::wstring newestLog();

    std::thread             worker;
    std::atomic<bool>       stopFlag{false};
    std::wstring            fixedPath;     // user-pinned path, may be empty
    mutable std::mutex      pathMtx;
    std::wstring            activePath;    // path being tailed right now
    LineFn                  cb;
    SessionFn               sessionCb;
    uint64_t                lastSize = 0;
    std::string             pending;       // bytes read but not yet \n
};
