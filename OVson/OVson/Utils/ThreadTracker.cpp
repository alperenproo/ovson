#include "ThreadTracker.h"

namespace ThreadTracker {
    std::atomic<int>  g_activeThreads{0};
    std::atomic<bool> g_shouldStop{false};
}
