#define WIN32_LEAN_AND_MEAN
#include "../Java.h"
#include "../Utils/Logger.h"
#include "StatsTracker.internal.h"

#include <Windows.h>
#include <mutex>

namespace OVson {

void JCache::init(JNIEnv *env) {
  if (initialized)
    return;
  std::lock_guard<std::mutex> lock(initMutex);
  if (initialized)
    return;

  if (!env)
    return;

  if (!worldCls) {
    jclass local = lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
    if (!local)
      return;
    worldCls = (jclass)env->NewGlobalRef(local);
  }
  if (worldCls) {
    m_getScoreboard = env->GetMethodID(
        worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScoreboard = env->GetMethodID(
          worldCls, "func_96441_U", "()Lnet/minecraft/scoreboard/Scoreboard;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScoreboard = env->GetMethodID(
          worldCls, "func_96441_as", "()Lnet/minecraft/scoreboard/Scoreboard;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScoreboard = env->GetMethodID(
          worldCls, "func_72967_aN", "()Lnet/minecraft/scoreboard/Scoreboard;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScoreboard = env->GetMethodID(
          worldCls, "func_72883_A", "()Lnet/minecraft/scoreboard/Scoreboard;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScoreboard = env->GetMethodID(worldCls, "Z", "()Lauo;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  if (!sbCls) {
    jclass local = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
    if (!local)
      return;
    sbCls = (jclass)env->NewGlobalRef(local);
  }
  if (sbCls) {
    m_getPlayersTeam = env->GetMethodID(
        sbCls, "getPlayersTeam",
        "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getPlayersTeam = env->GetMethodID(
          sbCls, "func_96509_i",
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getPlayersTeam =
          env->GetMethodID(sbCls, "h", "(Ljava/lang/String;)Laul;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();

    m_getObjectiveInDisplaySlot =
        env->GetMethodID(sbCls, "getObjectiveInDisplaySlot",
                         "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getObjectiveInDisplaySlot =
          env->GetMethodID(sbCls, "func_96539_a",
                           "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getObjectiveInDisplaySlot = env->GetMethodID(sbCls, "a", "(I)Lauk;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();

    m_getObjective = env->GetMethodID(
        sbCls, "getObjective",
        "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScoreObjective;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getObjective = env->GetMethodID(
          sbCls, "func_96518_b",
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScoreObjective;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getObjective =
          env->GetMethodID(sbCls, "b", "(Ljava/lang/String;)Lauk;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();

    m_getValueFromObjective =
        env->GetMethodID(sbCls, "getValueFromObjective",
                         "(Ljava/lang/String;Lnet/minecraft/scoreboard/"
                         "ScoreObjective;)Lnet/minecraft/scoreboard/Score;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getValueFromObjective = env->GetMethodID(
          sbCls, "func_96529_a",
          "(Ljava/lang/String;Lnet/minecraft/scoreboard/ScoreObjective;)Lnet/"
          "minecraft/scoreboard/Score;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getValueFromObjective =
          env->GetMethodID(sbCls, "c", "(Ljava/lang/String;Lauk;)Laum;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();

    m_onScoreUpdated = env->GetMethodID(sbCls, "broadcastScoreUpdate",
                                        "(Ljava/lang/String;)V");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_onScoreUpdated =
          env->GetMethodID(sbCls, "func_96516_a", "(Ljava/lang/String;)V");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_onScoreUpdated = env->GetMethodID(sbCls, "a", "(Ljava/lang/String;)V");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  if (!teamCls) {
    jclass local = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
    if (!local)
      return;
    teamCls = (jclass)env->NewGlobalRef(local);
  }
  if (teamCls) {
    m_getPrefix =
        env->GetMethodID(teamCls, "getPrefix", "()Ljava/lang/String;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getPrefix =
          env->GetMethodID(teamCls, "func_96668_e", "()Ljava/lang/String;");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getPrefix = env->GetMethodID(teamCls, "e", "()Ljava/lang/String;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  if (!scoreCls) {
    jclass local = lc->GetClass("net.minecraft.scoreboard.Score");
    if (!local)
      return;
    scoreCls = (jclass)env->NewGlobalRef(local);
  }
  if (scoreCls) {
    m_getScorePoints = env->GetMethodID(scoreCls, "getScorePoints", "()I");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScorePoints = env->GetMethodID(scoreCls, "func_96652_c", "()I");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_getScorePoints = env->GetMethodID(scoreCls, "c", "()I");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();

    m_setScorePoints = env->GetMethodID(scoreCls, "setScorePoints", "(I)V");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_setScorePoints = env->GetMethodID(scoreCls, "func_96647_a", "(I)V");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_setScorePoints = env->GetMethodID(scoreCls, "func_96647_c", "(I)V");
    }
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      m_setScorePoints = env->GetMethodID(scoreCls, "c", "(I)V");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  if (!f_gpName) {
    jclass gpCls = lc->GetClass("com.mojang.authlib.GameProfile");
    if (!gpCls)
      return;
    f_gpName = env->GetFieldID(gpCls, "name", "Ljava/lang/String;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      f_gpName = env->GetFieldID(gpCls, "field_109761_d", "Ljava/lang/String;");
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  initialized = true;

  Logger::info("JCache init: m_getScorePoints=%p m_setScorePoints=%p "
               "m_getValueFromObjective=%p m_onScoreUpdated=%p "
               "m_getObjectiveInDisplaySlot=%p",
               m_getScorePoints, m_setScorePoints, m_getValueFromObjective,
               m_onScoreUpdated, m_getObjectiveInDisplaySlot);
}

void JCache::cleanup(JNIEnv *env) {
  std::lock_guard<std::mutex> lock(initMutex);
  if (!initialized)
    return;
  if (env) {
    if (worldCls)
      env->DeleteGlobalRef(worldCls);
    if (sbCls)
      env->DeleteGlobalRef(sbCls);
    if (teamCls)
      env->DeleteGlobalRef(teamCls);
    if (scoreCls)
      env->DeleteGlobalRef(scoreCls);
  }
  worldCls = nullptr;
  sbCls = nullptr;
  teamCls = nullptr;
  scoreCls = nullptr;
  f_gpName = nullptr;
  initialized = false;
}

} // namespace OVson
