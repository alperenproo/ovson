#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Render/RenderHook.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/Timer.h"

#include <Windows.h>
#include <atomic>

namespace OVson {

void poll() {
  SafeGuard::installSehTranslator();
  SafeGuard::run("OVson::poll", []() { pollBody(); });
}

void pollBody() {
  JNIEnv *env = lc->getEnv();
  if (!g_initialized || !env)
    return;
  if (lc->CheckException())
    return;

  ULONGLONG now = GetTickCount64();
  if (g_lastChatReadTick == 0 || (now - g_lastChatReadTick) >= 20) {
    g_lastChatReadTick = now;
    tailLogOnce();

    static std::atomic<bool> tabStatsRunning{false};
    static std::atomic<bool> preGameRunning{false};
    static std::atomic<bool> teamScanRunning{false};

    if (!tabStatsRunning.exchange(true)) {
      RenderHook::enqueueTask([]() {
        updateTabListStats();
        tabStatsRunning.store(false);
      });
    }
    if (!preGameRunning.exchange(true)) {
      RenderHook::enqueueTask([]() {
        detectPreGameLobby();
        preGameRunning.store(false);
      });
    }

    if (g_lastTeamScanTick == 0 ||
        (now - g_lastTeamScanTick) >=
            (g_inHypixelGame && (now - g_lastResetTick < 10000) ? 200 : 1000)) {
      g_lastTeamScanTick = now;
      if (!teamScanRunning.exchange(true)) {
        RenderHook::enqueueTask([]() {
          updateTeamsFromScoreboard();
          teamScanRunning.store(false);
        });
      }
    }

    static ULONGLONG lastCleanup = 0;
    if (lastCleanup == 0 || (now - lastCleanup) >= 10000) {
      lastCleanup = now;
      cleanupStaleStats();
      pruneStatsCache();
    }

    static ULONGLONG lastSync = 0;
    if (lastSync == 0 || (now - lastSync) >= 5000) {
      lastSync = now;
      syncTeamColors();
    }

    static ULONGLONG lastTagSync = 0;
    if (lastTagSync == 0 || (now - lastTagSync) >= 2000) {
      lastTagSync = now;
      syncTags();
    }

    if (Config::isBedDefenseEnabled()) {
      BedDefense::BedDefenseManager::getInstance()->tick();
    }

    queuePlayersForFetching();
    processPendingStats();
  }

  double startTime = TimeUtil::getTime();

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc)
    return;

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_screen = lc->GetFieldID(mcCls, "currentScreen",
                                     "Lnet/minecraft/client/gui/GuiScreen;",
                                     "field_71462_r", "m", "Laxu;");
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = lc->GetFieldID(mcCls, "currentScreen",
                              "Lnet/minecraft/client/gui/GuiScreen;",
                              "field_71462_r", "ay", "Laxu;");
  }
  if (!f_screen) {
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject screen = env->GetObjectField(mcObj, f_screen);
  if (!screen) {
    env->DeleteLocalRef(mcObj);
    return;
  }

  env->DeleteLocalRef(mcObj);
  env->DeleteLocalRef(screen);

  double endTime = TimeUtil::getTime();
  float rawLatency = (float)((endTime - startTime) * 1000.0);
  if (g_jniLatency == 0.0f) {
    g_jniLatency = rawLatency;
  } else {
    g_jniLatency = (g_jniLatency * 0.9f) + (rawLatency * 0.1f);
  }
}

} // namespace OVson
