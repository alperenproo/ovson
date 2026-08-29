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

#include <Windows.h>
#include <string>
#include <vector>

namespace OVson::NickRoll {
namespace {

// Everything below is touched only from the render thread, the same way
// BedwarsRuntime::tick is.
ULONGLONG g_nextPoll = 0;
std::string g_lastPageJson;
std::string g_lastName;
bool g_bookOpen = false;
// Set when a page could not be understood. Nothing else is scored until the
// book changes, so a malformed page cannot produce a guessed notification.
bool g_halted = false;
bool g_reportedBinding = false;
bool g_reportedVocabulary = false;

// --- auto-reroll ---------------------------------------------------------
// The command TRY AGAIN would run, taken off the page. Empty when nothing is
// queued.
std::string g_pendingReroll;
ULONGLONG g_rerollAt = 0;
// Rerolls spent in the current book. NOT cleared by resetBookState: pressing
// TRY AGAIN swaps the screen for a frame or two, so the book briefly stops
// being a book, and clearing the counter there is exactly how the old cap read
// 522 against a limit of 200. The counter is cleared only after the book has
// genuinely been gone for a while.
int g_rerolls = 0;
ULONGLONG g_bookClosedAt = 0;
bool g_capReported = false;
constexpr ULONGLONG kBookGoneMs = 2500;

// Auto-reroll only ever fires while the game is the window in front. Anything
// else means the keystroke lands in whatever the player actually switched to.
bool gameHasFocus() {
  const HWND foreground = GetForegroundWindow();
  if (!foreground)
    return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(foreground, &pid);
  return pid == GetCurrentProcessId();
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

// Finds the object field holding the book's pages. The deobfuscated type is
// tried first; when that fails the field is identified by what it can DO --
// an NBTTagList is the only thing here exposing getStringTagAt, whose
// signature (I)Ljava/lang/String; contains no Minecraft type and therefore
// survives obfuscation intact. Guessing an obfuscated class name instead is
// how this project previously lost a whole feature to a silent null.
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

    // GetClassFields reports statics too, and reading one with GetObjectField
    // is undefined -- it access-violates on HotSpot.
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

// Reads page 0 of the open book. Every screenshot of this flow says
// "Page 1 of 1", so only the first page is read.
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
  // Signature first: it is mapping-independent, and there is only one method
  // on NBTTagList shaped like this.
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

// The logger truncates at 2048 bytes and a page can be longer than that, so
// the dump is split. Losing the tail of the very thing we are here to learn
// would waste one of the six daily /nick uses.
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

  // In score mode a pass pings; in target mode only the requested word pings.
  if (stopConditionMet && Config::isNickScorePingEnabled())
    BlockHitSound::requestPreview();
}

// Queues TRY AGAIN until the active stop rule is met. With an empty target the
// original score threshold remains the rule; a target switches the rule to a
// case-insensitive nickname substring match.
void maybeQueueReroll(const BookPage &page, const NickScore &result,
                      bool stopConditionMet, const std::string &targetWord) {
  if (!Config::isNickRollAutoRerollEnabled())
    return;
  if (stopConditionMet) {
    // Stopping is the entire purpose. Say so once so the log shows the run
    // ending on a decision rather than just going quiet.
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
  if (!gameHasFocus()) {
    Logger::log(Config::DebugCategory::General,
                "[NickRoll] not rerolling: the game is not the foreground window");
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

  // The command is read off the page rather than assembled. Hypixel is free to
  // change what TRY AGAIN runs, and a hard-coded guess would spend one of the
  // six daily /nick uses on a command that no longer means reroll.
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
  // The first sighting of any distinct page is dumped whole. This is the only
  // way to learn the real structure without spending another of the six daily
  // /nick uses, so it is unconditional rather than debug-gated.
  logLongLine("[NickRoll] book page:", json);

  const BookPage page = parsePageJson(json);
  if (!page.parsed) {
    Logger::error("[NickRoll] page did not parse; standing down for this book");
    g_halted = true;
    return;
  }
  if (!isGeneratedNamePage(page)) {
    // Rank, skin and the intro pages all land here. Nothing to do, and
    // nothing to complain about.
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
  announceScore(result, stopConditionMet, targetWord, targetMatched);
  maybeQueueReroll(page, result, stopConditionMet, targetWord);
}

} // namespace

void tick() {
  // Polled BEFORE the enabled check, or the key could only ever switch the
  // module on -- never off. GetAsyncKeyState rather than a window message
  // because the /nick book is an open GUI screen, which is exactly when this
  // needs to work.
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
    g_bookClosedAt = 0;
    g_capReported = false;
    return;
  }

  if (!lc)
    return;

  const ULONGLONG now = GetTickCount64();

  // A queued reroll fires on its own clock, not the page-poll clock, so the
  // configured delay means what it says.
  if (!g_pendingReroll.empty() && now >= g_rerollAt) {
    const std::string command = g_pendingReroll;
    g_pendingReroll.clear();
    if (gameHasFocus()) {
      ++g_rerolls;
      Logger::log(Config::DebugCategory::General,
                  "[NickRoll] reroll %d: %s", g_rerolls, command.c_str());
      if (!ChatSDK::sendClientChat(command)) {
        Logger::error("[NickRoll] the reroll command could not be sent");
      }
      // g_lastPageJson and g_lastName are deliberately NOT cleared here. The
      // server takes a moment to answer, so the very next poll usually reads
      // the page that is still on screen -- the one we just rerolled away
      // from. Clearing either would score that stale name again and queue a
      // second reroll for it, spending two rerolls on one roll. The page
      // changes on its own when the new name arrives, and that is the signal.
      //
      // The cost is that an identical name rolled twice in a row stalls
      // instead of rerolling. That is the safe direction to fail: boboa
      // presses the button once and it continues.
    }
  }

  // The counter survives the frame or two where pressing TRY AGAIN swaps the
  // screen. It is cleared only once the book has actually been gone a while,
  // which is what makes the cap mean one book rather than one session.
  if (!g_bookOpen && g_bookClosedAt != 0 && now - g_bookClosedAt > kBookGoneMs) {
    if (g_rerolls != 0)
      Logger::log(Config::DebugCategory::General,
                  "[NickRoll] book session ended after %d reroll(s)", g_rerolls);
    g_rerolls = 0;
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

  // lc->GetClass hands back a cached GLOBAL reference, not a local one.
  // Releasing it with DeleteLocalRef is undefined behaviour, and because the
  // reference stays in the cache every later tick reuses the one we damaged --
  // which is exactly how this crashed on every frame. Nothing here is ours to
  // free.
  jclass bookCls = lc->GetClass("net.minecraft.client.gui.GuiScreenBook");
  // GuiScreenBook is not in Java.h's notch map, so on a client that ships
  // Minecraft obfuscated the class cannot be named at all. Rather than refuse
  // to work there, the screen is then identified by what it holds: a page list
  // it can read strings out of. Everything downstream still has to parse as a
  // component and carry the sentence, so a screen wrongly let through here
  // costs nothing.
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
  g_capReported = false;
  g_bookClosedAt = 0;
}

} // namespace OVson::NickRoll
