#pragma once
namespace Watchdog {
void start();
void stop();
void tickFrame();
void enterSubsystem(const char *label);
void leaveSubsystem();
class SubsystemScope {
public:
  explicit SubsystemScope(const char *label) { enterSubsystem(label); }
  ~SubsystemScope() { leaveSubsystem(); }
  SubsystemScope(const SubsystemScope &) = delete;
  SubsystemScope &operator=(const SubsystemScope &) = delete;
};

} // namespace Watchdog
