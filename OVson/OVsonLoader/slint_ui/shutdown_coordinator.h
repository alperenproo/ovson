#pragma once

#include <atomic>
#include <cstdint>

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
