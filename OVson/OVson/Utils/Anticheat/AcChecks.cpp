#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "../../Config/Config.h"
#include "../../Java.h"
#include "AcInternal.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace Anticheat {
namespace Checks {

static bool isSwordId(int id) {
  return id == 267 || id == 268 || id == 272 || id == 276 || id == 283;
}
static bool isPlaceableBlockId(int id) { return id > 0 && id < 256; }

static bool isFoodOrBowOrSword(int id) {
  if (isSwordId(id))
    return true;
  if (id == 261)
    return true;
  static const int kFoods[] = {260, 282, 297, 319, 320, 322, 349, 350, 357,
                               360, 363, 364, 365, 366, 367, 375, 391, 392,
                               393, 396, 400, 411, 412, 423, 424, 432};
  for (int f : kFoods)
    if (f == id)
      return true;
  return false;
}

static double wrapAngle180(double a) {
  while (a >= 180.0)
    a -= 360.0;
  while (a < -180.0)
    a += 360.0;
  return a;
}

static double relativeMoveYaw(double dx, double dz, float yaw) {
  double moveAngle = std::atan2(-dx, dz) * 180.0 / 3.14159265358979323846;
  return wrapAngle180(moveAngle - yaw);
}

class NoSlowCheck : public Check {
public:
  NoSlowCheck() : Check("NoSlow", "Sprint while blocking/eating/drawing.") {}
  void onPlayerTick(PlayerData &p, JNIEnv *, jobject) override {
    if (!Config::isAnticheatNoSlowEnabled())
      return;

    bool slowItem = false;
    if (p.isBlocking && isSwordId(p.heldItemId))
      slowItem = true;
    if (p.isUsingItem &&
        (p.heldItemId == 261 || isFoodOrBowOrSword(p.heldItemId)))
      slowItem = true;

    bool now = slowItem && p.isSprinting && !p.isRiding;
    if (now && !m_active) {
      m_active = true;
      m_startMs = (long)GetTickCount64();
    } else if (!now) {
      m_active = false;
      m_startMs = 0;
    }
    if (m_active) {
      long dur = (long)GetTickCount64() - m_startMs;
      if (dur > 200) {
        std::ostringstream ss;
        ss << "duration: " << dur << "ms";
        flag(p, this, ss.str(), 1.0);
        m_active = false;
        m_startMs = 0;
      }
    }
  }

private:
  bool m_active = false;
  long m_startMs = 0;
};

class AutoBlockCheck : public Check {
public:
  AutoBlockCheck()
      : Check("AutoBlock", "Attack while sword stays blocked") {}
  void onPlayerTick(PlayerData &p, JNIEnv *, jobject) override {
    if (!Config::isAnticheatAutoBlockEnabled())
      return;
    if (p.isLocalPlayer && !Config::isAnticheatCheckSelfEnabled())
      return;

    long currentMs = (long)GetTickCount64();

    if (p.isBlocking && !p.wasBlocking) {
      p.lastBlockStartMs = currentMs;
    }

    bool isHoldingSword = isSwordId(p.heldItemId);
    bool isSwinging = p.isSwingInProgress;

    if (isSwinging &&
        (p.lastSwingDetectedMs == 0 ||
         currentMs - p.lastSwingDetectedMs > 100)) {
      bool blockingAtSwing = p.isBlocking;
      PlayerData::Swing sw;
      sw.ms = currentMs;
      sw.wasBlockingBefore = blockingAtSwing;
      sw.afterTrack = -1;
      p.swings.push_back(sw);
      p.lastSwingDetectedMs = currentMs;
      if (p.swings.size() > 20)
        p.swings.erase(p.swings.begin());
      abLog("swing recorded: name='%s' local=%d itemId=%d isBlocking=%d "
            "isUsingItem=%d lastBlockAgeMs=%ld wasBlockingBefore=%d",
            p.name.c_str(), (int)p.isLocalPlayer, p.heldItemId,
            (int)p.isBlocking, (int)p.isUsingItem,
            p.lastBlockStartMs > 0 ? (long)(currentMs - p.lastBlockStartMs)
                                   : -1L,
            (int)blockingAtSwing);
    }

    for (auto &sw : p.swings) {
      if (sw.afterTrack == 1) continue;
      long since = currentMs - sw.ms;
      if (since >= 150 && since <= 200 && p.isBlocking) {
        sw.afterTrack = 1;
      } else if (sw.afterTrack == -1 && since > 200) {
        sw.afterTrack = 0;
      }
    }

    int autoBlockCount = 0;
    for (const auto &sw : p.swings) {
      if (currentMs - sw.ms >= 1000)
        continue;
      if (sw.afterTrack == -1)
        continue;
      if (!isHoldingSword)
        continue;
      if (sw.wasBlockingBefore && sw.afterTrack == 1)
        autoBlockCount++;
    }

    if (autoBlockCount > 0) {
      abLog("count tick: name='%s' autoBlockCount=%d swings_in_window=%d",
            p.name.c_str(), autoBlockCount, (int)p.swings.size());
    }
    if (autoBlockCount >= 2) {
      std::ostringstream ss;
      ss << "itemId:" << p.heldItemId << " autoblks:" << autoBlockCount;
      abLog("FLAG: name='%s' itemId=%d autoblks=%d", p.name.c_str(),
            p.heldItemId, autoBlockCount);
      flag(p, this, ss.str(), 5.0);
      p.swings.clear();
    }
  }
};

class EagleCheck : public Check {
public:
  EagleCheck() : Check("Eagle", "LegitScaffold (mechanical sneak-bridge)") {}
  void onPlayerTick(PlayerData &p, JNIEnv *, jobject) override {
    if (!Config::isAnticheatEagleEnabled())
      return;
    if (p.isLocalPlayer && !Config::isAnticheatCheckSelfEnabled())
      return;

    long tick = p.currentTick;
    bool released = p.wasSneaking && !p.isSneaking;
    bool started = !p.wasSneaking && p.isSneaking;

    /*
    if (p.isLocalPlayer && (tick % 40) == 0) {
      mapLog("EagleState [local]: tick=%ld isSneaking=%d wasSneaking=%d "
             "released=%d pitch=%.1f onGround=%d heldItem=%d "
             "isSwingInProgress=%d crouchHist=%d",
             tick, (int)p.isSneaking, (int)p.wasSneaking, (int)released,
             p.rotPitch, (int)p.onGround, p.heldItemId,
             (int)p.isSwingInProgress, (int)p.crouchDurations.size());
    }
    if (p.isLocalPlayer && started) {
      mapLog("EagleEdge [local]: sneak STARTED tick=%ld", tick);
    }
    if (p.isLocalPlayer && released) {
      mapLog("EagleEdge [local]: sneak RELEASED tick=%ld "
             "crouchStart=%ld crouchEnd=%ld dur=%ld swingTick=%ld",
             tick, p.lastCrouchStartTick, p.lastCrouchEndTick,
             p.lastCrouchEndTick - p.lastCrouchStartTick, p.lastSwingTick);
    }
    */
    if (!p.isLocalPlayer && (tick % 40) == 0) {
      mapLog("EagleState [%s]: tick=%ld isSneaking=%d wasSneaking=%d "
             "released=%d pitch=%.1f onGround=%d heldItem=%d "
             "isSwingInProgress=%d crouchHist=%d",
             p.name.c_str(), tick, (int)p.isSneaking, (int)p.wasSneaking,
             (int)released, p.rotPitch, (int)p.onGround, p.heldItemId,
             (int)p.isSwingInProgress, (int)p.crouchDurations.size());
    }
    if (!p.isLocalPlayer && started) {
      mapLog("EagleEdge [%s]: sneak STARTED tick=%ld", p.name.c_str(), tick);
    }
    if (!p.isLocalPlayer && released) {
      mapLog("EagleEdge [%s]: sneak RELEASED tick=%ld "
             "crouchStart=%ld crouchEnd=%ld dur=%ld swingTick=%ld",
             p.name.c_str(), tick, p.lastCrouchStartTick, p.lastCrouchEndTick,
             p.lastCrouchEndTick - p.lastCrouchStartTick, p.lastSwingTick);
    }

    if (released || tick % 100 == 0) {
        if (p.rotPitch > 60.0f) {
            debugLog("EagleStatus [%s]: sneak=%d rel=%d pitch=%.1f block=%d gnd=%d", 
                     p.name.c_str(), (int)p.isSneaking, (int)released, p.rotPitch, (int)isPlaceableBlockId(p.heldItemId), (int)p.onGround);
        }
    }


    if (!released)
      return;

    long end = p.lastCrouchEndTick;
    long start = p.lastCrouchStartTick;
    long swing = p.lastSwingTick;
    int crouchDur = (int)(end - start);

    bool quickCrouch = crouchDur >= 1 && crouchDur <= 2;
    bool swingTiming = (swing >= end - 1 && swing <= end + 1);
    
    int fastSamples = 0;
    std::string history = "";
    for (size_t i = 0; i < p.crouchDurations.size() && i < 5; i++) {
        int d = p.crouchDurations[i];
        if (d >= 1 && d <= 2) fastSamples++;
        history += std::to_string(d) + (i == p.crouchDurations.size() - 1 || i == 4 ? "" : ",");
    }
    bool consistent = (fastSamples >= 4);

    bool recentlyOnGround = p.onGround || (tick - p.lastOnGroundTick) <= 5;
    bool holdingBlock = isPlaceableBlockId(p.heldItemId);
    bool lookingDown = p.rotPitch >= 60.0f;


    if (quickCrouch) {
        debugLog("EagleDebug [%s]: dur=%d itemID=%d quick=%d swing=%d consist=%d(%d/5) pitch=%.1f block=%d gnd=%d(rec=%d) hist=[%s]",
                 p.name.c_str(), crouchDur, p.heldItemId, (int)quickCrouch, (int)swingTiming, (int)consistent, fastSamples, p.rotPitch, (int)holdingBlock, (int)p.onGround, (int)recentlyOnGround, history.c_str());
    }


    auto& state = p.checks[name()];


    if (state.vl > 0) state.vl -= 0.01;

    if (!lookingDown || !holdingBlock)
        return;

    if (quickCrouch && swingTiming && consistent) {

        state.vl += 1.0;
        
        if (state.vl >= 5.0) {
            long sinceLast = tick - p.lastEaglePatternTick;
            if (sinceLast >= 10) {
                std::ostringstream ss;
                ss << "Eagle Bridge (VL: " << (int)state.vl << ") | " << crouchDur << "t crouch | Samples: " << fastSamples << "/5";
                
                debugLog("[Eagle FLAG] %s | %s", p.name.c_str(), ss.str().c_str());
                flag(p, this, ss.str(), 5.0);
                p.lastEaglePatternTick = tick;
                state.vl = 0; // reset local counter after firing
            }
        }
    }

  }
};

class ScaffoldCheck : public Check {
public:
  ScaffoldCheck() : Check("Scaffold", "Illegal bridging speed/angle.") {}
  void onPlayerTick(PlayerData &p, JNIEnv *, jobject) override {
    if (!Config::isAnticheatScaffoldEnabled())
      return;
    if (p.isRiding)
      return;
    if (p.positions.size() < 5)
      return;

    auto &samples = p.positions;
    auto &cur = samples[samples.size() - 1];
    auto &p1 = samples[samples.size() - 2];
    auto &p2 = samples[samples.size() - 3];
    auto &p3 = samples[samples.size() - 4];
    auto &p4 = samples[samples.size() - 5];
    long tick = p.currentTick;

    double dx = (cur.x - p1.x) * 20.0;
    double dz = (cur.z - p1.z) * 20.0;
    double sxz2 = dx * dx + dz * dz;
    double sxz = std::sqrt(sxz2);
    double sy = (p1.y - p2.y) * 20.0;
    double accY = 50.0 * ((p1.y - p2.y) - (p3.y - p4.y));

    double moveAngle =
        std::atan2(dz, dx) * 180.0 / 3.14159265358979323846 - 90.0;
    double lookAngle = wrapAngle180(p.rotYaw);
    double diff = std::fmod(moveAngle - lookAngle + 360.0, 360.0);
    if (diff > 180.0)
      diff -= 360.0;

    bool eligible = p.isSwingInProgress && p.hurtTime == 0 &&
                    p.rotPitch > 50.0f && sxz2 > 9.0 && sxz2 < 100.0 &&
                    isPlaceableBlockId(p.heldItemId) &&
                    std::fabs(diff) > 165.0 && std::fabs(accY) >= 0.001;

    bool flagged = false;
    if (eligible) {
      double pitchF = std::clamp((p.rotPitch - 50.0) / 40.0, 0.0, 1.0);
      double angF = std::clamp((std::fabs(diff) - 165.0) / 15.0, 0.0, 1.0);
      double base = 0.0;
      const char *type = "";

      if (sy >= 4.0 && sy <= 15.0 && accY > -25.0) {
        type = "tower";
        double yF = std::clamp((sy - 4.0) / 11.0, 0.0, 1.0);
        double aF = std::clamp((accY + 25.0) / 25.0, 0.0, 1.0);
        double sev = pitchF * 0.3 + angF * 0.3 + yF * 0.3 + aF * 0.1;
        base = 3.0 + sev * 3.0;
      } else if (sy >= -1.0 && sy <= 4.0 && std::fabs(sy) > 0.005 &&
                 sxz2 > 25.0) {
        type = "horizontal";
        double hF = std::clamp((sxz - 5.0) / 5.0, 0.0, 1.0);
        double sev = pitchF * 0.3 + angF * 0.3 + hF * 0.4;
        base = 3.0 + sev * 3.0;
      }

      if (*type) {
        int consec = 0;
        if (p.lastScaffoldType == type && (tick - p.lastScaffoldTime) < 40)
          consec = p.scaffoldConsecutive + 1;
        p.scaffoldConsecutive = consec;
        p.lastScaffoldType = type;
        p.lastScaffoldTime = tick;

        double consecM = 1.0 + std::min(consec, 5) * 0.2;
        double finalVL = std::min(2.0, base * consecM * 0.25);

        std::ostringstream ss;
        ss << "type:" << type << " angle:" << std::fixed << std::setprecision(1)
           << diff << " spd:" << sxz << " accY:" << accY;
        flag(p, this, ss.str(), finalVL);
        flagged = true;
      }
    }

    if (!flagged && (tick - p.lastScaffoldTime) > 60)
      p.scaffoldConsecutive = 0;
  }
};

Check *makeNoSlow() { return new NoSlowCheck(); }
Check *makeAutoBlock() { return new AutoBlockCheck(); }
Check *makeEagle() { return new EagleCheck(); }
Check *makeScaffold() { return new ScaffoldCheck(); }

} // namespace Checks
} // namespace Anticheat
