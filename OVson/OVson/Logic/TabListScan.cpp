#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Java.h"
#include "../Render/NotificationManager.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/Logger.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <jni.h>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OVson {

void updateTabListStats() {
  JNIEnv *env = lc->getEnv();
  jobject iter = nullptr;
  if (!g_initialized || !env)
    return;

  static bool s_firstTick = true;
  if (s_firstTick) {
    if (Config::isGlobalDebugEnabled()) {
      ChatSDK::showPrefixed("§a[DEBUG] updateTabListStats heartbeat active.");
    }
    s_firstTick = false;
  }

  static ULONGLONG lastUpdate = 0;
  ULONGLONG now = GetTickCount64();

  bool isTabEnabled = Config::isTabEnabled();
  if (!isTabEnabled) {
    static ULONGLONG lastWarn = 0;
    if (now - lastWarn > 30000) {
      if (Config::isGlobalDebugEnabled()) {
        ChatSDK::showPrefixed(
            "§7[DEBUG] Detection skipped: Tab is disabled in Config.");
      }
      lastWarn = now;
    }
  }

  static bool s_wasTabEnabled = false;
  bool forceReset = s_wasTabEnabled && !isTabEnabled;
  s_wasTabEnabled = isTabEnabled;

  bool doTabUpdate =
      (isTabEnabled && (now - lastUpdate >= (g_inHypixelGame ? 20 : 50))) ||
      forceReset;
  if (doTabUpdate && isTabEnabled)
    lastUpdate = now;

  if (!isTabEnabled && !forceReset)
    return;
  if (!env)
    return;

  static jclass mcCls = nullptr;
  static jfieldID theMc = nullptr;
  static jmethodID m_getNet = nullptr;
  static jclass nhCls = nullptr;
  static jmethodID m_getMap = nullptr;
  static jclass localCollCls = nullptr;
  static jmethodID m_size = nullptr;

  if (!mcCls) {
    mcCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.client.Minecraft"));
    if (mcCls) {
      theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
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

      m_getNet = env->GetMethodID(
          mcCls, "getNetHandler",
          "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
      if (!m_getNet) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getNet = env->GetMethodID(
            mcCls, "func_147114_u",
            "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
      }
      if (!m_getNet) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getNet = env->GetMethodID(mcCls, "ay", "()Lbcy;");
      }
    } else {
      ChatSDK::showPrefixed("§cCRITICAL: FAILED to find Minecraft class!");
      Logger::log(Config::DebugCategory::GUI,
                  "FAILED to find Minecraft class!");
    }

    jclass tmpNh =
        lc->GetClass("net.minecraft.client.network.NetHandlerPlayClient");
    if (!tmpNh) {
      ChatSDK::showPrefixed(
          "§cCRITICAL: FAILED to find NetHandlerPlayClient class!");
      return;
    }

    nhCls = (jclass)env->NewGlobalRef(tmpNh);
    m_getMap =
        env->GetMethodID(nhCls, "getPlayerInfoMap", "()Ljava/util/Collection;");
    if (!m_getMap) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getMap =
          env->GetMethodID(nhCls, "func_175106_d", "()Ljava/util/Collection;");
    }
    if (!m_getMap) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getMap = env->GetMethodID(nhCls, "d", "()Ljava/util/Collection;");
    }
    if (!m_getMap) {
      if (Config::isGlobalDebugEnabled()) {
        Logger::log(Config::DebugCategory::GUI,
                    "FAILED: getPlayerInfoMap. Dumping NetHandler methods...");
        jint mCount = 0;
        jmethodID *pM = nullptr;
        lc->jvmti->GetClassMethods(nhCls, &mCount, &pM);
        for (int i = 0; i < mCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetMethodName(pM[i], &n, &s, nullptr);
          if (n && s)
            Logger::log(Config::DebugCategory::GUI, "Method: %s | %s", n, s);
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pM)
          lc->jvmti->Deallocate((unsigned char *)pM);
      }
    }

    jclass tmpColl = env->FindClass("java/util/Collection");
    if (tmpColl) {
      localCollCls = (jclass)env->NewGlobalRef(tmpColl);
      m_size = env->GetMethodID(localCollCls, "size", "()I");
    }
  }

  if (!mcCls)
    return;
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
  if (!mcObj) {
    static bool s_oneTime = false;
    if (!s_oneTime) {
      ChatSDK::showPrefixed("§cCRITICAL: theMinecraft object is NULL!");
      s_oneTime = true;
    }
    return;
  }

  jobject nh = m_getNet ? env->CallObjectMethod(mcObj, m_getNet) : nullptr;
  if (!nh) {
    // brute force
    jmethodID bf_getNet = lc->FindMethodBySignature(
        mcCls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
    if (!bf_getNet)
      bf_getNet = lc->FindMethodBySignature(mcCls, "()Lbcy;");
    if (bf_getNet)
      nh = env->CallObjectMethod(mcObj, bf_getNet);
  }

  if (!nh) {
    ChatSDK::showPrefixed("§cCRITICAL: NetHandler object is NULL!");
    env->DeleteLocalRef(mcObj);
    return;
  }

  jobject col = m_getMap ? env->CallObjectMethod(nh, m_getMap) : nullptr;
  if (!col) {
    ChatSDK::showPrefixed("§cCRITICAL: PlayerInfoMap collection is NULL!");
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
    return;
  }

  int playerCount = m_size ? env->CallIntMethod(col, m_size) : 0;

  bool appearsToBeLobby = true;
  bool hasStrictGameKeywords = false;
  std::string detectedServer = "unknown";
  std::string detectionReason = "Default (Lobby)";
  std::string footerText = "";
  std::string compSig = "Lnet/minecraft/util/IChatComponent;";

  static jfieldID f_currServer = nullptr;
  static jclass serverDataCls = nullptr;
  static jfieldID f_serverMOTD = nullptr;

  static jfieldID f_gui = nullptr;
  static jclass guiCls = nullptr;
  static jfieldID f_tab = nullptr;
  static jclass tabCls = nullptr;
  static jfieldID f_footer = nullptr;
  static jclass compCls = nullptr;
  static jmethodID m_getUnf = nullptr;

  if (!f_currServer && mcCls) {
    f_currServer =
        env->GetFieldID(mcCls, "currentServerData",
                        "Lnet/minecraft/client/multiplayer/ServerData;");
    if (!f_currServer) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_currServer =
          env->GetFieldID(mcCls, "field_71422_O",
                          "Lnet/minecraft/client/multiplayer/ServerData;");
    }
    if (!f_currServer) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_currServer = env->GetFieldID(mcCls, "Q", "Lbha;");
    }

    jclass tmpSD = lc->GetClass("net.minecraft.client.multiplayer.ServerData");
    if (!tmpSD)
      return;
    if (tmpSD) {
      serverDataCls = (jclass)env->NewGlobalRef(tmpSD);
      f_serverMOTD =
          env->GetFieldID(serverDataCls, "serverMOTD", "Ljava/lang/String;");
      if (!f_serverMOTD) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_serverMOTD = env->GetFieldID(serverDataCls, "field_78847_f",
                                       "Ljava/lang/String;");
      }
    }

    f_gui = env->GetFieldID(mcCls, "ingameGUI",
                            "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "field_71456_v",
                              "Lnet/minecraft/client/gui/GuiIngame;");
    }
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "q", "Laxe;");
    }
    if (!f_gui) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      f_gui = env->GetFieldID(mcCls, "q", "Lavo;");
    }

    jclass tmpGui = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (tmpGui) {
      guiCls = (jclass)env->NewGlobalRef(tmpGui);

      jclass clsCls = env->GetObjectClass(guiCls);
      jmethodID mid_getName =
          env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
      jstring jsName = (jstring)env->CallObjectMethod(guiCls, mid_getName);
      const char *cName = env->GetStringUTFChars(jsName, 0);
      std::string guiSig =
          "L" +
          (std::string)(cName ? cName : "net/minecraft/client/gui/GuiIngame") +
          ";";
      for (size_t i = 0; i < guiSig.length(); ++i)
        if (guiSig[i] == '.')
          guiSig[i] = '/';
      if (cName)
        env->ReleaseStringUTFChars(jsName, cName);

      if (!f_gui) {
        f_gui = lc->FindFieldBySignature(mcCls, guiSig.c_str());
        if (f_gui)
          Logger::log(Config::DebugCategory::GUI, "AUTO-FOUND GUI: %s",
                      guiSig.c_str());
      }

      f_tab = env->GetFieldID(guiCls, "overlayPlayerList",
                              "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;");
      if (!f_tab) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_tab = env->GetFieldID(guiCls, "v", "Lawh;");
      }
      if (!f_tab) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_tab =
            env->GetFieldID(guiCls, "field_175181_C",
                            "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;");
      }

      if (!f_tab) {
        jint fCount = 0;
        jfieldID *pF = nullptr;
        lc->jvmti->GetClassFields(guiCls, &fCount, &pF);
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(guiCls, pF[i], &n, &s, nullptr);
          if (n && s &&
              (std::string(s).find("GuiPlayerTabOverlay") !=
                   std::string::npos ||
               std::string(s) == "Lawh;")) {
            f_tab = pF[i];
            if (f_tab)
              Logger::log(Config::DebugCategory::GUI, "AUTO-FOUND TAB: %s", s);
            break;
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pF)
          lc->jvmti->Deallocate((unsigned char *)pF);
      }
    }

    jclass tmpTab =
        lc->GetClass("net.minecraft.client.gui.GuiPlayerTabOverlay");
    if (tmpTab) {
      tabCls = (jclass)env->NewGlobalRef(tmpTab);

      jclass tmpComp = lc->GetClass("net.minecraft.util.IChatComponent");
      compSig = "Lnet/minecraft/util/IChatComponent;";
      if (tmpComp) {
        jclass compClsCls = env->GetObjectClass(tmpComp);
        jmethodID comp_getName =
            env->GetMethodID(compClsCls, "getName", "()Ljava/lang/String;");
        jstring jsCompName =
            (jstring)env->CallObjectMethod(tmpComp, comp_getName);
        const char *cCompName = env->GetStringUTFChars(jsCompName, 0);
        if (cCompName) {
          compSig = "L" + std::string(cCompName) + ";";
          for (size_t i = 0; i < compSig.length(); ++i)
            if (compSig[i] == '.')
              compSig[i] = '/';
          env->ReleaseStringUTFChars(jsCompName, cCompName);
        }
      }

      f_footer = env->GetFieldID(tabCls, "footer", compSig.c_str());
      if (!f_footer) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        f_footer = env->GetFieldID(tabCls, "field_175245_I", compSig.c_str());
      }

      if (!f_footer) {
        // scan
        jint fCount = 0;
        jfieldID *pF = nullptr;
        lc->jvmti->GetClassFields(tabCls, &fCount, &pF);
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(tabCls, pF[i], &n, &s, nullptr);
          if (n && s && std::string(s) == compSig) {
            f_footer = pF[i];
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        if (pF)
          lc->jvmti->Deallocate((unsigned char *)pF);
      }
    }

    jclass tmpComp = lc->GetClass("net.minecraft.util.IChatComponent");
    if (!tmpComp)
      return;
    if (tmpComp) {
      compCls = (jclass)env->NewGlobalRef(tmpComp);
      m_getUnf = env->GetMethodID(compCls, "getUnformattedText",
                                  "()Ljava/lang/String;");
      if (!m_getUnf) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getUnf =
            env->GetMethodID(compCls, "func_150260_c", "()Ljava/lang/String;");
      }
      if (!m_getUnf) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        m_getUnf = env->GetMethodID(compCls, "c", "()Ljava/lang/String;");
      }
    }
  }

  jobject serverData =
      f_currServer ? env->GetObjectField(mcObj, f_currServer) : nullptr;
  if (serverData) {
    jstring motd = f_serverMOTD
                       ? (jstring)env->GetObjectField(serverData, f_serverMOTD)
                       : nullptr;
    if (motd) {
      const char *motdUtf = env->GetStringUTFChars(motd, 0);
      if (motdUtf) {
        std::string motdStr = motdUtf;
        if (motdStr.find("Portal") != std::string::npos ||
            motdStr.find("Lobby") != std::string::npos) {
          appearsToBeLobby = true;
          g_explicitLobbySignal = true;
          detectionReason = "MOTD (Lobby Keywords)";
        }
        env->ReleaseStringUTFChars(motd, motdUtf);
      }
      env->DeleteLocalRef(motd);
    }
    env->DeleteLocalRef(serverData);
  }

  jobject gui = f_gui ? env->GetObjectField(mcObj, f_gui) : nullptr;

  if (gui) {
    jobject tab = f_tab ? env->GetObjectField(gui, f_tab) : nullptr;
    if (tab) {
      // again brute force
      std::string allTabText = "";
      jint fCount = 0;
      jfieldID *pF = nullptr;
      if (lc->jvmti->GetClassFields(tabCls, &fCount, &pF) == JVMTI_ERROR_NONE) {
        for (int i = 0; i < fCount; i++) {
          char *n = nullptr, *s = nullptr;
          lc->jvmti->GetFieldName(tabCls, pF[i], &n, &s, nullptr);
          if (n && s && std::string(s) == compSig) {
            jobject comp = env->GetObjectField(tab, pF[i]);
            if (comp) {
              jstring js = m_getUnf
                               ? (jstring)env->CallObjectMethod(comp, m_getUnf)
                               : nullptr;
              if (js) {
                const char *utf = env->GetStringUTFChars(js, 0);
                if (utf) {
                  std::string raw = utf;
                  for (size_t k = 0; k < raw.length(); ++k) {
                    if ((unsigned char)raw[k] == 0xC2 && k + 1 < raw.length() &&
                        (unsigned char)raw[k + 1] == 0xA7) {
                      k += 2;
                      continue;
                    }
                    if ((unsigned char)raw[k] == 0xA7) {
                      k += 1;
                      continue;
                    }
                    allTabText += raw[k];
                  }
                  allTabText += " ";
                  env->ReleaseStringUTFChars(js, utf);
                }
                env->DeleteLocalRef(js);
              }
              env->DeleteLocalRef(comp);
            }
          }
          if (n)
            lc->jvmti->Deallocate((unsigned char *)n);
          if (s)
            lc->jvmti->Deallocate((unsigned char *)s);
        }
        lc->jvmti->Deallocate((unsigned char *)pF);
      }

      std::string footerClean = allTabText;
      if (now - g_lastDetectionLogTick >= 3000) {
        if (Config::isGlobalDebugEnabled()) {
          std::string preview = footerClean;
          if (preview.length() > 100)
            preview = preview.substr(0, 100) + "...";
          Logger::log(Config::DebugCategory::GUI, "Combined Tab Text: '%s'",
                      preview.c_str());
        }
      }

      std::string lowerFooter = footerClean;
      std::transform(lowerFooter.begin(), lowerFooter.end(),
                     lowerFooter.begin(), ::tolower);

      bool foundFinalKills =
          (lowerFooter.find("final kills") != std::string::npos);
      bool foundBedsBroken =
          (lowerFooter.find("beds broken") != std::string::npos ||
           lowerFooter.find("beds b") != std::string::npos);
      bool foundKills = (lowerFooter.find("kills:") != std::string::npos ||
                         lowerFooter.find("kills :") != std::string::npos);

      if (foundFinalKills || foundBedsBroken ||
          (foundKills && !foundBedsBroken)) {
        hasStrictGameKeywords = true;
      }

      size_t srvPos = footerClean.find("Server: ");
      if (srvPos != std::string::npos) {
        std::string srv = footerClean.substr(srvPos + 8);
        size_t space = srv.find_first_of(" \n\r");
        if (space != std::string::npos)
          srv = srv.substr(0, space);
        detectedServer = srv;
      }

      {
        std::lock_guard<std::mutex> lock(g_footerMutex);
        g_tabFooterText = allTabText;
      }

      env->DeleteLocalRef(tab);
    }
    env->DeleteLocalRef(gui);
  }

  static jfieldID s_f_world = nullptr;
  static jclass s_worldCls = nullptr;
  static jmethodID s_m_getSB = nullptr;
  static jclass s_sbCls = nullptr;
  static jmethodID s_m_getObj = nullptr;
  static jclass s_objCls = nullptr;
  static jmethodID s_m_getDisp = nullptr;

  if (!s_f_world && mcCls) {
    s_f_world = env->GetFieldID(
        mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      s_f_world =
          env->GetFieldID(mcCls, "field_71441_e",
                          "Lnet/minecraft/client/multiplayer/WorldClient;");
    }
    if (!s_f_world) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      s_f_world =
          env->GetFieldID(mcCls, "field_71441_e",
                          "Lnet/minecraft/client/multiplayer/WorldClient;");
    }
    if (!s_f_world) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      s_f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
    }

    jclass tmpWorld =
        lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
    if (!tmpWorld)
      return;
    if (tmpWorld) {
      s_worldCls = (jclass)env->NewGlobalRef(tmpWorld);
      s_m_getSB = env->GetMethodID(s_worldCls, "getScoreboard",
                                   "()Lnet/minecraft/scoreboard/Scoreboard;");
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_96441_U",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_96441_as",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_72967_aN",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "func_72883_A",
                                     "()Lnet/minecraft/scoreboard/Scoreboard;");
      }
      if (!s_m_getSB) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getSB = env->GetMethodID(s_worldCls, "Z", "()Lauo;");
      }
    }

    jclass tmpSb = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
    if (!tmpSb)
      return;
    if (tmpSb) {
      s_sbCls = (jclass)env->NewGlobalRef(tmpSb);
      s_m_getObj =
          env->GetMethodID(s_sbCls, "getObjectiveInDisplaySlot",
                           "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      if (!s_m_getObj) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getObj =
            env->GetMethodID(s_sbCls, "func_96539_a",
                             "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
      }
      if (!s_m_getObj) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getObj = env->GetMethodID(s_sbCls, "a", "(I)Lauk;");
      }
    }

    jclass tmpObj = lc->GetClass("net.minecraft.scoreboard.ScoreObjective");
    if (!tmpObj)
      return;
    if (tmpObj) {
      s_objCls = (jclass)env->NewGlobalRef(tmpObj);
      s_m_getDisp =
          env->GetMethodID(s_objCls, "getDisplayName", "()Ljava/lang/String;");
      if (!s_m_getDisp) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getDisp =
            env->GetMethodID(s_objCls, "func_96678_d", "()Ljava/lang/String;");
      }
      if (!s_m_getDisp) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        s_m_getDisp = env->GetMethodID(s_objCls, "d", "()Ljava/lang/String;");
      }
    }
  }

  jobject world = s_f_world ? env->GetObjectField(mcObj, s_f_world) : nullptr;
  if (world) {
    jobject sb = s_m_getSB ? env->CallObjectMethod(world, s_m_getSB) : nullptr;
    if (sb) {
      jobject obj =
          s_m_getObj ? env->CallObjectMethod(sb, s_m_getObj, 1) : nullptr;
      if (obj) {
        jstring dispJ = s_m_getDisp
                            ? (jstring)env->CallObjectMethod(obj, s_m_getDisp)
                            : nullptr;
        if (dispJ) {
          const char *utf = env->GetStringUTFChars(dispJ, 0);
          if (utf) {
            std::string sbTitle = utf;
            if (now - g_lastDetectionLogTick >= 10000) {
              if (g_lastDetectedModeName != "SCOREBOARD") {
                Logger::log(Config::DebugCategory::GameDetection,
                            "Raw Scoreboard: %s", sbTitle.c_str());
                g_lastDetectedModeName = "SCOREBOARD";
                g_lastDetectionLogTick = now;
              }
            }
            std::string sbClean;
            for (size_t i = 0; i < sbTitle.length(); ++i) {
              if ((unsigned char)sbTitle[i] == 0xC2 &&
                  i + 1 < sbTitle.length() &&
                  (unsigned char)sbTitle[i + 1] == 0xA7) {
                i += 2;
                continue;
              }
              if ((unsigned char)sbTitle[i] == 0xA7) {
                i += 1;
                continue;
              }
              sbClean += (char)toupper(sbTitle[i]);
            }

            if (sbClean.find("BED WARS") != std::string::npos ||
                sbClean.find("SKYWARS") != std::string::npos ||
                sbClean.find("DUELS") != std::string::npos ||
                sbClean.find("WARS") != std::string::npos ||
                sbClean.find("THE BRIDGE") != std::string::npos ||
                sbClean.find("TNT") != std::string::npos ||
                sbClean.find("MURDER") != std::string::npos ||
                sbClean.find("GAMES") != std::string::npos) {

              bool isLobbyTitle =
                  (sbClean.find("LOBBY") != std::string::npos ||
                   sbClean.find("WAITING") != std::string::npos ||
                   sbClean.find("STARTING") != std::string::npos);

              if (hasStrictGameKeywords && !isLobbyTitle) {
                appearsToBeLobby = false;
                detectionReason = "Tab Footer Keywords (Game)";
              } else {
                appearsToBeLobby = true;
                if (isLobbyTitle) {
                  detectionReason = "Scoreboard (Lobby/Waiting Keywords)";
                } else {
                  detectionReason = "No Game Keywords in Tab (Lobby)";
                }
              }
            } else if (sbClean == "HYPIXEL" ||
                       sbClean.find("LOBBY") != std::string::npos) {
              appearsToBeLobby = true;
              detectionReason = (sbClean.find("LOBBY") != std::string::npos)
                                    ? "Scoreboard (Lobby Keyword)"
                                    : "Scoreboard (Generic HYPIXEL)";
            }
            env->ReleaseStringUTFChars(dispJ, utf);
          }
          env->DeleteLocalRef(dispJ);
        }
        env->DeleteLocalRef(obj);
      }
      env->DeleteLocalRef(sb);
    }
    env->DeleteLocalRef(world);
  }

  if (detectedServer != "unknown") {
    std::string srvLower = detectedServer;
    std::transform(srvLower.begin(), srvLower.end(), srvLower.begin(),
                   ::tolower);
    if (srvLower.find("lobby") != std::string::npos ||
        srvLower.find("mega") != std::string::npos) {
      appearsToBeLobby = true;
      detectionReason = "Server ID (Lobby)";
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "LOBBY (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection, "Server: %s (LOBBY)",
                    detectedServer.c_str());
        g_lastDetectedModeName = "LOBBY (Server ID)";
        g_lastDetectionLogTick = now;
      }
    } else if (srvLower.find("mini") != std::string::npos ||
               srvLower.find("bed") != std::string::npos) {
      appearsToBeLobby = false;
      detectionReason = "Server ID (Game)";
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "GAME (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection, "Server: %s (GAME)",
                    detectedServer.c_str());
        g_lastDetectedModeName = "GAME (Server ID)";
        g_lastDetectionLogTick = now;
      }
    } else {
      if ((now - g_lastDetectionLogTick >= 10000) &&
          g_lastDetectedModeName != "UNKNOWN (Server ID)") {
        Logger::log(Config::DebugCategory::GameDetection,
                    "Server: %s (UNKNOWN)", detectedServer.c_str());
        g_lastDetectedModeName = "UNKNOWN (Server ID)";
        g_lastDetectionLogTick = now;
      }
    }
  }

  if (hasStrictGameKeywords &&
      detectionReason != "Scoreboard (Lobby Keyword)" &&
      detectionReason != "Server ID (Lobby)") {
    appearsToBeLobby = false;
    detectionReason = "Tab Keywords Priority (Game)";
  }

  bool detectedLobby = appearsToBeLobby;
  if (detectedLobby) {
    g_lobbyGraceTicks++;
  } else {
    g_lobbyGraceTicks = 0;
  }

  if (now - g_lastDetectionLogTick >= 3000) {
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Current Mode: %s | Reason: %s",
                  (detectedLobby ? "LOBBY" : "GAME"), detectionReason.c_str());
    }
    g_lastDetectionLogTick = now;
  }

  bool shouldBeInGame = g_inHypixelGame;
  if (g_lobbyGraceTicks >= 10 || g_explicitLobbySignal) {
    shouldBeInGame = false;
    g_explicitLobbySignal = false;
  } else if (g_lobbyGraceTicks == 0 && hasStrictGameKeywords) {
    shouldBeInGame = true;
  } else if (g_lobbyGraceTicks == 0 && !hasStrictGameKeywords) {
    shouldBeInGame = false;
  }

  if (shouldBeInGame != g_inHypixelGame) {
    bool transitionToGame = shouldBeInGame && !g_inHypixelGame;
    g_inHypixelGame = shouldBeInGame;
    if (g_inHypixelGame) {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Detected Hypixel GAME session (Confirmed) - RESETTING FOR "
                  "FRESH START");

      {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        g_playerStatsMap.clear();
      }
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_persistentStatsCache.clear();
      }
      {
        std::lock_guard<std::mutex> lock(g_uuidMapMutex);
        g_playerUuidMap.clear();
      }

      g_processedPlayers.clear();
      {
        std::lock_guard<std::mutex> lock(g_alertedMutex);
        g_alertedPlayers.clear();
      }
      g_chatPrintedPlayers.clear();
      g_manualPushedPlayers.clear();
      g_forceChatOutputPlayers.clear();

      // clear caches mmm
      Urchin::clearCache();
      Seraph::clearCache();

      if (Config::isGlobalDebugEnabled()) {
        Render::NotificationManager::getInstance()->add(
            "System", "Game Started: Stats Reset",
            Render::NotificationType::Success);
      }
      syncTeamColors();
    } else {
      Logger::log(Config::DebugCategory::GameDetection,
                  "Detected Hypixel LOBBY session (Confirmed)");
      if (Config::isGlobalDebugEnabled()) {
        Render::NotificationManager::getInstance()->add(
            "System", "Lobby Session Detected",
            Render::NotificationType::Warning);
      }
      resetGameCache();
    }
  }

  static jclass iterCls = nullptr, npiCls = nullptr, profCls = nullptr,
                uuidCls = nullptr, cctCls = nullptr, collCls = nullptr;
  static jmethodID m_iter = nullptr, m_has = nullptr, m_next = nullptr,
                   m_setDisp = nullptr;
  static jmethodID m_getProf = nullptr, m_getName = nullptr, m_getId = nullptr,
                   m_uuidToString = nullptr, cctInit = nullptr;
  static jfieldID f_gpName = nullptr;

  if (!iterCls) {
    collCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.Collection"));
    iterCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.Iterator"));
    npiCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.client.network.NetworkPlayerInfo"));
    profCls = (jclass)env->NewGlobalRef(
        lc->GetClass("com.mojang.authlib.GameProfile"));
    uuidCls = (jclass)env->NewGlobalRef(lc->GetClass("java.util.UUID"));
    cctCls = (jclass)env->NewGlobalRef(
        lc->GetClass("net.minecraft.util.ChatComponentText"));

    m_iter = env->GetMethodID(collCls, "iterator", "()Ljava/util/Iterator;");
    m_has = env->GetMethodID(iterCls, "hasNext", "()Z");
    m_next = env->GetMethodID(iterCls, "next", "()Ljava/lang/Object;");

    m_setDisp = env->GetMethodID(npiCls, "setDisplayName",
                                 "(Lnet/minecraft/util/IChatComponent;)V");
    if (!m_setDisp) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_setDisp = env->GetMethodID(npiCls, "func_178859_a",
                                   "(Lnet/minecraft/util/IChatComponent;)V");
    }
    if (!m_setDisp) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_setDisp = env->GetMethodID(npiCls, "a", "(Leu;)V");
    }
    m_getProf = env->GetMethodID(npiCls, "getGameProfile",
                                 "()Lcom/mojang/authlib/GameProfile;");
    if (!m_getProf) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getProf = env->GetMethodID(npiCls, "func_178845_a",
                                   "()Lcom/mojang/authlib/GameProfile;");
    }
    if (!m_getProf) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      m_getProf =
          env->GetMethodID(npiCls, "a", "()Lcom/mojang/authlib/GameProfile;");
    }

    m_getName = env->GetMethodID(profCls, "getName", "()Ljava/lang/String;");
    m_getId = env->GetMethodID(profCls, "getId", "()Ljava/util/UUID;");
    f_gpName = env->GetFieldID(profCls, "name", "Ljava/lang/String;");

    m_uuidToString =
        env->GetMethodID(uuidCls, "toString", "()Ljava/lang/String;");
    cctInit = env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
  }

  if (!iterCls || !m_iter || !m_has || !m_next) {
    env->DeleteLocalRef(col);
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
    return;
  }

  world =
      (mcObj && s_f_world) ? env->GetObjectField(mcObj, s_f_world) : nullptr;

  g_jCache.init(env);

  jclass worldCls = g_jCache.worldCls;
  jmethodID m_getSB = g_jCache.m_getScoreboard;
  jclass sbCls = g_jCache.sbCls;
  jmethodID m_getObj = g_jCache.m_getObjectiveInDisplaySlot;
  jmethodID m_getObjByName = g_jCache.m_getObjective;
  jmethodID m_getScore = g_jCache.m_getValueFromObjective;
  jclass scoreCls = g_jCache.scoreCls;
  jmethodID m_getVal = g_jCache.m_getScorePoints;
  jmethodID m_setVal = g_jCache.m_setScorePoints;
  f_gpName = g_jCache.f_gpName;

  std::string currentSortMode = Config::getSortMode();
  std::vector<std::string> currentNames;

  static ULONGLONG lastExtraction = 0;
  bool doExtraction = (now - lastExtraction >= 50);

  if (doExtraction && m_has && m_next) {
    lastExtraction = now;
    int processedCount = 0;
    iter = env->CallObjectMethod(col, m_iter);
    int extractionsThisFrame = 0;
    if (iter) {
      while (env->CallBooleanMethod(iter, m_has)) {
        if (lc->CheckException())
          break;
        if (env->PushLocalFrame(50) < 0)
          break;

        jobject info = env->CallObjectMethod(iter, m_next);
        if (info) {
          jobject prof =
              m_getProf ? env->CallObjectMethod(info, m_getProf) : nullptr;
          if (prof) {
            jstring jname =
                m_getName ? (jstring)env->CallObjectMethod(prof, m_getName)
                          : nullptr;
            if (jname) {
              const char *nameUtf = env->GetStringUTFChars(jname, 0);
              std::string name(nameUtf);
              bool isObfuscated = (name.find("\xC2\xA7k") != std::string::npos);
              while (true) {
                size_t pos = name.find("\xC2\xA7");
                if (pos == std::string::npos)
                  break;
                if (pos + 3 <= name.length()) {
                  name.erase(pos, 3);
                } else {
                  name.erase(pos);
                  break;
                }
              }
              if (!isObfuscated) {
                currentNames.push_back(name);
              }
              env->ReleaseStringUTFChars(jname, nameUtf);

              if (Config::isNickedBypass() && extractionsThisFrame < 4) {
                bool needsUuid = false;
                {
                  std::lock_guard<std::mutex> lock(g_uuidMapMutex);
                  needsUuid =
                      (g_playerUuidMap.find(name) == g_playerUuidMap.end());
                }
                if (needsUuid) {
                  jobject guid = env->CallObjectMethod(prof, m_getId);
                  if (guid) {
                    jstring jUuid =
                        (jstring)env->CallObjectMethod(guid, m_uuidToString);
                    if (jUuid) {
                      const char *uUtf = env->GetStringUTFChars(jUuid, 0);
                      if (uUtf) {
                        std::string uuidStr = uUtf;
                        env->ReleaseStringUTFChars(jUuid, uUtf);
                        {
                          std::lock_guard<std::mutex> lock(g_uuidMapMutex);
                          g_playerUuidMap[name] = uuidStr;
                        }
                        extractionsThisFrame++;
                      }
                      env->DeleteLocalRef(jUuid);
                    }
                    env->DeleteLocalRef(guid);
                  }
                }
              }
            }
          }
        }

        processedCount++;
        env->PopLocalFrame(nullptr);
        if (processedCount > 500)
          break; // sanity
      }
      if (iter)
        env->DeleteLocalRef(iter);
    }

    {
      std::lock_guard<std::mutex> lock(g_statsMutex);
      bool needsImmediateTeamSync = false;
      for (const auto &name : currentNames) {
        if (g_playerTeamColor.find(name) == g_playerTeamColor.end()) {
          needsImmediateTeamSync = true;
          break;
        }
      }

      if (!g_inPreGameLobby) {
        g_onlinePlayers = currentNames;
      }

      if (needsImmediateTeamSync && g_inHypixelGame) {
        // better team sync
        updateTeamsFromScoreboard();
        g_lastTeamScanTick = now;
      }
    }
  }

  if (doTabUpdate && m_has && m_next) {
    iter = env->CallObjectMethod(col, m_iter);
    if (!iter) {
      env->DeleteLocalRef(col);
      env->DeleteLocalRef(nh);
      env->DeleteLocalRef(mcObj);
      return;
    }

    {
      static bool s_sbDebugOnce = false;
      if (!s_sbDebugOnce && Config::isGlobalDebugEnabled()) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "§e[HEALTH-DBG] world=%p s_m_getSB=%p s_m_getObj=%p "
                 "s_f_world=%p mcObj=%p m_getSB(jcache)=%p",
                 world, s_m_getSB, s_m_getObj, s_f_world, mcObj,
                 g_jCache.m_getScoreboard);
        ChatSDK::showPrefixed(buf);
        s_sbDebugOnce = true;
      }
    }
    jobject scoreboard = (world && s_m_getSB)
                             ? env->CallObjectMethod(world, s_m_getSB)
                             : nullptr;
    env->ExceptionClear();

    jobject tabObj = nullptr;
    if (scoreboard && s_m_getObj) {
      tabObj = env->CallObjectMethod(scoreboard, s_m_getObj, 0);
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    if (env->ExceptionCheck())
      env->ExceptionClear();

    int processedTab = 0;
    while (env->CallBooleanMethod(iter, m_has)) {
      if (lc->CheckException())
        break;

      if (env->PushLocalFrame(100) < 0)
        break;

      jobject info = env->CallObjectMethod(iter, m_next);
      if (info && m_getProf && m_getName) {
        jobject prof = env->CallObjectMethod(info, m_getProf);
        if (prof) {
          jstring jn = (jstring)env->CallObjectMethod(prof, m_getName);
          if (jn) {
            const char *utf = env->GetStringUTFChars(jn, 0);
            std::string name(utf ? utf : "");
            if (utf)
              env->ReleaseStringUTFChars(jn, utf);

            while (true) {
              size_t pos = name.find("\xC2\xA7");
              if (pos == std::string::npos)
                break;
              if (pos + 3 <= name.length()) {
                name.erase(pos, 3);
              } else {
                name.erase(pos);
                break;
              }
            }

            if (forceReset || !Config::isTabEnabled() || !g_inHypixelGame) {
              if (m_setDisp)
                env->CallVoidMethod(info, m_setDisp, nullptr);
              if (f_gpName) {
                jstring orig = env->NewStringUTF(name.c_str());
                env->SetObjectField(prof, f_gpName, orig);
                env->DeleteLocalRef(orig);
              }
            } else if (cctInit && m_setDisp) {
              Hypixel::PlayerStats stats;
              bool hasStats = false;
              {
                std::lock_guard<std::mutex> lock(g_statsMutex);
                auto itS = g_playerStatsMap.find(name);
                if (itS != g_playerStatsMap.end()) {
                  stats = itS->second;
                  hasStats = true;
                }
              }

              std::string teamColorCode = "\xC2\xA7"
                                          "f";
              std::string currentTeam;
              std::string cName = name;

              {
                std::lock_guard<std::mutex> lock(g_statsMutex);
                auto itTC = g_playerTeamColor.find(name);
                if (itTC != g_playerTeamColor.end() && !itTC->second.empty()) {
                  currentTeam = itTC->second;
                  teamColorCode = mcColorForTeam(currentTeam);
                }
              }

              std::string sortMetric = Config::getSortMode();
              std::transform(sortMetric.begin(), sortMetric.end(),
                             sortMetric.begin(), ::tolower);
              double sortVal = 0;
              if (hasStats) {
                if (sortMetric == "fk")
                  sortVal = (double)stats.bedwarsFinalKills;
                else if (sortMetric == "fkdr")
                  sortVal = (stats.bedwarsFinalDeaths == 0)
                                ? (double)stats.bedwarsFinalKills
                                : (double)stats.bedwarsFinalKills /
                                      stats.bedwarsFinalDeaths;
                else if (sortMetric == "wins")
                  sortVal = (double)stats.bedwarsWins;
                else if (sortMetric == "wlr")
                  sortVal =
                      (stats.bedwarsLosses == 0)
                          ? (double)stats.bedwarsWins
                          : (double)stats.bedwarsWins / stats.bedwarsLosses;
                else if (sortMetric == "star")
                  sortVal = (double)stats.bedwarsStar;
                else if (sortMetric == "ws")
                  sortVal = (double)stats.winstreak;
              }
              if (sortMetric == "team") {
                if (currentTeam == "Red")
                  sortVal = 100;
                else if (currentTeam == "Blue")
                  sortVal = 200;
                else if (currentTeam == "Green")
                  sortVal = 300;
                else if (currentTeam == "Yellow")
                  sortVal = 400;
                else if (currentTeam == "Aqua")
                  sortVal = 500;
                else if (currentTeam == "White")
                  sortVal = 600;
                else if (currentTeam == "Pink")
                  sortVal = 700;
                else if (currentTeam == "Gray" || currentTeam == "Grey")
                  sortVal = 800;
                else
                  sortVal = 999;
              }

              long rank = (long)(sortVal * 10.0);
              if (Config::isTabSortDescending())
                rank = 9999L - rank;
              if (rank < 0)
                rank = 0;
              if (rank > 9999)
                rank = 9999;
              char rankBuf[8];
              sprintf_s(rankBuf, "%04ld", rank);
              std::string calculatedPrefix = "";
              for (int i = 0; i < 4; ++i) {
                calculatedPrefix += "\xC2\xA7";
                calculatedPrefix += rankBuf[i];
              }

              std::string finalPrefix = calculatedPrefix;
              {
                std::lock_guard<std::mutex> lockR(g_stableRankMutex);
                auto itR = g_stableRankMap.find(name);
                if (hasStats) {
                  g_stableRankMap[name] = calculatedPrefix;
                  finalPrefix = calculatedPrefix;
                } else if (itR != g_stableRankMap.end()) {
                  finalPrefix = itR->second;
                } else if (processedTab > 5 && !currentTeam.empty()) {
                  g_stableRankMap[name] = calculatedPrefix;
                  finalPrefix = calculatedPrefix;
                }
              }

              std::string internalName = finalPrefix + teamColorCode + cName;
              if (f_gpName) {
                if (internalName.length() > 40) {
                  internalName = teamColorCode + cName;
                  if (internalName.length() > 40)
                    internalName = internalName.substr(0, 40);
                }
                jstring newNameObj = env->NewStringUTF(internalName.c_str());
                if (newNameObj) {
                  env->SetObjectField(prof, f_gpName, newNameObj);
                  if (env->ExceptionCheck())
                    env->ExceptionClear();
                  env->DeleteLocalRef(newNameObj);
                }
              }

              std::string fullTabString;
              if (hasStats) {
                if (stats.isNicked) {
                  fullTabString = teamColorCode + name +
                                  " \xC2\xA7"
                                  "4[NICKED]";
                } else {
                  fullTabString =
                      BedwarsStars::GetFormattedLevel(stats.bedwarsStar) + " " +
                      teamColorCode + name;
                  if (Config::isTagsEnabled())
                    fullTabString += stats.tagsDisplay;
                  fullTabString += " \xC2\xA7"
                                   "7: ";

                  std::string dMode = Config::getTabDisplayMode();
                  std::transform(dMode.begin(), dMode.end(), dMode.begin(),
                                 ::tolower);
                  if (dMode == "fk")
                    fullTabString += fullTabString +=
                        StatColors::getMcColor(
                            StatColors::StatType::FinalKills,
                            (double)stats.bedwarsFinalKills) +
                        std::to_string(stats.bedwarsFinalKills);
                  else if (dMode == "fkdr") {
                    double fkdr = (stats.bedwarsFinalDeaths == 0)
                                      ? (double)stats.bedwarsFinalKills
                                      : (double)stats.bedwarsFinalKills /
                                            stats.bedwarsFinalDeaths;
                    std::ostringstream ss_fkdr;
                    ss_fkdr << std::fixed << std::setprecision(2) << fkdr;
                    fullTabString += StatColors::getMcColor(
                                         StatColors::StatType::FKDR, fkdr) +
                                     ss_fkdr.str();
                  } else if (dMode == "wins")
                    fullTabString +=
                        StatColors::getMcColor(StatColors::StatType::Wins,
                                               (double)stats.bedwarsWins) +
                        std::to_string(stats.bedwarsWins);
                  else if (dMode == "wlr") {
                    double wlr =
                        (stats.bedwarsLosses == 0)
                            ? (double)stats.bedwarsWins
                            : (double)stats.bedwarsWins / stats.bedwarsLosses;
                    std::ostringstream ss_wlr;
                    ss_wlr << std::fixed << std::setprecision(2) << wlr;
                    fullTabString +=
                        StatColors::getMcColor(StatColors::StatType::WLR, wlr) +
                        ss_wlr.str();
                  } else if (dMode == "star" || dMode == "lvl")
                    fullTabString += "\xC2\xA7"
                                     "6" +
                                     std::to_string(stats.bedwarsStar) +
                                     "\xC2\xA7"
                                     "e\xE2\x9C\xAF";
                  else if (dMode == "ws")
                    fullTabString += "\xC2\xA7"
                                     "d" +
                                     std::to_string(stats.winstreak) + " WS";
                  else if (dMode == "team")
                    fullTabString += currentTeam.empty()
                                         ? "\xC2\xA7"
                                           "7None"
                                         : teamColorCode + currentTeam;
                }
              } else {
                fullTabString = teamColorCode + name;
              }

              jstring jf = env->NewStringUTF(fullTabString.c_str());
              jobject component =
                  (jf) ? env->NewObject(cctCls, cctInit, jf) : nullptr;
              if (component && m_setDisp)
                env->CallVoidMethod(info, m_setDisp, component);
              if (jf)
                env->DeleteLocalRef(jf);
              if (component)
                env->DeleteLocalRef(component);

              // === HEALTH SYNC DEBUG ===
              {
                static ULONGLONG lastHealthDbg = 0;
                bool doHealthDbg = Config::isGlobalDebugEnabled() &&
                                   (now - lastHealthDbg > 3000) &&
                                   processedTab == 0;

                if (doHealthDbg) {
                  Logger::log(Config::DebugCategory::GameDetection,
                              "HealthSync check: scoreboard=%p tabObj=%p "
                              "m_getScore=%p m_getVal=%p m_setVal=%p",
                              scoreboard, tabObj, m_getScore, m_getVal,
                              m_setVal);
                }

                if (scoreboard && tabObj && m_getScore && m_getVal &&
                    m_setVal) {
                  jstring oldNameJ = env->NewStringUTF(name.c_str());
                  int scoreVal = 0;
                  bool scoreFound = false;
                  if (oldNameJ) {
                    jobject oldScore = env->CallObjectMethod(
                        scoreboard, m_getScore, oldNameJ, tabObj);
                    if (env->ExceptionCheck())
                      env->ExceptionClear();
                    if (oldScore) {
                      scoreVal = env->CallIntMethod(oldScore, m_getVal);
                      if (scoreVal > 0)
                        scoreFound = true;
                      if (doHealthDbg) {
                        Logger::log(
                            Config::DebugCategory::GameDetection,
                            "HealthSync READ: name=%s scoreVal=%d found=%d",
                            name.c_str(), scoreVal, scoreFound ? 1 : 0);
                      }
                      env->DeleteLocalRef(oldScore);
                    } else {
                      if (doHealthDbg) {
                        Logger::log(
                            Config::DebugCategory::GameDetection,
                            "HealthSync READ: oldScore is NULL for name=%s",
                            name.c_str());
                      }
                    }
                    env->DeleteLocalRef(oldNameJ);
                  }

                  if (scoreFound) {
                    jstring newNameJ = env->NewStringUTF(internalName.c_str());
                    if (newNameJ) {
                      jobject newScore = env->CallObjectMethod(
                          scoreboard, m_getScore, newNameJ, tabObj);
                      if (env->ExceptionCheck())
                        env->ExceptionClear();
                      if (newScore) {
                        env->CallVoidMethod(newScore, m_setVal, scoreVal);
                        if (env->ExceptionCheck()) {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync WRITE: EXCEPTION on "
                                        "setScorePoints!");
                          env->ExceptionClear();
                        } else {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync WRITE: OK val=%d",
                                        scoreVal);
                        }
                        env->DeleteLocalRef(newScore);
                      } else {
                        if (doHealthDbg)
                          Logger::log(Config::DebugCategory::GameDetection,
                                      "HealthSync WRITE: newScore is NULL");
                      }
                      if (g_jCache.m_onScoreUpdated) {
                        env->CallVoidMethod(
                            scoreboard, g_jCache.m_onScoreUpdated, newNameJ);
                        if (env->ExceptionCheck()) {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync BROADCAST: EXCEPTION!");
                          env->ExceptionClear();
                        } else {
                          if (doHealthDbg)
                            Logger::log(Config::DebugCategory::GameDetection,
                                        "HealthSync BROADCAST: OK");
                        }
                      } else {
                        if (doHealthDbg)
                          Logger::log(
                              Config::DebugCategory::GameDetection,
                              "HealthSync BROADCAST: m_onScoreUpdated is NULL");
                      }
                      env->DeleteLocalRef(newNameJ);
                    }
                  } else {
                    if (doHealthDbg) {
                      Logger::log(Config::DebugCategory::GameDetection,
                                  "HealthSync: score NOT found for %s (val=%d)",
                                  name.c_str(), scoreVal);
                    }
                  }
                } else if (doHealthDbg) {
                  Logger::log(Config::DebugCategory::GameDetection,
                              "HealthSync SKIP: sb=%d tabObj=%d getScore=%d "
                              "getVal=%d setVal=%d",
                              scoreboard ? 1 : 0, tabObj ? 1 : 0,
                              m_getScore ? 1 : 0, m_getVal ? 1 : 0,
                              m_setVal ? 1 : 0);
                }

                if (doHealthDbg)
                  lastHealthDbg = now;
              }
            } else {
              if (m_setDisp)
                env->CallVoidMethod(info, m_setDisp, nullptr);
            }
          }
        }
      }

      if (lc->CheckException()) {
        env->ExceptionClear();
      }

      env->PopLocalFrame(nullptr);
      processedTab++;
      if (processedTab > 500)
        break; // sanity
    }
    if (scoreboard)
      env->DeleteLocalRef(scoreboard);
    if (tabObj)
      env->DeleteLocalRef(tabObj);

    if (!currentNames.empty() && g_inHypixelGame) {
      if (!g_manualPushedPlayers.empty()) {
        for (const auto &mName : g_manualPushedPlayers) {
          if (std::find(currentNames.begin(), currentNames.end(), mName) ==
              currentNames.end()) {
            currentNames.push_back(mName);
          }
        }
      }

      bool changed = false;
      {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        if (currentNames.size() != g_onlinePlayers.size()) {
          changed = true;
        } else {
          for (size_t i = 0; i < currentNames.size(); ++i) {
            if (currentNames[i] != g_onlinePlayers[i]) {
              changed = true;
              break;
            }
          }
        }
        if (changed) {
          g_onlinePlayers = currentNames;
          g_nextFetchIdx = 0;
        }
      }
    }
  }

  if (iter)
    env->DeleteLocalRef(iter);
  env->DeleteLocalRef(col);
  env->DeleteLocalRef(nh);
  if (world)
    env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

void syncTeamColors() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  g_jCache.init(env);

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
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
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
  jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

  if (world) {
    jmethodID m_getScoreboard = g_jCache.m_getScoreboard;
    jobject scoreboard = m_getScoreboard
                             ? env->CallObjectMethod(world, m_getScoreboard)
                             : nullptr;
    env->ExceptionClear();
    if (scoreboard) {
      if (g_jCache.m_getPlayersTeam && g_jCache.m_getPrefix) {
        std::vector<std::string> namesToSync;
        {
          std::lock_guard<std::mutex> stLock(g_statsMutex);
          for (const auto &pair : g_playerStatsMap) {
            namesToSync.push_back(pair.first);
          }
        }

        std::unordered_map<std::string, std::string> resolvedTeams;
        for (const auto &name : namesToSync) {
          std::string team = resolveTeamForNameEx(
              env, name, scoreboard, g_jCache.m_getPlayersTeam,
              g_jCache.teamCls, g_jCache.m_getPrefix);
          if (!team.empty()) {
            resolvedTeams[name] = team;
          }
        }

        {
          std::lock_guard<std::mutex> stLock2(g_statsMutex);
          for (const auto &name : namesToSync) {
            auto it = resolvedTeams.find(name);
            std::string team;
            if (it != resolvedTeams.end()) {
              team = it->second;
            } else {
              auto itT = g_playerTeamColor.find(name);
              if (itT != g_playerTeamColor.end())
                team = itT->second;
            }

            if (!team.empty()) {
              auto existingIt = g_playerTeamColor.find(name);
              bool downgrade = (existingIt != g_playerTeamColor.end() &&
                                isRealBedwarsTeam(existingIt->second) &&
                                (team == "Gray" || team == "Grey"));
              if (!downgrade) {
                auto statIt = g_playerStatsMap.find(name);
                if (statIt != g_playerStatsMap.end()) {
                  statIt->second.teamColor = team;
                }
              }
              setTeamColorSticky(name, team);
            }
          }
        }
      }
      env->DeleteLocalRef(scoreboard);
    }
    env->DeleteLocalRef(world);
  }
  env->DeleteLocalRef(mcObj);
}

#pragma warning(push)
#pragma warning(disable : 26110 26117)
void syncTags() {
  if (!Config::isTagsEnabled())
    return;

  std::string activeS = Config::getActiveTagService();
  auto getAbbr = [](const std::string &raw) -> std::string {
    std::string t = raw;
    for (auto &c : t)
      c = toupper(c);
    if (t.find("BLATANT") != std::string::npos)
      return "\xC2\xA7"
             "4[BC]";
    if (t.find("CLOSET") != std::string::npos)
      return "\xC2\xA7"
             "4[CC]";
    if (t.find("CHEATER") != std::string::npos)
      return "\xC2\xA7"
             "5[C]";
    if (t.find("CONFIRMED") != std::string::npos)
      return "\xC2\xA7"
             "5[C]";
    if (t.find("CAUTION") != std::string::npos)
      return "\xC2\xA7"
             "e[!]";
    if (t.find("SUSPICIOUS") != std::string::npos)
      return "\xC2\xA7"
             "6[?]";
    if (t.find("SNIPER") != std::string::npos)
      return "\xC2\xA7"
             "6[S]";
    return "";
  };

  std::vector<std::pair<std::string, std::string>> playersNeedingTags;
  {
    std::lock_guard<std::mutex> tagLock(g_statsMutex);
    for (auto &pair : g_playerStatsMap) {
      bool needsUrchin = (activeS == "Urchin" || activeS == "Both");
      bool needsSeraph = (activeS == "Seraph" || activeS == "Both");

      for (const auto &rt : pair.second.rawTags) {
        if (rt.find("URCHIN") == 0)
          needsUrchin = false;
        if (rt.find("SERAPH") == 0)
          needsSeraph = false;
      }

      if (needsUrchin || needsSeraph) {
        playersNeedingTags.push_back({pair.first, pair.second.uuid});
      }
    }
  }

  if (playersNeedingTags.empty())
    return;

  std::vector<std::tuple<std::string, std::string, std::vector<std::string>>>
      updates;
  for (const auto &p : playersNeedingTags) {
    std::string tagStr;
    std::vector<std::string> rTags;
    bool foundAny = false;

    bool needsUrchin = (activeS == "Urchin" || activeS == "Both");
    bool needsSeraph = (activeS == "Seraph" || activeS == "Both");
    {
      std::lock_guard<std::mutex> lock(g_statsMutex);
      auto itS = g_playerStatsMap.find(p.first);
      if (itS != g_playerStatsMap.end()) {
        for (const auto &rt : itS->second.rawTags) {
          if (rt.find("URCHIN") == 0)
            needsUrchin = false;
          if (rt.find("SERAPH") == 0)
            needsSeraph = false;
        }
      }
    }

    if (activeS == "Urchin" || activeS == "Both") {
      if (needsUrchin) {
        auto uT = Urchin::getPlayerTags(p.first);
        rTags.push_back("URCHIN_CHECKED");
        foundAny = true;
        if (uT && !uT->tags.empty()) {
          std::string a = getAbbr(uT->tags[0].type);
          tagStr += " " + (a.empty() ? "\xC2\xA7"
                                       "4[U]"
                                     : a);
          for (const auto &t : uT->tags)
            rTags.push_back("URCHIN:" + t.type);
        }
      }
    }
    if ((activeS == "Seraph" || activeS == "Both") && !p.second.empty()) {
      if (needsSeraph) {
        auto sT = Seraph::getPlayerTags(p.first, p.second);
        rTags.push_back("SERAPH_CHECKED");
        foundAny = true;
        if (sT && !sT->tags.empty()) {
          std::string a = getAbbr(sT->tags[0].type);
          tagStr += " " + (a.empty() ? "\xC2\xA7"
                                       "4[S]"
                                     : a);
          for (const auto &t : sT->tags)
            rTags.push_back("SERAPH:" + t.type);
        }
      }
    }

    if (foundAny) {
      updates.push_back({p.first, tagStr, rTags});
    }
  }

  if (!updates.empty()) {
    std::lock_guard<std::mutex> tagLock2(g_statsMutex);
    for (const auto &u : updates) {
      auto it = g_playerStatsMap.find(std::get<0>(u));
      if (it != g_playerStatsMap.end()) {
        for (const auto &newTag : std::get<2>(u)) {
          bool exists = false;
          for (const auto &oldTag : it->second.rawTags) {
            if (oldTag == newTag) {
              exists = true;
              break;
            }
          }
          if (!exists)
            it->second.rawTags.push_back(newTag);
        }

        std::string newDisplay = "";
        std::set<std::string> seenTags;

        for (const auto &rt : it->second.rawTags) {
          if (seenTags.find(rt) != seenTags.end())
            continue;
          seenTags.insert(rt);

          if (rt.find("URCHIN:") == 0) {
            std::string type = rt.substr(7);
            std::string abbr = getAbbr(type);
            newDisplay += " " + (abbr.empty() ? ("\xC2\xA7"
                                                 "7[" +
                                                 type + "]")
                                              : abbr);
          } else if (rt.find("SERAPH:") == 0) {
            std::string type = rt.substr(7);
            std::string abbr = getAbbr(type);
            newDisplay += " " + (abbr.empty() ? ("\xC2\xA7"
                                                 "7[" +
                                                 type + "]")
                                              : abbr);
          }
        }
        it->second.tagsDisplay = newDisplay;
      }
    }
  }
}
#pragma warning(pop)

} // namespace OVson
