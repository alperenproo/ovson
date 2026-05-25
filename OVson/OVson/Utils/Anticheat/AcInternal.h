#pragma once
#include <Windows.h>
#include <jni.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Anticheat {

struct PlayerData {
  std::string name;
  int entityId = 0;
  bool isLocalPlayer = false;
  bool isFlagged = false;
  long currentTick = 0;

  double posX = 0, posY = 0, posZ = 0;
  double lastPosX = 0, lastPosY = 0, lastPosZ = 0;
  bool havePos = false;
  float rotYaw = 0, rotPitch = 0;
  bool onGround = false;
  bool isBlocking = false;
  bool isUsingItem = false;
  bool isSwingInProgress = false;
  bool isSneaking = false;
  bool isSprinting = false;
  bool isRiding = false;
  int heldItemId = 0;
  int hurtTime = 0;

  bool wasBlocking = false;
  bool wasUsingItem = false;
  bool wasSwingInProgress = false;
  bool wasSneaking = false;

  long lastBlockStartMs = 0;
  long lastSwingMs = 0;

  struct Sample {
    double x, y, z;
    ULONGLONG t;
  };
  std::vector<Sample> positions;
  void pushPosition(double x, double y, double z) {
    Sample s{x, y, z, GetTickCount64()};
    positions.push_back(s);
    if (positions.size() > 20)
      positions.erase(positions.begin());
  }

  struct Swing {
    long ms;
    bool wasBlockingBefore;
    int afterTrack;
  };
  std::vector<Swing> swings;
  long lastSwingDetectedMs = 0;

  long lastCrouchStartTick = 0;
  long lastCrouchEndTick = 0;
  long lastSwingTick = 0;
  long lastEaglePatternTick = 0;
  long lastOnGroundTick = 0;
  std::vector<int> crouchDurations;

  int useItemTime = 0;
  double autoBlockVL = 0;
  int autoBlockConsecutive = 0;

  int scaffoldConsecutive = 0;
  std::string lastScaffoldType;
  long lastScaffoldTime = 0;

  struct CheckState {
    double vl = 0;
    ULONGLONG lastAlertMs = 0;
    ULONGLONG lastFlagMs = 0;
  };
  std::unordered_map<std::string, CheckState> checks;
};

class Check {
public:
  Check(const char *name, const char *desc) : m_name(name), m_desc(desc) {}
  virtual ~Check() = default;
  virtual void onPlayerTick(PlayerData &p, JNIEnv *env, jobject entity) = 0;
  const char *name() const { return m_name; }
  const char *desc() const { return m_desc; }

private:
  const char *m_name;
  const char *m_desc;
};

namespace Checks {
Check *makeNoSlow();
Check *makeAutoBlock();
Check *makeEagle();
Check *makeScaffold();
} // namespace Checks

void flag(PlayerData &p, Check *check, const std::string &info, double vl);
void tickFromRenderThread();
void debugLog(const char *fmt, ...);
void abLog(const char *fmt, ...);
void mapLog(const char *fmt, ...);

} // namespace Anticheat
