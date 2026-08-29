#pragma once

#include <atomic>
#include <cstdint>

// Pure, platform-independent shutdown state used by the loader and its unit
// tests.  Requesting shutdown is idempotent and stages can only advance.
class ShutdownCoordinator {
public:
  enum class Stage : std::uint8_t {
    Running,
    Requested,
    EventLoopExited,
    ScanStopped,
    WorkersStopped,
    TrayRemoved,
    WatcherStopped,
    HandlesClosed,
    Complete
  };

  bool request();
  bool stopping() const;
  Stage stage() const;
  bool advance(Stage next);
  void resetForTests();

private:
  std::atomic<Stage> m_stage{Stage::Running};
};

const char *shutdownStageName(ShutdownCoordinator::Stage stage);
