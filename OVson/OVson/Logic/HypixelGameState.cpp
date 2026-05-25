// kapat ışıklarını

#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Render/NotificationManager.h"
#include "../Utils/Logger.h"

#include <Windows.h>
#include <cctype>
#include <jni.h>
#include <mutex>
#include <string>

namespace OVson {

void detectFinalKillsFromLine(const std::string &chat) {
  // "gamerboy80 was killed by sekerbenimkedim. FINAL KILL!"
  std::string lowerChat = chat;
  for (auto &c : lowerChat)
    c = (char)toupper((unsigned char)c);

  if (lowerChat.find("FINAL KILL!") == std::string::npos)
    return;

  std::string clean = "";
  for (size_t i = 0; i < chat.length(); ++i) {
    if ((unsigned char)chat[i] == 0xC2 && i + 1 < chat.length() &&
        (unsigned char)chat[i + 1] == 0xA7) {
      i += 2;
      continue;
    }
    if ((unsigned char)chat[i] == 0xA7) {
      i += 1;
      continue;
    }
    clean += chat[i];
  }
  while (!clean.empty() && isspace((unsigned char)clean[0]))
    clean.erase(0, 1);
  if (clean.empty())
    return;

  std::string victim;
  if (clean[0] == '[') {
    size_t firstSpace = clean.find(' ');
    if (firstSpace != std::string::npos) {
      std::string afterRank = clean.substr(firstSpace + 1);
      size_t endOfName = afterRank.find(' ');
      if (endOfName != std::string::npos) {
        victim = afterRank.substr(0, endOfName);
      }
    }
  } else {
    size_t firstSpace = clean.find(' ');
    if (firstSpace != std::string::npos) {
      victim = clean.substr(0, firstSpace);
    }
  }

  if (!victim.empty()) {
    std::lock_guard<std::mutex> lock(g_statsMutex);
    if (g_playerStatsMap.find(victim) != g_playerStatsMap.end()) {
      g_playerStatsMap.erase(victim);
      Logger::info("Player removed from GUI due to FINAL KILL: %s",
                   victim.c_str());
    }
  }
}

void detectBedDestructionFromLine(const std::string &chat) {
  std::string clean = "";
  for (size_t i = 0; i < chat.length(); ++i) {
    if ((unsigned char)chat[i] == 0xC2 && i + 1 < chat.length() &&
        (unsigned char)chat[i + 1] == 0xA7) {
      i += 2;
      continue;
    }
    if ((unsigned char)chat[i] == 0xA7) {
      i += 1;
      continue;
    }
    clean += chat[i];
  }
  if (clean.find("BED DESTRUCTION >") != std::string::npos) {
    // lets do this later
  }
}

void detectPreGameLobby() {
  JNIEnv *env = lc->getEnv();
  if (!env || !g_initialized)
    return;

  ULONGLONG now = GetTickCount64();
  if (now - g_preGameDetectTick < 500)
    return;
  g_preGameDetectTick = now;

  if (g_inHypixelGame) {
    if (g_inPreGameLobby) {
      g_inPreGameLobby = false;
      Logger::log(Config::DebugCategory::GameDetection,
                  "Pre-game lobby ended (game started)");
      sendTeamStatsReport();
    }
    return;
  }

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
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return;
  }

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
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
    if (env->ExceptionCheck())
      env->ExceptionClear();
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return;
  }

  g_jCache.init(env);
  jmethodID m_getSB = g_jCache.m_getScoreboard;
  if (!m_getSB) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject scoreboard = env->CallObjectMethod(world, m_getSB);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!scoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jmethodID m_getObj = g_jCache.m_getObjectiveInDisplaySlot;
  if (!m_getObj) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject sidebarObj = env->CallObjectMethod(scoreboard, m_getObj, 1);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!sidebarObj) {
    if (Config::isDebugging()) {
      static ULONGLONG lastSbDbg = 0;
      if (now - lastSbDbg > 3000) {
        ChatSDK::showClientMessage(
            ChatSDK::formatPrefix() + "\xC2\xA7" +
            "7[DEBUG] detectPreGameLobby: Sidebar is NULL");
        lastSbDbg = now;
      }
    }
    if (g_inPreGameLobby) {
      g_inPreGameLobby = false;
      Logger::log(Config::DebugCategory::GameDetection,
                  "Pre-game lobby ended (no sidebar)");
    }
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  static jmethodID s_m_getSorted = nullptr;
  static jmethodID s_m_getPlayerName = nullptr;
  static bool s_resolved = false;
  static std::string s_objClassName;

  if (!s_resolved) {
    jclass sbCls = g_jCache.sbCls;
    jclass scoreCls = g_jCache.scoreCls;

    jclass objRealCls = env->GetObjectClass(sidebarObj);
    if (objRealCls) {
      jclass classCls = env->FindClass("java/lang/Class");
      jmethodID m_getName =
          env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
      if (m_getName) {
        jstring nameJ = (jstring)env->CallObjectMethod(objRealCls, m_getName);
        if (nameJ) {
          const char *nameUtf = env->GetStringUTFChars(nameJ, 0);
          if (nameUtf) {
            s_objClassName = nameUtf;
            for (auto &ch : s_objClassName) {
              if (ch == '.')
                ch = '/';
            }
            env->ReleaseStringUTFChars(nameJ, nameUtf);
          }
          env->DeleteLocalRef(nameJ);
        }
      }
      env->DeleteLocalRef(classCls);
      env->DeleteLocalRef(objRealCls);
    }

    if (sbCls && !s_objClassName.empty()) {
      std::string dynSig = "(L" + s_objClassName + ";)Ljava/util/Collection;";

      const char *knownNames[] = {"getSortedScores", "func_96534_i", nullptr};
      const char *sigs[] = {
          "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;",
          dynSig.c_str(), nullptr};

      for (int s = 0; sigs[s] && !s_m_getSorted; s++) {
        for (int n = 0; knownNames[n] && !s_m_getSorted; n++) {
          s_m_getSorted = env->GetMethodID(sbCls, knownNames[n], sigs[s]);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getSorted = nullptr;
          }
        }
      }

      if (!s_m_getSorted) {
        for (char c = 'a'; c <= 'z' && !s_m_getSorted; c++) {
          char name[2] = {c, 0};
          s_m_getSorted = env->GetMethodID(sbCls, name, dynSig.c_str());
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getSorted = nullptr;
          }
        }
      }
    }

    if (scoreCls) {
      const char *scoreNames[] = {"getPlayerName", "func_96653_e", nullptr};
      for (int n = 0; scoreNames[n] && !s_m_getPlayerName; n++) {
        s_m_getPlayerName =
            env->GetMethodID(scoreCls, scoreNames[n], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getPlayerName = nullptr;
        }
      }

      if (!s_m_getPlayerName) {
        for (char c = 'a'; c <= 'z' && !s_m_getPlayerName; c++) {
          char name[2] = {c, 0};
          s_m_getPlayerName =
              env->GetMethodID(scoreCls, name, "()Ljava/lang/String;");
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getPlayerName = nullptr;
          }
        }
      }
    }

    s_resolved = true;
  }

  static jmethodID s_m_getPlayersTeam = nullptr;
  static jmethodID s_m_getPrefix = nullptr;
  static jmethodID s_m_getSuffix = nullptr;
  static bool s_teamRes = false;

  if (!s_teamRes) {
    jclass sbCls = g_jCache.sbCls;
    if (sbCls) {
      const char *names[] = {"getPlayersTeam", "func_96509_i", "h", "i",
                             nullptr};
      const char *sigs[] = {
          "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;",
          "(Ljava/lang/String;)Laul;", "(Ljava/lang/String;)Lauq;", nullptr};
      for (int s = 0; sigs[s] && !s_m_getPlayersTeam; s++) {
        for (int n = 0; names[n] && !s_m_getPlayersTeam; n++) {
          s_m_getPlayersTeam = env->GetMethodID(sbCls, names[n], sigs[s]);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
            s_m_getPlayersTeam = nullptr;
          } else {
            Logger::info("Resolved getPlayersTeam with %s %s", names[n],
                         sigs[s]);
          }
        }
      }
    }

    jclass ptCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
    if (!ptCls) {
      ptCls = env->FindClass("aul");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    if (!ptCls) {
      ptCls = env->FindClass("auq");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (ptCls) {
      const char *prefNames[] = {"getColorPrefix", "func_96668_e", "e",
                                 nullptr};
      const char *sufNames[] = {"getColorSuffix", "func_96663_f", "f", nullptr};

      for (int i = 0; prefNames[i] && !s_m_getPrefix; i++) {
        s_m_getPrefix =
            env->GetMethodID(ptCls, prefNames[i], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getPrefix = nullptr;
        }
      }

      for (int i = 0; sufNames[i] && !s_m_getSuffix; i++) {
        s_m_getSuffix =
            env->GetMethodID(ptCls, sufNames[i], "()Ljava/lang/String;");
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          s_m_getSuffix = nullptr;
        }
      }
    }
    s_teamRes = true;
  }

  bool wasPreGame = g_inPreGameLobby;
  bool foundMap = false, foundPlayers = false, foundMode = false;

  if (s_m_getSorted && s_m_getPlayerName) {
    jobject scoresCollection =
        env->CallObjectMethod(scoreboard, s_m_getSorted, sidebarObj);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      scoresCollection = nullptr;
    }

    if (scoresCollection) {
      jclass collCls = env->FindClass("java/util/Collection");
      jmethodID m_iterator =
          env->GetMethodID(collCls, "iterator", "()Ljava/util/Iterator;");
      jclass iterCls = env->FindClass("java/util/Iterator");
      jmethodID m_hasNext = env->GetMethodID(iterCls, "hasNext", "()Z");
      jmethodID m_next =
          env->GetMethodID(iterCls, "next", "()Ljava/lang/Object;");

      if (m_iterator && m_hasNext && m_next) {
        jobject iter = env->CallObjectMethod(scoresCollection, m_iterator);
        int lineCount = 0;
        while (iter && env->CallBooleanMethod(iter, m_hasNext) &&
               lineCount < 20) {
          jobject scoreObj = env->CallObjectMethod(iter, m_next);
          if (scoreObj) {
            jstring pnJ =
                (jstring)env->CallObjectMethod(scoreObj, s_m_getPlayerName);
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              pnJ = nullptr;
            }

            if (pnJ) {
              std::string rawLine;
              std::string prefRaw, nameRaw, suffRaw;

              const char *pnUtf = env->GetStringUTFChars(pnJ, 0);
              if (pnUtf) {
                nameRaw = pnUtf;
                env->ReleaseStringUTFChars(pnJ, pnUtf);
              }

              if (s_m_getPlayersTeam && s_m_getPrefix && s_m_getSuffix) {
                jobject teamObj =
                    env->CallObjectMethod(scoreboard, s_m_getPlayersTeam, pnJ);
                if (env->ExceptionCheck())
                  env->ExceptionClear();

                if (teamObj) {
                  jstring prefJ =
                      (jstring)env->CallObjectMethod(teamObj, s_m_getPrefix);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  if (prefJ) {
                    const char *prefUtf = env->GetStringUTFChars(prefJ, 0);
                    if (prefUtf) {
                      prefRaw = prefUtf;
                      env->ReleaseStringUTFChars(prefJ, prefUtf);
                    }
                    env->DeleteLocalRef(prefJ);
                  }

                  jstring suffJ =
                      (jstring)env->CallObjectMethod(teamObj, s_m_getSuffix);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  if (suffJ) {
                    const char *suffUtf = env->GetStringUTFChars(suffJ, 0);
                    if (suffUtf) {
                      suffRaw = suffUtf;
                      env->ReleaseStringUTFChars(suffJ, suffUtf);
                    }
                    env->DeleteLocalRef(suffJ);
                  }
                  env->DeleteLocalRef(teamObj);
                }
              }

              rawLine = prefRaw + nameRaw + suffRaw;

              std::string clean;
              for (size_t i = 0; i < rawLine.length(); ++i) {
                unsigned char c = (unsigned char)rawLine[i];
                if (c == 0xC2 && i + 1 < rawLine.length() &&
                    (unsigned char)rawLine[i + 1] == 0xA7) {
                  i += 2;
                  continue;
                }
                if (c == 0xA7) {
                  i += 1;
                  continue;
                }
                clean += (char)c;
              }

              if (clean.find("Map:") != std::string::npos)
                foundMap = true;
              if (clean.find("Players:") != std::string::npos)
                foundPlayers = true;
              if (clean.find("Mode:") != std::string::npos)
                foundMode = true;

              if (clean.length() >= 8 && isdigit((unsigned char)clean[0]) &&
                  isdigit((unsigned char)clean[1]) && clean[2] == '/' &&
                  isdigit((unsigned char)clean[3]) &&
                  isdigit((unsigned char)clean[4]) && clean[5] == '/') {
                static std::string lastServerLine = "";
                if (clean != lastServerLine) {
                  lastServerLine = clean;
                  g_chatPrintedPlayers.clear();
                }
              }

              env->DeleteLocalRef(pnJ);
            }
            env->DeleteLocalRef(scoreObj);
          }
          lineCount++;
        }
        if (iter)
          env->DeleteLocalRef(iter);
      }
      if (collCls)
        env->DeleteLocalRef(collCls);
      if (iterCls)
        env->DeleteLocalRef(iterCls);
      env->DeleteLocalRef(scoresCollection);
    }
  }

  bool isPreGame = (foundMap && foundPlayers) || (foundMap && foundMode) ||
                   (foundPlayers && foundMode);

  if (Config::isDebugging()) {
    static ULONGLONG lastPreDbg = 0;
    if (now - lastPreDbg > 5000) {
      ChatSDK::showClientMessage(
          ChatSDK::formatPrefix() + "\xC2\xA7" +
          "e[DEBUG] PreGame Detection Results: Map=" +
          (foundMap ? "Yes" : "No") +
          " Players=" + (foundPlayers ? "Yes" : "No") +
          " Mode=" + (foundMode ? "Yes" : "No") +
          " Result=" + (isPreGame ? "PreGame" : "NotPreGame"));
      lastPreDbg = now;
    }
  }

  if (isPreGame && !wasPreGame) {
    g_inPreGameLobby = true;
    clearAllCaches();
    Logger::log(Config::DebugCategory::GameDetection,
                "Pre-game lobby DETECTED (sidebar has Map/Players/Mode)");
    Render::NotificationManager::getInstance()->add(
        "System", "Pre-Game Lobby Detected", Render::NotificationType::Success);
  } else if (!isPreGame && wasPreGame) {
    g_inPreGameLobby = false;
    clearAllCaches();
    Logger::log(Config::DebugCategory::GameDetection, "Pre-game lobby ended");
  }

  g_inPreGameLobby = isPreGame;

  env->DeleteLocalRef(sidebarObj);
  env->DeleteLocalRef(scoreboard);
  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

} // namespace OVson
