#pragma once
#include "Logger.h"
#include <Windows.h>
#include <eh.h>
#include <exception>
#include <stdexcept>

namespace SafeGuard {

class SehException : public std::runtime_error {
public:
  unsigned int code;
  SehException(unsigned int c)
      : std::runtime_error("native exception"), code(c) {}
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
  _set_se_translator([](unsigned int code, EXCEPTION_POINTERS *) {
    throw SehException(code);
  });
}

template <typename Fn> inline void run(const char *site, Fn &&body) {
  static LogThrottle throttle;
  try {
    body();
  } catch (const SehException &e) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s native crash %s (0x%08X)", site,
                    exceptionName(e.code), e.code);
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
  try {
    return body();
  } catch (const SehException &e) {
    if (throttle.shouldLog())
      Logger::error("[SafeGuard] %s native crash %s (0x%08X)", site,
                    exceptionName(e.code), e.code);
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
