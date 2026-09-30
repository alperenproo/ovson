#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.h"
#include "StatsTracker.internal.h"

#include "../Chat/ChatSDK.h"
#include "../Chat/Commands.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/ChatBypasser.h"
#include "../Utils/Logger.h"
#include "../Utils/NumberDenicker.h"
#include "../Utils/ColoredHitboxes.h"

#include <Windows.h>
#include <cctype>
#include "../Chat/ChatHook.h"
#include "FootstepMuter.h"

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace OVson {

bool g_initialized = false;
int g_mode = 0;
bool g_inHypixelGame = false;
bool g_inPreGameLobby = false;
bool g_inReplay = false;

std::string g_lastOnlineLine;
std::vector<std::string> g_onlinePlayers;
size_t g_nextFetchIdx = 0;
std::string g_logsDir;
std::string g_logFilePath;
HANDLE g_logHandle = INVALID_HANDLE_VALUE;
long long g_logOffset = 0;
std::string g_logBuf;

std::unordered_map<std::string, std::string> g_pendingTabNames;
std::unordered_map<std::string, std::string> g_stableRankMap;
std::mutex g_stableRankMutex;

bool g_lastInGameStatus = false;
std::string g_lastDetectedModeName;
int g_lobbyGraceTicks = 0;
bool g_explicitLobbySignal = false;
ULONGLONG g_lastImmediateTeamProbeTick = 0;
ULONGLONG g_lastTeamScanTick = 0;
ULONGLONG g_lastChatReadTick = 0;
ULONGLONG g_lastResetTick = 0;
ULONGLONG g_lastDetectionLogTick = 0;
ULONGLONG g_bootstrapStartTick = 0;
ULONGLONG g_preGameDetectTick = 0;
std::string g_localTeam;
std::string g_localName;
bool g_isNicked = false;
std::string g_activeNick;
std::string g_realUsername;

std::string getRealLocalUsername(bool forceRefresh) {
  if (!forceRefresh && !g_realUsername.empty())
    return g_realUsername;

  JNIEnv *env = lc ? lc->getEnv() : nullptr;
  if (!env)
    return g_realUsername.empty() ? g_localName : g_realUsername;

  std::string detectedName;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (mcCls) {
    jmethodID getMcM = lc->GetStaticMethodID(
        mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (getMcM) {
      jobject mc = env->CallStaticObjectMethod(mcCls, getMcM);
      if (mc) {
        jclass mcClass = env->GetObjectClass(mc);

        jfieldID playerField = lc->GetFieldID(
            mcClass, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
            "field_71439_g", "h", "Lbew;");
        if (playerField) {
          jobject player = env->GetObjectField(mc, playerField);
          if (player) {
            jclass playerClass = env->GetObjectClass(player);
            jmethodID getProfile = lc->GetMethodID(
                playerClass, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
                "func_146103_bH", "eQ");
            if (!getProfile) {
              getProfile = lc->GetMethodID(
                  playerClass, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
                  "func_146103_bH", "bH");
            }
            if (getProfile) {
              jobject prof = env->CallObjectMethod(player, getProfile);
              if (prof) {
                jclass profClass = env->GetObjectClass(prof);
                jmethodID getProfName = lc->GetMethodID(profClass, "getName", "()Ljava/lang/String;");
                env->DeleteLocalRef(profClass);
                if (getProfName) {
                  jstring js = (jstring)env->CallObjectMethod(prof, getProfName);
                  if (js) {
                    const char *utf = env->GetStringUTFChars(js, nullptr);
                    if (utf && strlen(utf) > 0) {
                      detectedName = utf;
                      env->ReleaseStringUTFChars(js, utf);
                    }
                    env->DeleteLocalRef(js);
                  }
                }
                env->DeleteLocalRef(prof);
              }
            }
            if (detectedName.empty()) {
              const char *nameMethods[] = {"getName", "func_70005_c_", "e_", "h_", "f_", "g_", "i_", "j_", "k_", nullptr};
              for (int i = 0; nameMethods[i]; i++) {
                jmethodID m_getName = env->GetMethodID(playerClass, nameMethods[i], "()Ljava/lang/String;");
                if (env->ExceptionCheck()) env->ExceptionClear();
                else if (m_getName) {
                  jstring js = (jstring)env->CallObjectMethod(player, m_getName);
                  if (js) {
                    const char *utf = env->GetStringUTFChars(js, nullptr);
                    if (utf && strlen(utf) > 0) {
                      detectedName = utf;
                      env->ReleaseStringUTFChars(js, utf);
                    }
                    env->DeleteLocalRef(js);
                  }
                  break;
                }
              }
            }
            env->DeleteLocalRef(playerClass);
            env->DeleteLocalRef(player);
          }
        }

        if (detectedName.empty()) {
          jobject sess = nullptr;
          jmethodID getSessM = lc->GetMethodID(
              mcClass, "getSession", "()Lnet/minecraft/util/Session;",
              "func_110432_I", "L", "()Lavm;");
          if (getSessM) {
            sess = env->CallObjectMethod(mc, getSessM);
          }
          if (!sess) {
            jfieldID sessField = lc->GetFieldID(
                mcClass, "session", "Lnet/minecraft/util/Session;",
                "field_71449_j", "ae", "Lavm;");
            if (sessField) {
              sess = env->GetObjectField(mc, sessField);
            }
          }
          if (sess) {
            jclass sessClass = env->GetObjectClass(sess);
            jmethodID getUsername = lc->GetMethodID(
                sessClass, "getUsername", "()Ljava/lang/String;", "func_111285_a", "c");
            if (!getUsername) {
              const char *userMethods[] = {"getUsername", "func_111285_a", "c", nullptr};
              for (int i = 0; userMethods[i]; i++) {
                getUsername = env->GetMethodID(sessClass, userMethods[i], "()Ljava/lang/String;");
                if (env->ExceptionCheck()) env->ExceptionClear();
                else if (getUsername) break;
              }
            }
            env->DeleteLocalRef(sessClass);
            if (getUsername) {
              jstring js = (jstring)env->CallObjectMethod(sess, getUsername);
              if (js) {
                const char *utf = env->GetStringUTFChars(js, nullptr);
                if (utf && strlen(utf) > 0) {
                  detectedName = utf;
                  env->ReleaseStringUTFChars(js, utf);
                }
                env->DeleteLocalRef(js);
              }
            }
            env->DeleteLocalRef(sess);
          }
        }

        env->DeleteLocalRef(mcClass);
        env->DeleteLocalRef(mc);
      }
    }
  }

  if (!detectedName.empty()) {
    g_realUsername = detectedName;
    g_localName = detectedName;
    return detectedName;
  }

  if (g_realUsername.empty() && !g_localName.empty() && (g_activeNick.empty() || g_localName != g_activeNick)) {
    g_realUsername = g_localName;
  }

  return g_realUsername;
}

std::string getRealLocalUsername() {
  return getRealLocalUsername(false);
}

std::unordered_map<std::string, ULONGLONG> g_autoStatsCooldowns;
std::unordered_map<std::string, int> g_teamProbeTries;
bool g_teamReportSent = false;

std::unordered_set<std::string> g_processedPlayers;
std::unordered_set<std::string> g_queuedPlayers;
std::unordered_set<std::string> g_alertedPlayers;
std::mutex g_alertedMutex;
std::mutex g_queueMutex;
std::unordered_set<std::string> g_activeFetches;
std::mutex g_activeFetchesMutex;
std::vector<std::string> g_manualPushedPlayers;
std::unordered_set<std::string> g_forceChatOutputPlayers;
std::unordered_set<std::string> g_chatPrintedPlayers;
std::unordered_map<std::string, ULONGLONG> g_retryUntil;
std::mutex g_retryMutex;
std::unordered_map<std::string, int> g_playerFetchRetries;
std::unordered_map<std::string, int> g_player500Retries;
std::unordered_set<std::string> g_eliminatedPlayers;
std::mutex g_eliminatedMutex;
std::unordered_map<std::string, std::string> g_playerUuidMap;
std::mutex g_uuidMapMutex;

std::unordered_map<std::string, CachedStats> g_persistentStatsCache;
std::mutex g_cacheMutex;

JCache g_jCache;

std::unordered_map<std::string, Hypixel::PlayerStats> g_playerStatsMap;
std::recursive_mutex g_statsMutex;

std::unordered_map<std::string, std::string> g_nickToRealMap;
std::mutex g_nickMapMutex;

std::unordered_map<std::string, std::string> g_playerTeamColor;
std::unordered_map<std::string, Hypixel::PlayerStats> g_pendingStatsMap;
std::mutex g_pendingStatsMutex;
float g_jniLatency = 0.0f;
float g_apiLatency = 0.0f;
float g_scanSpeed = 0.0f;
std::string g_tabFooterText;
std::mutex g_footerMutex;

void initialize() {
  g_initialized = true;
  g_bootstrapStartTick = (ULONGLONG)GetTickCount64();
}

void shutdown() {
  if (!g_initialized)
    return;
  g_initialized = false;
  {
    std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
    g_playerStatsMap.clear();
  }
  g_onlinePlayers.clear();
  g_processedPlayers.clear();
  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_queuedPlayers.clear();
  }
  g_activeFetches.clear();
  g_playerTeamColor.clear();

  JNIEnv *env = lc->getEnv();
  if (env) {
    FootstepMuter::cleanup(env);
    g_jCache.cleanup(env);
  }
}

void setMode(int mode) { g_mode = mode; }

bool isInGame(const std::string &name) {
  std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
  return g_playerStatsMap.count(name) > 0;
}

bool shouldAlert(const std::string &name) {
  std::lock_guard<std::mutex> lock(g_alertedMutex);
  if (g_alertedPlayers.count(name))
    return false;
  g_alertedPlayers.insert(name);
  return true;
}

bool isInHypixelGame() { return g_inHypixelGame; }
bool isInPreGameLobby() { return g_inPreGameLobby; }
bool isInReplay() { return g_inReplay; }
bool shouldAutoFetchTags() {
  return Config::isTagsEnabled() &&
         (g_inPreGameLobby || g_inHypixelGame);
}
int getGameMode() { return g_mode; }
float getApiLatency() { return g_apiLatency; }
float getScanSpeed() { return g_scanSpeed; }

int getProcessedCount() { return (int)g_processedPlayers.size(); }
int getActiveFetchCount() {
  std::lock_guard<std::mutex> lock(g_activeFetchesMutex);
  return (int)g_activeFetches.size();
}
int getPendingStatsCount() {
  std::lock_guard<std::mutex> lock(g_pendingStatsMutex);
  return (int)g_pendingStatsMap.size();
}

void clearAllCaches() {
  {
    NumberDenicker::onWorldChange();
    std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
    std::lock_guard<std::mutex> nLock(g_nickMapMutex);
    g_playerStatsMap.clear();
    g_nickToRealMap.clear();
  }
  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_persistentStatsCache.clear();
  }
  {
    std::lock_guard<std::mutex> lock(g_uuidMapMutex);
    g_playerUuidMap.clear();
  }
  {
    std::lock_guard<std::mutex> qlock(g_queueMutex);
    g_onlinePlayers.clear();
    g_processedPlayers.clear();
    g_queuedPlayers.clear();
  }
  {
    std::lock_guard<std::mutex> aLock(g_activeFetchesMutex);
    g_activeFetches.clear();
  }
  {
    std::lock_guard<std::mutex> pLock(g_pendingStatsMutex);
    g_pendingStatsMap.clear();
  }
  {
    std::lock_guard<std::mutex> rLock(g_retryMutex);
    g_retryUntil.clear();
    g_playerFetchRetries.clear();
    g_player500Retries.clear();
  }
  {
    std::lock_guard<std::mutex> lock(g_alertedMutex);
    g_alertedPlayers.clear();
  }
  g_playerTeamColor.clear();
  g_localTeam.clear();
  g_helmetTeamSet.clear();
  g_manualPushedPlayers.clear();
  g_forceChatOutputPlayers.clear();
  g_chatPrintedPlayers.clear();
  g_stableRankMap.clear();
  {
    std::lock_guard<std::mutex> lockE(g_eliminatedMutex);
    g_eliminatedPlayers.clear();
  }
  Urchin::clearCache();
  Seraph::clearCache();
  OVson::Utils::clearHitboxColorCache();
  Logger::log(Config::DebugCategory::General,
              "All player caches cleared via OVson::clearAllCaches.");
}

bool handleEnterKeyPress() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return false;

  jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                           "Lnet/minecraft/client/Minecraft;",
                                           "field_71432_P", "S", "Lave;");
  if (!mcObj)
    return false;

  jobject screen = lc->GetObjectField(mcObj, "currentScreen",
                                      "Lnet/minecraft/client/gui/GuiScreen;",
                                      "field_71462_r", "m", "Laxu;");
  if (!screen) {
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jclass screenCls = env->GetObjectClass(screen);
  jfieldID f_input = lc->GetFieldID(screenCls, "inputField",
                                    "Lnet/minecraft/client/gui/GuiTextField;",
                                    "field_146415_a", "a", "Lavw;");
  if (!f_input) {
    f_input = lc->FindFieldBySignature(
        screenCls, "Lnet/minecraft/client/gui/GuiTextField;");
    if (!f_input)
      f_input = lc->FindFieldBySignature(screenCls, "Lavw;");
  }
  if (!f_input) {
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jobject input = env->GetObjectField(screen, f_input);
  if (!input) {
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jclass tfCls = env->GetObjectClass(input);
  jmethodID getText = lc->GetMethodID(tfCls, "getText", "()Ljava/lang/String;",
                                      "func_146179_b", "b");
  if (!getText) {
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jstring jtxt = (jstring)env->CallObjectMethod(input, getText);
  if (!jtxt) {
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  const char *utf = env->GetStringUTFChars(jtxt, 0);
  std::string text = utf ? utf : "";
  if (utf)
    env->ReleaseStringUTFChars(jtxt, utf);

  if (text.empty()) {
    env->DeleteLocalRef(jtxt);
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  size_t pos = 0;
  while (pos < text.size()) {
    unsigned char c = (unsigned char)text[pos];
    if (c <= 32) {
      pos++;
      continue;
    }
    if (c == 0xA7) {
      pos += 2;
      continue;
    }
    break;
  }

  const std::string &cp = ::Config::getCommandPrefix();
  bool isCommandPrefix = (!cp.empty() && text.substr(pos, cp.length()) == cp);
  if (isCommandPrefix && pos + cp.length() < text.size() &&
      text.substr(pos + cp.length(), cp.length()) == cp) {
    isCommandPrefix = false;
  }

  bool isIrcPrefix = (Config::isIrcEnabled() && pos < text.size() && text[pos] == '@');
  if (isIrcPrefix && pos + 1 < text.size() && text[pos + 1] == '@') {
    isIrcPrefix = false;
  }

  if (pos < text.size() && ((isCommandPrefix && Config::isCommandsEnabled()) || isIrcPrefix)) {
    std::string cmdText = text.substr(pos);

    jmethodID mSetText = lc->GetMethodID(
        tfCls, "setText", "(Ljava/lang/String;)V", "func_146180_a", "a");
    if (mSetText) {
      jstring empty = env->NewStringUTF("");
      env->CallVoidMethod(input, mSetText, empty);
      env->DeleteLocalRef(empty);
    }

    ChatHook::onClientSendMessage(cmdText);

    jmethodID mDisplay = lc->GetMethodID(
        mcCls, "displayGuiScreen", "(Lnet/minecraft/client/gui/GuiScreen;)V",
        "func_147108_a", "a", "(Laxu;)V");
    if (mDisplay)
      env->CallVoidMethod(mcObj, mDisplay, nullptr);

    env->DeleteLocalRef(jtxt);
    env->DeleteLocalRef(tfCls);
    env->DeleteLocalRef(input);
    env->DeleteLocalRef(screenCls);
    env->DeleteLocalRef(screen);
    env->DeleteLocalRef(mcObj);
    return true;
  }

  if (Config::isChatBypasserEnabled()) {
    std::string textFromPos = text.substr(pos);
    static const std::vector<std::pair<std::string, int>> bypassableCommands = {
        {"/shout ", 1}, {"/ac ", 1}, {"/pc ", 1},  {"/msg ", 2},
        {"/r ", 1},     {"/w ", 2},  {"/tell ", 2}};

    bool handledAsBypass = false;
    std::string bypassedText;

    if (!textFromPos.empty() && textFromPos[0] == '/') {
      for (const auto &cmdPair : bypassableCommands) {
        if (textFromPos.size() >= cmdPair.first.size()) {
          std::string lower = textFromPos.substr(0, cmdPair.first.size());
          for (auto &ch : lower)
            ch = (char)tolower((unsigned char)ch);

          if (lower == cmdPair.first) {
            size_t currentPos = 0;
            for (int i = 0; i < cmdPair.second; ++i) {
              currentPos = textFromPos.find_first_not_of(" ", currentPos);
              if (currentPos == std::string::npos)
                break;
              currentPos = textFromPos.find_first_of(" ", currentPos);
              if (currentPos == std::string::npos)
                break;
            }

            std::string prefix =
                text.substr(0, pos) + textFromPos.substr(0, currentPos);
            std::string msgPart = (currentPos != std::string::npos)
                                      ? textFromPos.substr(currentPos)
                                      : "";
            bypassedText = prefix + (Config::isSmartChatBypassEnabled()
                                         ? ChatBypasser::smartProcess(msgPart)
                                         : ChatBypasser::process(msgPart));
            handledAsBypass = true;
            break;
          }
        }
      }
    } else {
      bypassedText = Config::isSmartChatBypassEnabled()
                         ? ChatBypasser::smartProcess(text)
                         : ChatBypasser::process(text);
      handledAsBypass = true;
    }

    if (handledAsBypass) {
      jmethodID mSetText = lc->GetMethodID(
          tfCls, "setText", "(Ljava/lang/String;)V", "func_146180_a", "a");
      if (mSetText) {
        jstring empty = env->NewStringUTF("");
        env->CallVoidMethod(input, mSetText, empty);
        env->DeleteLocalRef(empty);

        ChatSDK::sendClientChat(bypassedText);

        jmethodID mDisplay =
            lc->GetMethodID(mcCls, "displayGuiScreen",
                            "(Lnet/minecraft/client/gui/GuiScreen;)V",
                            "func_147108_a", "a", "(Laxu;)V");
        if (mDisplay)
          env->CallVoidMethod(mcObj, mDisplay, nullptr);

        env->DeleteLocalRef(jtxt);
        env->DeleteLocalRef(tfCls);
        env->DeleteLocalRef(input);
        env->DeleteLocalRef(screenCls);
        env->DeleteLocalRef(screen);
        env->DeleteLocalRef(mcObj);
        return true;
      }
    }
  }

  env->DeleteLocalRef(jtxt);
  env->DeleteLocalRef(tfCls);
  env->DeleteLocalRef(input);
  env->DeleteLocalRef(screenCls);
  env->DeleteLocalRef(screen);
  env->DeleteLocalRef(mcObj);
  return false;
}

bool isChatOpen() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;

  static jclass mcCls = nullptr;
  static jfieldID f_mc = nullptr;
  static jfieldID f_screen = nullptr;
  static jclass chatCls = nullptr;
  static bool initialized = false;

  if (!initialized) {
    mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (mcCls) {
      mcCls = (jclass)env->NewGlobalRef(mcCls);
      f_mc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;",
                                         "field_71432_P", "S", "Lave;");
      f_screen = lc->GetFieldID(mcCls, "currentScreen",
                                       "Lnet/minecraft/client/gui/GuiScreen;",
                                       "field_71462_r", "m", "Laxu;");
      if (!f_screen)
        f_screen = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiScreen;");
      if (!f_screen)
        f_screen = lc->FindFieldBySignature(mcCls, "Laxu;");
    }

    jclass localChatCls = lc->GetClass("net.minecraft.client.gui.GuiChat");
    if (localChatCls) {
      chatCls = (jclass)env->NewGlobalRef(localChatCls);
    }
    initialized = true;
  }

  if (!mcCls || !f_mc || !f_screen || !chatCls) return false;

  jobject mcObj = env->GetStaticObjectField(mcCls, f_mc);
  if (!mcObj)
    return false;

  jobject screen = env->GetObjectField(mcObj, f_screen);
  env->DeleteLocalRef(mcObj);

  if (!screen)
    return false;

  bool isChat = env->IsInstanceOf(screen, chatCls);
  env->DeleteLocalRef(screen);
  return isChat;
}

} // namespace OVson
