#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "NickRollRuntime.h"

#include "NickRollCore.h"
#include "NickScore.h"

#include "../../Config/Config.h"
#include "../../Java.h"
#include "../../Chat/ChatSDK.h"
#include "../BlockHitSound.h"
#include "../../Render/NotificationManager.h"
#include "../../Utils/Logger.h"
#include "../StatsTracker.internal.h"

#include <Windows.h>
#include <ShlObj.h>
#include <string>
#include <vector>

namespace OVson::NickRoll {
namespace {

ULONGLONG g_nextPoll = 0;
std::string g_lastPageJson;
std::string g_lastName;
bool g_bookOpen = false;
bool g_halted = false;
bool g_reportedBinding = false;
bool g_reportedVocabulary = false;

std::string g_pendingReroll;
ULONGLONG g_rerollAt = 0;
int g_rerolls = 0;
int g_rerollsInCurrentLobby = 0;
ULONGLONG g_bookClosedAt = 0;
bool g_capReported = false;
constexpr ULONGLONG kBookGoneMs = 2500;

bool g_waitingForLobbyLoad = false;
ULONGLONG g_reopenBookAt = 0;
ULONGLONG g_nextLimboEscape = 0;
size_t g_lobbyIdx = 0;
const char *const kLobbies[] = {
    "bw", "sw", "duels", "arcade", "classic", "tnt", "uhc", "mm", "bb", "mw", "blitz"
};

bool gameHasFocus() {
  const HWND foreground = GetForegroundWindow();
  if (!foreground)
    return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(foreground, &pid);
  return pid == GetCurrentProcessId();
}

int getPlayerDimension(JNIEnv *env, jobject mc) {
  if (!env || !mc || !lc)
    return 0;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return 0;

  jfieldID playerField = lc->GetFieldID(
      mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
      "field_71439_g", "h");
  if (!playerField) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    playerField = lc->FindFieldBySignature(
        mcCls, "Lnet/minecraft/client/entity/EntityPlayerSP;");
  }
  if (!playerField)
    return 0;

  jobject player = env->GetObjectField(mc, playerField);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!player)
    return 0;

  int dimension = 0;
  jclass entityCls = lc->GetClass("net.minecraft.entity.Entity");
  if (entityCls) {
    jfieldID dimField = lc->GetFieldID(
        entityCls, "dimension", "I", "field_71093_bK", "am");
    if (dimField) {
      dimension = env->GetIntField(player, dimField);
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
  }
  env->DeleteLocalRef(player);
  return dimension;
}

std::string fromJavaString(JNIEnv *env, jstring value) {
  if (!env || !value)
    return {};
  const char *utf = env->GetStringUTFChars(value, nullptr);
  std::string out = utf ? utf : "";
  if (utf)
    env->ReleaseStringUTFChars(value, utf);
  return out;
}

void resetBookState(const char *reason) {
  if (g_bookOpen && reason)
    Logger::log(Config::DebugCategory::General,
                "[NickRoll] book closed (%s)", reason);
  if (g_bookOpen)
    g_bookClosedAt = GetTickCount64();
  g_bookOpen = false;
  g_halted = false;
  g_lastPageJson.clear();
  g_lastName.clear();
  g_pendingReroll.clear();
}

jfieldID findPagesField(JNIEnv *env, jobject screen, jclass screenCls) {
  jfieldID byType =
      lc->FindFieldBySignature(screenCls, "Lnet/minecraft/nbt/NBTTagList;");
  if (byType)
    return byType;
  if (!lc->jvmti)
    return nullptr;

  jint fieldCount = 0;
  jfieldID *fields = nullptr;
  if (lc->jvmti->GetClassFields(screenCls, &fieldCount, &fields) !=
      JVMTI_ERROR_NONE)
    return nullptr;

  jfieldID found = nullptr;
  for (jint i = 0; i < fieldCount && !found; ++i) {
    char *fieldName = nullptr;
    char *fieldSig = nullptr;
    if (lc->jvmti->GetFieldName(screenCls, fields[i], &fieldName, &fieldSig,
                                nullptr) != JVMTI_ERROR_NONE)
      continue;

    jint modifiers = 0;
    const bool isStatic =
        lc->jvmti->GetFieldModifiers(screenCls, fields[i], &modifiers) !=
            JVMTI_ERROR_NONE ||
        (modifiers & 0x0008) != 0;

    if (!isStatic && fieldSig && fieldSig[0] == 'L') {
      jobject candidate = env->GetObjectField(screen, fields[i]);
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (candidate) {
        jclass candidateCls = env->GetObjectClass(candidate);
        if (candidateCls &&
            lc->FindMethodBySignature(candidateCls, "(I)Ljava/lang/String;")) {
          found = fields[i];
          Logger::log(Config::DebugCategory::General,
                      "[NickRoll] page list found structurally: field '%s' "
                      "of type %s",
                      fieldName ? fieldName : "?", fieldSig ? fieldSig : "?");
        }
        if (candidateCls)
          env->DeleteLocalRef(candidateCls);
        env->DeleteLocalRef(candidate);
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    if (fieldName)
      lc->jvmti->Deallocate(reinterpret_cast<unsigned char *>(fieldName));
    if (fieldSig)
      lc->jvmti->Deallocate(reinterpret_cast<unsigned char *>(fieldSig));
  }
  if (fields)
    lc->jvmti->Deallocate(reinterpret_cast<unsigned char *>(fields));
  return found;
}

std::string readFirstPage(JNIEnv *env, jobject screen) {
  jclass screenCls = env->GetObjectClass(screen);
  if (!screenCls)
    return {};

  jfieldID pagesField = findPagesField(env, screen, screenCls);
  env->DeleteLocalRef(screenCls);
  if (!pagesField) {
    static bool reported = false;
    if (!reported) {
      reported = true;
      Logger::log(Config::DebugCategory::General,
                  "[NickRoll] this screen has no readable page list");
    }
    return {};
  }

  jobject pages = env->GetObjectField(screen, pagesField);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!pages)
    return {};

  jclass listCls = env->GetObjectClass(pages);
  jmethodID getString =
      listCls ? lc->FindMethodBySignature(listCls, "(I)Ljava/lang/String;")
              : nullptr;

  std::string page;
  if (getString) {
    jstring text = static_cast<jstring>(
        env->CallObjectMethod(pages, getString, static_cast<jint>(0)));
    if (env->ExceptionCheck())
      env->ExceptionClear();
    if (text) {
      page = fromJavaString(env, text);
      env->DeleteLocalRef(text);
    }
  } else {
    static bool reported = false;
    if (!reported) {
      reported = true;
      Logger::log(Config::DebugCategory::General,
                  "[NickRoll] the page list has no readable string accessor");
    }
  }
  if (listCls)
    env->DeleteLocalRef(listCls);
  env->DeleteLocalRef(pages);
  return page;
}

void logLongLine(const char *tag, const std::string &text) {
  constexpr std::size_t kChunk = 400;
  if (text.size() <= kChunk) {
    Logger::info("%s %s", tag, text.c_str());
    return;
  }
  const std::size_t parts = (text.size() + kChunk - 1) / kChunk;
  for (std::size_t i = 0; i < parts; ++i) {
    Logger::info("%s [%u/%u] %s", tag, static_cast<unsigned>(i + 1),
                 static_cast<unsigned>(parts),
                 text.substr(i * kChunk, kChunk).c_str());
  }
}

Render::NotificationType notificationType(const NickScore &result) {
  if (!result.valid || result.verdict == "Reject")
    return Render::NotificationType::Error;
  if (result.verdict == "Borderline")
    return Render::NotificationType::Warning;
  return Render::NotificationType::Success;
}

DWORD verdictColor(const NickScore &result) {
  if (!result.valid || result.verdict == "Reject") return 0xFFFF3333;
  if (result.verdict == "Borderline") return 0xFFFFCC00;
  return 0xFF00FF55;
}

void logToDisk(const NickScore& result) {
  if (!Config::isNickRollLogAllEnabled()) return;

  wchar_t appData[MAX_PATH] = L"";
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) return;

  wchar_t dirPath[MAX_PATH];
  swprintf_s(dirPath, L"%s\\OVson", appData);
  CreateDirectoryW(dirPath, nullptr);
  swprintf_s(dirPath, L"%s\\OVson\\nickreroll", appData);
  CreateDirectoryW(dirPath, nullptr);

  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t filePath[MAX_PATH];
  swprintf_s(filePath, L"%s\\NickRoll_%04d-%02d-%02d.log", dirPath, st.wYear, st.wMonth, st.wDay);

  FILE* f = nullptr;
  _wfopen_s(&f, filePath, L"a");
  if (f) {
    fprintf(f, "[%02d:%02d:%02d] Nick: %-16s | Score: %3d | Tell: %3d | Appeal: %3d | Verdict: %s\n",
            st.wHour, st.wMinute, st.wSecond,
            result.nickname.c_str(), result.score, result.tell, result.appeal,
            result.verdict.c_str());
    fclose(f);
  }
}

void announceScore(const NickScore &result, bool stopConditionMet,
                   const std::string &targetWord, bool targetMatched) {
  if (Config::isNickScoreAlertEveryEnabled() || stopConditionMet) {
    const DWORD resultColor = verdictColor(result);
    std::vector<Render::NotificationSegment> segments = {
        {result.nickname, 0xFFFFFFFF},
        {" · ", 0xFF909096},
        {result.verdict, resultColor},
        {" · ", 0xFF909096},
        {result.summary, 0xFFE0E0E0},
    };
    if (targetMatched) {
      segments.push_back({" · target: ", 0xFF909096});
      segments.push_back({targetWord, 0xFF00FF55});
    }
    Render::NotificationManager::getInstance()->addRich(
        targetMatched ? "NICK TARGET FOUND"
                      : "NICK SCORE " + std::to_string(result.score),
        segments,
        targetMatched ? Render::NotificationType::Success
                      : notificationType(result),
        4.0F, 8);
  }

  if (stopConditionMet && Config::isNickScorePingEnabled())
    BlockHitSound::requestPreview();
}

void maybeQueueReroll(const BookPage &page, const NickScore &result,
                      bool stopConditionMet, const std::string &targetWord) {
  if (!Config::isNickRollAutoRerollEnabled())
    return;
  if (stopConditionMet) {
    if (targetWord.empty()) {
      Logger::info(
          "[NickRoll] stopping on '%s' (score %d >= %d) after %d reroll(s)",
          result.nickname.c_str(), result.score, result.threshold, g_rerolls);
    } else {
      Logger::info(
          "[NickRoll] stopping on '%s' (contains target '%s') after %d reroll(s)",
          result.nickname.c_str(), targetWord.c_str(), g_rerolls);
    }
    return;
  }
  const int cap = Config::getNickRollRerollCap();
  if (g_rerolls >= cap) {
    if (!g_capReported) {
      g_capReported = true;
      Logger::error("[NickRoll] reroll cap of %d reached; stopping", cap);
      Render::NotificationManager::getInstance()->add(
          "NICK ROLL", "Reroll cap of " + std::to_string(cap) + " reached",
          Render::NotificationType::Warning, 4.0F, 8);
    }
    return;
  }

  const std::string command = findButtonCommand(page, "try again");
  if (command.empty()) {
    Logger::error("[NickRoll] TRY AGAIN carries no run_command on this page; "
                  "not rerolling");
    return;
  }

  g_pendingReroll = command;
  g_rerollAt = GetTickCount64() +
               static_cast<ULONGLONG>(Config::getNickRollRerollDelayMs());
}

void handlePage(const std::string &json) {
  logLongLine("[NickRoll] book page:", json);

  const BookPage page = parsePageJson(json);
  if (!page.parsed) {
    Logger::error("[NickRoll] page did not parse; standing down for this book");
    g_halted = true;
    return;
  }
  if (!isGeneratedNamePage(page)) {
    return;
  }

  const std::string name = findGeneratedName(page);
  if (name.empty()) {
    Logger::error("[NickRoll] generated-name page recognised but no username "
                  "could be read; standing down for this book");
    g_halted = true;
    return;
  }
  if (name == g_lastName)
    return;
  g_lastName = name;

  if (!g_reportedVocabulary) {
    g_reportedVocabulary = true;
    if (vocabularyAvailable())
      Logger::info("[NickRoll] private NickScore vocabulary loaded");
    else
      Logger::error("[NickRoll] NickScore vocabulary unavailable; using "
                    "shape-only scoring");
  }

  const NickScore result = scoreNickname(name, Config::getNickScoreThreshold());
  const std::string targetWord = Config::getNickRollTargetWord();
  const bool targetMatched = nicknameMatchesTargetWord(name, targetWord);
  const bool stopConditionMet =
      shouldStopReroll(result.passes, name, targetWord);
  Logger::info("[NickRoll] '%s' score=%d tell=%d appeal=%d threshold=%d "
               "passes=%s target='%s' targetMatch=%s pattern=%s",
               name.c_str(), result.score, result.tell, result.appeal,
               result.threshold, result.passes ? "yes" : "no",
               targetWord.c_str(), targetMatched ? "yes" : "no",
               result.pattern.c_str());
  logToDisk(result);
    announceScore(result, stopConditionMet, targetWord, targetMatched);
  maybeQueueReroll(page, result, stopConditionMet, targetWord);
}

} // namespace

void tick() {
  {
    static bool wasDown = false;
    const int key = Config::getNickRollToggleKey();
    const bool down =
        key > 0 && gameHasFocus() && (GetAsyncKeyState(key) & 0x8000) != 0;
    if (down && !wasDown) {
      const bool next = !Config::isNickRollEnabled();
      Config::setNickRollEnabled(next);
      Render::NotificationManager::getInstance()->add(
          "Nick Score", next ? "Nick scoring enabled" : "Nick scoring disabled",
          next ? Render::NotificationType::Success
               : Render::NotificationType::Warning,
          2.5F, 8);
    }
    wasDown = down;
  }

  if (!Config::isNickRollEnabled()) {
    resetBookState("module disabled");
    g_rerolls = 0;
    g_rerollsInCurrentLobby = 0;
    g_waitingForLobbyLoad = false;
    g_reopenBookAt = 0;
    g_nextLimboEscape = 0;
    g_bookClosedAt = 0;
    g_capReported = false;
    return;
  }

  if (!lc)
    return;

  const ULONGLONG now = GetTickCount64();

  if (!g_pendingReroll.empty() && now >= g_rerollAt) {
    const std::string command = g_pendingReroll;
    g_pendingReroll.clear();
    ++g_rerolls;
    ++g_rerollsInCurrentLobby;
    Logger::log(Config::DebugCategory::General,
                "[NickRoll] reroll %d (lobby %d): %s", g_rerolls,
                g_rerollsInCurrentLobby, command.c_str());
    if (!ChatSDK::sendClientChat(command)) {
      Logger::error("[NickRoll] the reroll command could not be sent");
    }
  }

  if (!g_waitingForLobbyLoad && !g_bookOpen && g_bookClosedAt != 0 && now - g_bookClosedAt > kBookGoneMs) {
    if (g_rerolls != 0)
      Logger::log(Config::DebugCategory::General,
                  "[NickRoll] book session ended after %d reroll(s)", g_rerolls);
    g_rerolls = 0;
    g_rerollsInCurrentLobby = 0;
    g_capReported = false;
    g_bookClosedAt = 0;
  }

  if (now < g_nextPoll)
    return;
  g_nextPoll = now + 150;

  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
  jmethodID getMc = lc->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;",
      "func_71410_x", "A", "()Lave;");
  if (!getMc)
    return;
  jobject mc = env->CallStaticObjectMethod(mcCls, getMc);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (!mc)
    return;

  const int dimension = getPlayerDimension(env, mc);

  if (Config::isNickRollLimboRecoveryEnabled() && dimension == 1) {
    if (now >= g_nextLimboEscape) {
      g_nextLimboEscape = now + 5000;
      const char *targetLobby =
          kLobbies[g_lobbyIdx % (sizeof(kLobbies) / sizeof(kLobbies[0]))];
      std::string lCmd = std::string("/l ") + targetLobby;
      Logger::info("[NickRoll] In Limbo (dimension 1). Escaping via %s",
                   lCmd.c_str());
      Render::NotificationManager::getInstance()->add(
          "Nick Reroll", "In Limbo - escaping to lobby...",
          Render::NotificationType::Warning, 3.0F, 8);
      ChatSDK::sendClientChat(lCmd);
      g_waitingForLobbyLoad = true;
      g_reopenBookAt = now + 4000;
      g_rerollsInCurrentLobby = 0;
      resetBookState("escaping limbo");
    }
    env->DeleteLocalRef(mc);
    return;
  }

  const int swapInterval = Config::getNickRollLobbySwapInterval();
  if (swapInterval > 0 && g_rerollsInCurrentLobby >= swapInterval) {
    g_rerollsInCurrentLobby = 0;
    g_lobbyIdx++;
    const char *targetLobby =
        kLobbies[g_lobbyIdx % (sizeof(kLobbies) / sizeof(kLobbies[0]))];
    std::string lCmd = std::string("/l ") + targetLobby;
    Logger::info(
        "[NickRoll] Lobby swap interval reached (%d rolls). Switching lobby: %s",
        swapInterval, lCmd.c_str());
    Render::NotificationManager::getInstance()->add(
        "Nick Reroll", "Rotating lobby to avoid rate limits...",
        Render::NotificationType::Info, 3.0F, 8);
    ChatSDK::sendClientChat(lCmd);
    g_waitingForLobbyLoad = true;
    g_reopenBookAt = now + 4000;
    resetBookState("swapping lobby");
    env->DeleteLocalRef(mc);
    return;
  }

  if (g_waitingForLobbyLoad) {
    if (now >= g_reopenBookAt) {
      if (!OVson::g_inHypixelGame && !OVson::g_inPreGameLobby && dimension == 0) {
        Logger::info(
            "[NickRoll] In lobby, reopening nick book: /nick help setrandom");
        ChatSDK::sendClientChat("/nick help setrandom");
        g_waitingForLobbyLoad = false;
        g_nextPoll = now + 1000;
      } else {
        g_reopenBookAt = now + 2000;
      }
    }
    env->DeleteLocalRef(mc);
    return;
  }

  jfieldID screenField = lc->GetFieldID(
      mcCls, "currentScreen", "Lnet/minecraft/client/gui/GuiScreen;",
      "field_71462_r", "m", "Laxu;");
  if (!screenField) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    screenField =
        lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiScreen;");
  }
  jobject screen = screenField ? env->GetObjectField(mc, screenField) : nullptr;
  if (env->ExceptionCheck())
    env->ExceptionClear();
  env->DeleteLocalRef(mc);

  if (!screen) {
    resetBookState("no screen");
    return;
  }

  jclass bookCls = lc->GetClass("net.minecraft.client.gui.GuiScreenBook");
  const bool namedBook =
      bookCls && env->IsInstanceOf(screen, bookCls) == JNI_TRUE;
  if (env->ExceptionCheck())
    env->ExceptionClear();
  const bool isBook = bookCls ? namedBook : true;
  if (!g_reportedBinding) {
    g_reportedBinding = true;
    Logger::info("[NickRoll] watching for the nick book: GuiScreenBook %s",
                 bookCls ? "resolved by name"
                         : "not nameable on this client, matching by shape");
  }
  if (!isBook) {
    env->DeleteLocalRef(screen);
    resetBookState("different screen");
    return;
  }

  if (!g_bookOpen) {
    g_bookOpen = true;
    Logger::log(Config::DebugCategory::General, "[NickRoll] book opened");
  }

  const std::string json = readFirstPage(env, screen);
  env->DeleteLocalRef(screen);
  if (json.empty() || json == g_lastPageJson)
    return;
  g_lastPageJson = json;
  if (!g_halted) handlePage(json);
}

void shutdown() {
  resetBookState(nullptr);
  g_rerolls = 0;
  g_rerollsInCurrentLobby = 0;
  g_waitingForLobbyLoad = false;
  g_reopenBookAt = 0;
  g_nextLimboEscape = 0;
  g_capReported = false;
  g_bookClosedAt = 0;
}

} // namespace OVson::NickRoll



