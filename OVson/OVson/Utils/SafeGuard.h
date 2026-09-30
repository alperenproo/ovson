#pragma once
#include "CrashDump.h"
#include "Logger.h"
#include <Windows.h>
#include <eh.h>
#include <exception>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace SafeGuard {

inline const void *imageBase() { return &__ImageBase; }

class SehException : public std::runtime_error {
public:
  unsigned int code;
  const void *address;
  int accessType;
  const void *accessAddress;
  SehException(unsigned int c, const void *at, int type, const void *operand)
      : std::runtime_error("native exception"), code(c), address(at),
        accessType(type), accessAddress(operand) {}
};

inline const char *exceptionName(unsigned int code) {
  switch (code) {
  case EXCEPTION_ACCESS_VIOLATION:
    return "ACCESS_VIOLATION";
  case EXCEPTION_STACK_OVERFLOW:
    return "STACK_OVERFLOW";
  case EXCEPTION_ILLEGAL_INSTRUCTION:
    return "ILLEGAL_INSTRUCTION";
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    return "INT_DIVIDE_BY_ZERO";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    return "FLT_DIVIDE_BY_ZERO";
  case EXCEPTION_PRIV_INSTRUCTION:
    return "PRIV_INSTRUCTION";
  case EXCEPTION_IN_PAGE_ERROR:
    return "IN_PAGE_ERROR";
  case EXCEPTION_DATATYPE_MISALIGNMENT:
    return "DATATYPE_MISALIGNMENT";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    return "ARRAY_BOUNDS";
  default:
    return "UNKNOWN";
  }
}

struct LogThrottle {
  ULONGLONG nextAllowedTick = 0;
  bool shouldLog() {
    ULONGLONG now = GetTickCount64();
    if (now < nextAllowedTick)
      return false;
    nextAllowedTick = now + 5000;
    return true;
  }
};

inline void installSehTranslator() {
  static bool announced = false;
  if (!announced) {
    announced = true;
    Logger::info("[OVson] image base=%p", imageBase());
  }
  _set_se_translator([](unsigned int code, EXCEPTION_POINTERS *info) {
    const void *at = nullptr;
    int type = -1;
    const void *operand = nullptr;
    if (info && info->ExceptionRecord) {
      const EXCEPTION_RECORD *record = info->ExceptionRecord;
      at = record->ExceptionAddress;
      if (record->NumberParameters >= 2) {
        type = static_cast<int>(record->ExceptionInformation[0]);
        operand = reinterpret_cast<const void *>(record->ExceptionInformation[1]);
      }
    }
    throw SehException(code, at, type, operand);
  });
}

inline void describeAddress(const void *address, char *out, size_t size) {
  HMODULE module = nullptr;
  char path[MAX_PATH] = {0};
  if (address != nullptr &&
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(address), &module) &&
      module != nullptr &&
      GetModuleFileNameA(module, path, MAX_PATH) > 0) {
    const char *name = strrchr(path, '\\');
    name = (name != nullptr) ? name + 1 : path;
    const auto offset = reinterpret_cast<uintptr_t>(address) -
                        reinterpret_cast<uintptr_t>(module);
    _snprintf_s(out, size, _TRUNCATE, "%s+0x%llX", name,
                static_cast<unsigned long long>(offset));
    return;
  }
  _snprintf_s(out, size, _TRUNCATE, "%p (no module)", address);
}

inline void logFault(const char *site, const SehException &e) {
  char where[MAX_PATH + 32] = {0};
  describeAddress(e.address, where, sizeof(where));

  if (e.code == EXCEPTION_ACCESS_VIOLATION && e.accessType >= 0) {
    const char *how = e.accessType == 1   ? "writing"
                      : e.accessType == 8 ? "executing"
                                          : "reading";
    Logger::error("[SafeGuard] %s native crash %s (0x%08X) at %s -- %s %p "
                  "(OVson base=%p)",
                  site, exceptionName(e.code), e.code, where, how,
                  e.accessAddress, imageBase());
  } else {
    Logger::error("[SafeGuard] %s native crash %s (0x%08X) at %s "
                  "(OVson base=%p)",
                  site, exceptionName(e.code), e.code, where, imageBase());
  }
}

template <typename Fn> inline void run(const char *site, Fn &&body) {
  static LogThrottle throttle;
  static int faults = 0;
  static bool retired = false;
  if (retired)
    return;
  try {
    body();
  } catch (const SehException &e) {
    if (throttle.shouldLog())
      logFault(site, e);
    CrashDump::writeOnce("seh");
    if (++faults >= 3) {
      retired = true;
      Logger::error("[SafeGuard] %s retired after %d native faults", site,
                    faults);
    }
  } catch (const std::exception &e) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s C++ exception: %s", site,
                    e.what() ? e.what() : "?");
  } catch (...) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s unknown exception", site);
  }
}

template <typename T, typename Fn>
inline T runOr(const char *site, T defaultValue, Fn &&body) {
  static LogThrottle throttle;
  static int faults = 0;
  static bool retired = false;
  if (retired)
    return defaultValue;
  try {
    return body();
  } catch (const SehException &e) {
    if (throttle.shouldLog())
      logFault(site, e);
    CrashDump::writeOnce("seh");
    if (++faults >= 3) {
      retired = true;
      Logger::error("[SafeGuard] %s retired after %d native faults", site,
                    faults);
    }
  } catch (const std::exception &e) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s C++ exception: %s", site,
                    e.what() ? e.what() : "?");
  } catch (...) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s unknown exception", site);
  }
  return defaultValue;
}

} // namespace SafeGuard
