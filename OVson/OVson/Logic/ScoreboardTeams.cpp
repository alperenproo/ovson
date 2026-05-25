#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Java.h"
#include "../Utils/Logger.h"

#include <Windows.h>
#include <cstring>
#include <jni.h>
#include <mutex>
#include <string>

namespace OVson {

const char *mcColorForTeam(const std::string &team) {
  if (team == "Red")
    return "\xC2\xA7"
           "c";
  if (team == "Blue")
    return "\xC2\xA7"
           "9";
  if (team == "Green")
    return "\xC2\xA7"
           "a";
  if (team == "Yellow")
    return "\xC2\xA7"
           "e";
  if (team == "Aqua")
    return "\xC2\xA7"
           "b";
  if (team == "White")
    return "\xC2\xA7"
           "f";
  if (team == "Pink")
    return "\xC2\xA7"
           "d";
  if (team == "Gray" || team == "Grey")
    return "\xC2\xA7"
           "8";
  return "\xC2\xA7"
         "f";
}

const char *teamInitial(const std::string &team) {
  if (team == "Red")
    return "R";
  if (team == "Blue")
    return "B";
  if (team == "Green")
    return "G";
  if (team == "Yellow")
    return "Y";
  if (team == "Aqua")
    return "A";
  if (team == "White")
    return "W";
  if (team == "Pink")
    return "P";
  if (team == "Gray" || team == "Grey")
    return "G";
  return "?";
}

bool isRealBedwarsTeam(const std::string &t) {
  return t == "Red" || t == "Blue" || t == "Green" || t == "Yellow" ||
         t == "Aqua" || t == "Pink" || t == "White";
}

void setTeamColorSticky(const std::string &name, const std::string &newTeam) {
  if (newTeam.empty())
    return;
  auto it = g_playerTeamColor.find(name);
  if (it != g_playerTeamColor.end() && isRealBedwarsTeam(it->second) &&
      (newTeam == "Gray" || newTeam == "Grey")) {
    return;
  }
  g_playerTeamColor[name] = newTeam;
}

std::string teamFromColorCode(char code) {
  switch (code) {
  case 'c':
    return "Red";
  case '9':
    return "Blue";
  case 'a':
    return "Green";
  case 'e':
    return "Yellow";
  case 'b':
    return "Aqua";
  case 'f':
    return "White";
  case '7':
    return "Gray";
  case 'd':
    return "Pink";
  case '8':
    return "Gray";
  default:
    return "Unknown";
  }
}

void detectTeamsFromLine(const std::string &chat) {
  static const char *teams[] = {"Red",   "Blue", "Green", "Yellow", "Aqua",
                                "White", "Pink", "Gray",  "Grey"};
  for (const char *t : teams) {
    std::string needle1 = std::string("You are on the ") + t + " Team!";
    if (chat.find(needle1) != std::string::npos) {
      Logger::info("Local team detected: %s", t);
      g_localTeam = t;
      if (!g_localName.empty() && !g_localTeam.empty()) {
        g_playerTeamColor[g_localName] = g_localTeam;
      }
      sendTeamStatsReport();
    }
    std::string needle2 = std::string(" joined (") + t + ")";
    auto p2 = chat.find(needle2);
    if (p2 != std::string::npos) {
      auto s = chat.rfind(' ', p2);
      std::string name =
          (s == std::string::npos) ? std::string() : chat.substr(0, s);
      auto sp = name.find_last_of(' ');
      if (sp != std::string::npos)
        name = name.substr(sp + 1);
      if (!name.empty()) {
        setTeamColorSticky(name, t);
        Logger::info("Team detected: %s -> %s", name.c_str(), t);
      }
    }
  }
}

void updateTeamsFromScoreboard() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

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
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  jobject mcObj = nullptr;
  if (m_getMc)
    mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj && theMc)
    mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  if (!f_world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass worldCls =
      lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
  if (!worldCls) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jmethodID m_getScoreboard = env->GetMethodID(
      worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(
        worldCls, "func_96441_U", "()Lnet/minecraft/scoreboard/Scoreboard;");
  }
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(worldCls, "Z", "()Lauo;");
  }
  if (!m_getScoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject scoreboard = env->CallObjectMethod(world, m_getScoreboard);
  if (!scoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass sbCls = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
  if (!sbCls) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jmethodID m_getPlayersTeam = env->GetMethodID(
      sbCls, "getPlayersTeam",
      "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam = env->GetMethodID(
        sbCls, "func_96509_i",
        "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  }
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam =
        env->GetMethodID(sbCls, "h", "(Ljava/lang/String;)Laul;");
  }
  if (!m_getPlayersTeam) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
  if (!teamCls)
    return;

  auto oldReporter = Lunar::reporter;
  Lunar::reporter = nullptr;

  jmethodID m_getPrefix = lc->GetMethodID(
      teamCls, "getPrefix", "()Ljava/lang/String;", "func_96668_e", "e");
  if (!m_getPrefix) {
    m_getPrefix = lc->GetMethodID(teamCls, "getColorPrefix",
                                  "()Ljava/lang/String;", "func_96661_b", "b");
  }

  jmethodID m_getSuffix = lc->GetMethodID(
      teamCls, "getSuffix", "()Ljava/lang/String;", "func_96663_f", "f");
  if (!m_getSuffix) {
    m_getSuffix = lc->GetMethodID(teamCls, "getColorSuffix",
                                  "()Ljava/lang/String;", "func_96662_c", "c");
  }
  (void)m_getSuffix; // resolved for completeness but only prefix is used below

  Lunar::reporter = oldReporter;

  for (const std::string &name : g_onlinePlayers) {
    jstring jn = env->NewStringUTF(name.c_str());
    jobject team = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jn);
    if (team) {
      jstring pref = nullptr;
      if (m_getPrefix)
        pref = (jstring)env->CallObjectMethod(team, m_getPrefix);
      env->ExceptionClear();
      if (pref) {
        const char *utf = env->GetStringUTFChars(pref, 0);
        if (utf) {
          const char *sect = strchr(utf, '\xC2');
          char code = 0;
          const char *raw = strchr(utf, '\xA7');
          if (raw && raw[1])
            code = raw[1];
          if (!code && sect) {
            const unsigned char *u = (const unsigned char *)utf;
            for (size_t i = 0; u[i]; ++i) {
              if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
                code = (char)u[i + 2];
                break;
              }
            }
          }
          if (code) {
            std::string tname = teamFromColorCode(code);
            if (!tname.empty())
              setTeamColorSticky(name, tname);
          }
          env->ReleaseStringUTFChars(pref, utf);
        }
        env->DeleteLocalRef(pref);
      }
      env->DeleteLocalRef(team);
    }
    env->DeleteLocalRef(jn);
  }
  env->DeleteLocalRef(scoreboard);
  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

std::string resolveTeamForNameEx(JNIEnv *env, const std::string &name,
                                 jobject scoreboard, jmethodID m_getPlayersTeam,
                                 jclass teamCls, jmethodID m_getPrefix) {
  if (!env || !scoreboard || !m_getPlayersTeam || !teamCls || !m_getPrefix)
    return std::string();

  jstring jname = env->NewStringUTF(name.c_str());
  jobject teamObj = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jname);
  env->ExceptionClear();
  env->DeleteLocalRef(jname);

  std::string result;
  if (teamObj) {
    jstring pref = (jstring)env->CallObjectMethod(teamObj, m_getPrefix);
    env->ExceptionClear();
    if (pref) {
      const unsigned char *u =
          (const unsigned char *)env->GetStringUTFChars(pref, 0);
      char code = 0;
      if (u) {
        for (size_t i = 0; u[i]; ++i) {
          if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
            code = (char)u[i + 2];
            break;
          }
        }
        env->ReleaseStringUTFChars(pref, (const char *)u);
      }
      if (code) {
        std::string tname = teamFromColorCode(code);
        if (!tname.empty())
          result = tname;
      }
      env->DeleteLocalRef(pref);
    }
    env->DeleteLocalRef(teamObj);
  }
  return result;
}

std::string resolveTeamForName(const std::string &name) {
  JNIEnv *env = lc->getEnv();
  if (!g_initialized || !env)
    return std::string();

  if (!g_localName.empty() && name == g_localName) {
    if (!g_localTeam.empty())
      return g_localTeam;
  }

  {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    auto itT = g_playerTeamColor.find(name);
    if (itT != g_playerTeamColor.end() && !itT->second.empty()) {
      return itT->second;
    }
  }

  g_jCache.init(env);

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return std::string();
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
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
  if (!mcObj)
    return std::string();

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

  std::string result;
  if (world) {
    jmethodID m_getScoreboard = g_jCache.m_getScoreboard;
    jobject scoreboard = m_getScoreboard
                             ? env->CallObjectMethod(world, m_getScoreboard)
                             : nullptr;
    env->ExceptionClear();
    if (scoreboard) {
      result =
          resolveTeamForNameEx(env, name, scoreboard, g_jCache.m_getPlayersTeam,
                               g_jCache.teamCls, g_jCache.m_getPrefix);
      env->DeleteLocalRef(scoreboard);
    }
    env->DeleteLocalRef(world);
  }
  env->DeleteLocalRef(mcObj);
  return result;
}

} // namespace OVson
