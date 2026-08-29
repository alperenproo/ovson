#include "shutdown_coordinator.h"

bool ShutdownCoordinator::request() {
  Stage expected = Stage::Running;
  return m_stage.compare_exchange_strong(expected, Stage::Requested,
                                         std::memory_order_acq_rel);
}

bool ShutdownCoordinator::stopping() const {
  return m_stage.load(std::memory_order_acquire) != Stage::Running;
}

ShutdownCoordinator::Stage ShutdownCoordinator::stage() const {
  return m_stage.load(std::memory_order_acquire);
}

bool ShutdownCoordinator::advance(Stage next) {
  Stage current = m_stage.load(std::memory_order_acquire);
  while (static_cast<unsigned>(next) > static_cast<unsigned>(current)) {
    if (m_stage.compare_exchange_weak(current, next,
                                      std::memory_order_acq_rel))
      return true;
  }
  return false;
}

void ShutdownCoordinator::resetForTests() {
  m_stage.store(Stage::Running, std::memory_order_release);
}

const char *shutdownStageName(ShutdownCoordinator::Stage stage) {
  switch (stage) {
  case ShutdownCoordinator::Stage::Running: return "running";
  case ShutdownCoordinator::Stage::Requested: return "requested";
  case ShutdownCoordinator::Stage::EventLoopExited: return "event-loop-exited";
  case ShutdownCoordinator::Stage::ScanStopped: return "scan-stopped";
  case ShutdownCoordinator::Stage::WorkersStopped: return "workers-stopped";
  case ShutdownCoordinator::Stage::TrayRemoved: return "tray-removed";
  case ShutdownCoordinator::Stage::WatcherStopped: return "watcher-stopped";
  case ShutdownCoordinator::Stage::HandlesClosed: return "handles-closed";
  case ShutdownCoordinator::Stage::Complete: return "complete";
  default: return "unknown";
  }
}
