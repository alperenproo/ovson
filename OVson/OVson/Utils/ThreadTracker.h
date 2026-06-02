#pragma once
#include <atomic>
#include <windows.h>

namespace ThreadTracker {
    extern std::atomic<int> g_activeThreads;

    extern std::atomic<bool> g_shouldStop;

    inline bool shouldStop() {
        return g_shouldStop.load(std::memory_order_acquire);
    }
    inline void requestStop() {
        g_shouldStop.store(true, std::memory_order_release);
    }
    inline void resetStop() {
        g_shouldStop.store(false, std::memory_order_release);
    }

    inline void increment() { g_activeThreads++; }
    inline void decrement() { g_activeThreads--; }

    inline void waitForAll() {
        int retries = 0;
        while (g_activeThreads > 0 && retries < 500) {
            Sleep(10);
            retries++;
        }
    }
}
