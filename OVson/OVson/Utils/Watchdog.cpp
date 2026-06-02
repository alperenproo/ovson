#include "Watchdog.h"
#include "CrashDump.h"
#include "Logger.h"
#include <Windows.h>
#include <atomic>
#include <thread>

namespace Watchdog {
namespace {

constexpr ULONGLONG kPollIntervalMs = 1000;  // how often we check
constexpr ULONGLONG kStallWarnMs    = 5000;  // log warning after 5s
constexpr ULONGLONG kStallDumpMs    = 8000;  // dump after 8s
constexpr ULONGLONG kReportCooldownMs = 30000;

std::atomic<ULONGLONG>     g_lastFrameTick{0};
std::atomic<const char *>  g_currentSubsystem{nullptr};
std::atomic<bool>          g_running{false};
std::atomic<ULONGLONG>     g_lastReportTick{0};

std::thread                g_thread;

void watchdogThread() {
  g_lastFrameTick.store(GetTickCount64(), std::memory_order_relaxed);

  while (g_running.load(std::memory_order_acquire)) {
    Sleep((DWORD)kPollIntervalMs);
    if (!g_running.load(std::memory_order_acquire)) break;

    ULONGLONG now      = GetTickCount64();
    ULONGLONG lastTick = g_lastFrameTick.load(std::memory_order_relaxed);
    ULONGLONG gap      = (now > lastTick) ? (now - lastTick) : 0;

    if (gap >= kStallWarnMs) {
      ULONGLONG lastReport = g_lastReportTick.load(std::memory_order_relaxed);
      if (now - lastReport >= kReportCooldownMs) {
        g_lastReportTick.store(now, std::memory_order_relaxed);
        const char *sub = g_currentSubsystem.load(std::memory_order_relaxed);
        Logger::error("[Watchdog] Render frame stalled for %llus "
                      "(last subsystem: %s)",
                      gap / 1000ULL, sub ? sub : "<none>");
      }

      if (gap >= kStallDumpMs) {
        CrashDump::writeOnce("hang");
      }
    }
  }
}

} // namespace

void start() {
  if (g_running.exchange(true))
    return; // already running
  g_lastFrameTick.store(GetTickCount64(), std::memory_order_relaxed);
  g_lastReportTick.store(0, std::memory_order_relaxed);
  g_thread = std::thread(watchdogThread);
}

void stop() {
  if (!g_running.exchange(false))
    return;
  if (g_thread.joinable()) g_thread.join();
}

void tickFrame() {
  g_lastFrameTick.store(GetTickCount64(), std::memory_order_relaxed);
}

void enterSubsystem(const char *label) {
  g_currentSubsystem.store(label, std::memory_order_relaxed);
}

void leaveSubsystem() {
  g_currentSubsystem.store(nullptr, std::memory_order_relaxed);
}

} // namespace Watchdog
