#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "BlockHitSound.h"

#include "BlockHitAudio.h"
#include "BlockHitAudioBackend.h"
#include "BlockHitHeuristic.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Render/NotificationManager.h"
#include "../SDK/McAccess.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/Logger.h"

#include <Windows.h>
#include <shellapi.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>

namespace BlockHitSound {
namespace {

using BlockHitHeuristic::Diagnostic;
using BlockHitHeuristic::DiagnosticCode;
using BlockHitHeuristic::Hazard;
using BlockHitHeuristic::Millis;
using BlockHitHeuristic::ResetReason;
using BlockHitHeuristic::Result;
using Render::NotificationManager;
using Render::NotificationType;

constexpr std::size_t kMaximumQueuedSignals = 128;
constexpr std::uint64_t kJniRetryMs = 2000;
constexpr std::uint64_t kRejectionLogIntervalMs = 250;

struct ServerSignal {
  ServerSignalKind kind{};
  int entityId = -1;
  int data1 = 0;
  int data2 = 0;
  int data3 = 0;
  float value1 = 0.0f;
  std::uint64_t atMs = 0;
};

struct JniCache {
  bool ready = false;
  std::optional<std::uint64_t> lastAttemptMs;
  jmethodID getEntityId = nullptr;
  jmethodID isBlocking = nullptr;
  jmethodID getHeldItem = nullptr;
  jmethodID getHealth = nullptr;
  jmethodID playSound = nullptr;
  jmethodID isBurning = nullptr;
  jmethodID isInsideOpaqueBlock = nullptr;
  jmethodID isInWater = nullptr;
  jmethodID isInLava = nullptr;
  jmethodID isEntityAlive = nullptr;
  jmethodID isSpectator = nullptr;
  jfieldID fallDistance = nullptr;
  jfieldID posY = nullptr;
  jmethodID worldGetEntity = nullptr;
  jmethodID distanceSqToEntity = nullptr;
  jmethodID isOnSameTeam = nullptr;
  jclass playerClass = nullptr;
  jclass itemClass = nullptr;
  jmethodID stackGetItem = nullptr;
  jmethodID itemGetId = nullptr;
};

struct LocalSnapshot {
  int entityId = -1;
  float health = -1.0f;
  bool blocking = false;
  bool holdingSword = false;
  float fallDistance = 0.0f;
  double posY = 0.0;
  bool burning = false;
  bool opaque = false;
  bool inWater = false;
  bool inLava = false;
};

struct EnvironmentalTracker {
  std::optional<Millis> fall;
  std::optional<Millis> fire;
  std::optional<Millis> lava;
  std::optional<Millis> water;
  std::optional<Millis> opaque;
  std::optional<Millis> voidDamage;

  void clear() { *this = {}; }

  void observe(const LocalSnapshot &snapshot, Millis nowMs) {
    if (snapshot.fallDistance > 2.5f) fall = nowMs;
    if (snapshot.burning) fire = nowMs;
    if (snapshot.inLava) lava = nowMs;
    if (snapshot.inWater) water = nowMs;
    if (snapshot.opaque) opaque = nowMs;
    if (snapshot.posY < -60.0) voidDamage = nowMs;
  }

  Hazard evidenceAt(Millis atMs) const {
    auto recent = [atMs](const std::optional<Millis> &observed,
                         Millis beforeMs, Millis afterMs) {
      if (!observed) return false;
      return *observed <= atMs ? atMs - *observed <= beforeMs
                               : *observed - atMs <= afterMs;
    };

    Hazard hazards = Hazard::None;
    // Falling and opaque/void contact are strong vetoes. Periodic hazards are
    // passed to the pure detector as confidence reducers: they require close
    // range plus velocity instead of suppressing all PvP while on fire/water.
    if (recent(fall, 350, 80)) hazards |= Hazard::Fall;
    if (recent(fire, 150, 80)) hazards |= Hazard::Fire;
    if (recent(lava, 150, 80)) hazards |= Hazard::Lava;
    if (recent(water, 150, 80)) hazards |= Hazard::Drowning;
    if (recent(opaque, 150, 80)) hazards |= Hazard::Suffocation;
    if (recent(voidDamage, 150, 80)) hazards |= Hazard::Void;
    return hazards;
  }
};

std::mutex g_queueMutex;
std::deque<ServerSignal> g_signalQueue;
std::atomic<bool> g_acceptCallbacks{false};
std::atomic<std::uint64_t> g_droppedSignals{0};

// Per-kind arrival counters. The heuristic only logs a decision once a signal
// has already made it into the detector, so when nothing is heard there is no
// way to tell "the packet never arrived" apart from "the packet arrived and
// was rejected". These counters split those two cases: they are incremented
// at the callback boundary, before any filtering, and dumped periodically
// next to the local player's blocking/sword state.
std::atomic<std::uint64_t> g_signalArrivals[8]{};
std::atomic<std::uint64_t> g_worldResetCount{0};
std::uint64_t g_lastTelemetryAtMs = 0;
BlockHitHeuristic::Detector g_detector;
BlockHitHeuristic::Detector g_autoBlockDetector;
JniCache g_jni;
EnvironmentalTracker g_environment;
jobject g_worldRef = nullptr;
int g_localEntityId = -1;
bool g_soundDetectorEnabled = false;
bool g_autoBlockDetectorEnabled = false;
bool g_wasDead = false;
bool g_audioWorkerUsed = false;
bool g_audioSettingsInitialized = false;
bool g_previewWaitingForLoad = false;
BlockHitAudio::SoundSource g_lastSoundSource =
    BlockHitAudio::SoundSource::Default;
std::string g_lastSoundFilename;
float g_lastSoundVolume = BlockHitAudio::kDefaultVolumePercent;
std::atomic<bool> g_reloadRequested{false};
std::atomic<bool> g_previewRequested{false};
std::atomic<bool> g_selectNextRequested{false};
std::uint64_t g_lastCustomFallbackLogMs = 0;
std::array<std::uint64_t,
           static_cast<std::size_t>(DiagnosticCode::Count)>
    g_lastDiagnosticLogMs{};

Millis toMillis(std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<Millis>::max()))
    return std::numeric_limits<Millis>::max();
  return static_cast<Millis>(value);
}

void clearJniException(JNIEnv *env) {
  if (env && env->ExceptionCheck()) env->ExceptionClear();
}

jmethodID findMethod(
    JNIEnv *env, jclass cls,
    std::initializer_list<std::pair<const char *, const char *>> candidates) {
  if (!env || !cls) return nullptr;
  for (const auto &candidate : candidates) {
    jmethodID method =
        env->GetMethodID(cls, candidate.first, candidate.second);
    if (method) return method;
    clearJniException(env);
  }
  return nullptr;
}

jmethodID findStaticMethod(
    JNIEnv *env, jclass cls,
    std::initializer_list<std::pair<const char *, const char *>> candidates) {
  if (!env || !cls) return nullptr;
  for (const auto &candidate : candidates) {
    jmethodID method =
        env->GetStaticMethodID(cls, candidate.first, candidate.second);
    if (method) return method;
    clearJniException(env);
  }
  return nullptr;
}

jfieldID findField(
    JNIEnv *env, jclass cls,
    std::initializer_list<std::pair<const char *, const char *>> candidates) {
  if (!env || !cls) return nullptr;
  for (const auto &candidate : candidates) {
    jfieldID field = env->GetFieldID(cls, candidate.first, candidate.second);
    if (field) return field;
    clearJniException(env);
  }
  return nullptr;
}

bool callBoolean(JNIEnv *env, jobject object, jmethodID method,
                 bool *readSuccessfully = nullptr) {
  if (readSuccessfully) *readSuccessfully = false;
  if (!object || !method) return false;
  const jboolean result = env->CallBooleanMethod(object, method);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return false;
  }
  if (readSuccessfully) *readSuccessfully = true;
  return result == JNI_TRUE;
}

bool isSwordItemId(int itemId) {
  return itemId == 267 || itemId == 268 || itemId == 272 || itemId == 276 ||
         itemId == 283;
}

bool ensureJni(JNIEnv *env, jobject player, jobject world,
               std::uint64_t nowMs) {
  if (g_jni.ready) return true;
  if (g_jni.lastAttemptMs && nowMs >= *g_jni.lastAttemptMs &&
      nowMs - *g_jni.lastAttemptMs < kJniRetryMs) {
    return false;
  }

  JniCache candidate;
  candidate.lastAttemptMs = nowMs;
  jclass playerObjectClass = env->GetObjectClass(player);
  jclass worldObjectClass = env->GetObjectClass(world);
  if (!playerObjectClass || !worldObjectClass) {
    clearJniException(env);
    if (playerObjectClass) env->DeleteLocalRef(playerObjectClass);
    if (worldObjectClass) env->DeleteLocalRef(worldObjectClass);
    g_jni = candidate;
    return false;
  }

  candidate.getEntityId = findMethod(
      env, playerObjectClass,
      {{"getEntityId", "()I"}, {"func_145782_y", "()I"}, {"F", "()I"}});
  candidate.isBlocking = findMethod(
      env, playerObjectClass,
      {{"isBlocking", "()Z"}, {"func_70632_aY", "()Z"}, {"bW", "()Z"}});
  candidate.getHeldItem = findMethod(
      env, playerObjectClass,
      {{"getHeldItem", "()Lnet/minecraft/item/ItemStack;"},
       {"func_70694_bm", "()Lnet/minecraft/item/ItemStack;"},
       {"getHeldItem", "()Lzx;"}, {"func_70694_bm", "()Lzx;"},
       {"bA", "()Lzx;"}});
  candidate.getHealth = findMethod(
      env, playerObjectClass,
      {{"getHealth", "()F"}, {"func_110143_aJ", "()F"}, {"bn", "()F"}});
  candidate.playSound = findMethod(
      env, playerObjectClass,
      {{"playSound", "(Ljava/lang/String;FF)V"},
       {"func_85030_a", "(Ljava/lang/String;FF)V"},
       {"a", "(Ljava/lang/String;FF)V"}});
  candidate.isBurning = findMethod(
      env, playerObjectClass,
      {{"isBurning", "()Z"}, {"func_70027_ad", "()Z"}, {"at", "()Z"}});
  candidate.isInsideOpaqueBlock = findMethod(
      env, playerObjectClass,
      {{"isEntityInsideOpaqueBlock", "()Z"}, {"func_70094_T", "()Z"},
       {"aj", "()Z"}});
  candidate.isInWater = findMethod(
      env, playerObjectClass,
      {{"isInWater", "()Z"}, {"func_70090_H", "()Z"}, {"V", "()Z"}});
  candidate.isInLava = findMethod(
      env, playerObjectClass,
      {{"isInLava", "()Z"}, {"func_70058_J", "()Z"}, {"ab", "()Z"}});
  candidate.isEntityAlive = findMethod(
      env, playerObjectClass,
      {{"isEntityAlive", "()Z"}, {"func_70089_S", "()Z"}, {"ai", "()Z"}});
  candidate.isSpectator = findMethod(
      env, playerObjectClass,
      {{"isSpectator", "()Z"}, {"func_175149_v", "()Z"}, {"v", "()Z"}});
  candidate.fallDistance = findField(
      env, playerObjectClass,
      {{"fallDistance", "F"}, {"field_70143_R", "F"}, {"O", "F"}});
  candidate.posY = findField(
      env, playerObjectClass,
      {{"posY", "D"}, {"field_70163_u", "D"}, {"t", "D"}});

  candidate.worldGetEntity = findMethod(
      env, worldObjectClass,
      {{"getEntityByID", "(I)Lnet/minecraft/entity/Entity;"},
       {"func_73045_a", "(I)Lnet/minecraft/entity/Entity;"},
       {"getEntityByID", "(I)Lpk;"}, {"func_73045_a", "(I)Lpk;"},
       {"a", "(I)Lpk;"}});
  candidate.distanceSqToEntity = findMethod(
      env, playerObjectClass,
      {{"getDistanceSqToEntity", "(Lnet/minecraft/entity/Entity;)D"},
       {"func_70068_e", "(Lnet/minecraft/entity/Entity;)D"},
       {"getDistanceSqToEntity", "(Lpk;)D"},
       {"func_70068_e", "(Lpk;)D"}, {"h", "(Lpk;)D"}});
  candidate.isOnSameTeam = findMethod(
      env, playerObjectClass,
      {{"isOnSameTeam", "(Lnet/minecraft/entity/EntityLivingBase;)Z"},
       {"func_142014_c", "(Lnet/minecraft/entity/EntityLivingBase;)Z"},
       {"isOnSameTeam", "(Lpr;)Z"}, {"func_142014_c", "(Lpr;)Z"},
       {"c", "(Lpr;)Z"}});

  candidate.playerClass =
      lc ? lc->GetClass("net.minecraft.entity.player.EntityPlayer") : nullptr;
  if (!candidate.playerClass && lc) candidate.playerClass = lc->GetClass("wn");
  candidate.itemClass = lc ? lc->GetClass("net.minecraft.item.Item") : nullptr;
  if (!candidate.itemClass && lc) candidate.itemClass = lc->GetClass("zw");
  jclass stackClass =
      lc ? lc->GetClass("net.minecraft.item.ItemStack") : nullptr;
  if (!stackClass && lc) stackClass = lc->GetClass("zx");
  candidate.stackGetItem = findMethod(
      env, stackClass,
      {{"getItem", "()Lnet/minecraft/item/Item;"},
       {"func_77973_b", "()Lnet/minecraft/item/Item;"},
       {"getItem", "()Lzw;"}, {"func_77973_b", "()Lzw;"},
       {"b", "()Lzw;"}});
  candidate.itemGetId = findStaticMethod(
      env, candidate.itemClass,
      {{"getIdFromItem", "(Lnet/minecraft/item/Item;)I"},
       {"func_150891_b", "(Lnet/minecraft/item/Item;)I"},
       {"getIdFromItem", "(Lzw;)I"}, {"func_150891_b", "(Lzw;)I"},
       {"b", "(Lzw;)I"}});

  candidate.ready = candidate.getEntityId && candidate.isBlocking &&
                    candidate.getHeldItem && candidate.getHealth &&
                    candidate.playSound && candidate.worldGetEntity &&
                    candidate.distanceSqToEntity && candidate.playerClass &&
                    candidate.itemClass && candidate.stackGetItem &&
                    candidate.itemGetId;
  g_jni = candidate;
  env->DeleteLocalRef(playerObjectClass);
  env->DeleteLocalRef(worldObjectClass);

  if (Config::isBlockHitSoundDebugEnabled()) {
    Logger::info(
        "[BlockHitSound/heuristic] JNI mapping %s: entity=%d blocking=%d "
        "sword=%d health=%d worldEntity=%d distance=%d sound=%d",
        g_jni.ready ? "ready" : "incomplete", g_jni.getEntityId != nullptr,
        g_jni.isBlocking != nullptr,
        g_jni.getHeldItem && g_jni.stackGetItem && g_jni.itemGetId,
        g_jni.getHealth != nullptr, g_jni.worldGetEntity != nullptr,
        g_jni.distanceSqToEntity != nullptr, g_jni.playSound != nullptr);
  }
  return g_jni.ready;
}

int readEntityId(JNIEnv *env, jobject entity) {
  if (!entity || !g_jni.getEntityId) return -1;
  const jint id = env->CallIntMethod(entity, g_jni.getEntityId);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1;
  }
  return static_cast<int>(id);
}

float readHealth(JNIEnv *env, jobject player) {
  if (!player || !g_jni.getHealth) return -1.0f;
  const jfloat health = env->CallFloatMethod(player, g_jni.getHealth);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1.0f;
  }
  return health;
}

bool readHoldingSword(JNIEnv *env, jobject player) {
  jobject stack = env->CallObjectMethod(player, g_jni.getHeldItem);
  if (env->ExceptionCheck() || !stack) {
    clearJniException(env);
    return false;
  }
  jobject item = env->CallObjectMethod(stack, g_jni.stackGetItem);
  if (env->ExceptionCheck() || !item) {
    clearJniException(env);
    env->DeleteLocalRef(stack);
    return false;
  }
  const jint itemId =
      env->CallStaticIntMethod(g_jni.itemClass, g_jni.itemGetId, item);
  const bool sword = !env->ExceptionCheck() && isSwordItemId(itemId);
  clearJniException(env);
  env->DeleteLocalRef(item);
  env->DeleteLocalRef(stack);
  return sword;
}

LocalSnapshot readLocalSnapshot(JNIEnv *env, jobject player) {
  LocalSnapshot snapshot;
  snapshot.entityId = readEntityId(env, player);
  snapshot.health = readHealth(env, player);
  snapshot.blocking = callBoolean(env, player, g_jni.isBlocking);
  snapshot.holdingSword = readHoldingSword(env, player);
  snapshot.burning = callBoolean(env, player, g_jni.isBurning);
  snapshot.opaque = callBoolean(env, player, g_jni.isInsideOpaqueBlock);
  snapshot.inWater = callBoolean(env, player, g_jni.isInWater);
  snapshot.inLava = callBoolean(env, player, g_jni.isInLava);
  if (g_jni.fallDistance) {
    snapshot.fallDistance = env->GetFloatField(player, g_jni.fallDistance);
    clearJniException(env);
  }
  if (g_jni.posY) {
    snapshot.posY = env->GetDoubleField(player, g_jni.posY);
    clearJniException(env);
  }
  return snapshot;
}

bool isLifecycleSignal(ServerSignalKind kind) {
  return kind == ServerSignalKind::Respawn ||
         kind == ServerSignalKind::Disconnect;
}

void clearQueuedSignals() {
  std::lock_guard<std::mutex> lock(g_queueMutex);
  g_signalQueue.clear();
}

void releaseWorldRef(JNIEnv *env) {
  if (env && g_worldRef) env->DeleteGlobalRef(g_worldRef);
  g_worldRef = nullptr;
}

const char *resetReasonName(ResetReason reason) {
  switch (reason) {
  case ResetReason::WorldChanged: return "world_changed";
  case ResetReason::Respawn: return "respawn";
  case ResetReason::Disconnect: return "disconnect";
  case ResetReason::Death: return "death";
  case ResetReason::LocalEntityChanged: return "local_entity_changed";
  case ResetReason::LocalStateUnavailable: return "local_state_unavailable";
  case ResetReason::FeatureDisabled: return "feature_disabled";
  case ResetReason::Shutdown: return "shutdown";
  case ResetReason::ClockRegression: return "clock_regression";
  default: return "unknown";
  }
}

void resetBoundary(JNIEnv *env, ResetReason reason, Millis atMs,
                    bool clearQueue) {
  const Result result = g_detector.reset(reason, atMs);
  g_autoBlockDetector.reset(reason, atMs);
  Anticheat::resetAutoBlockEvidence();
  g_environment.clear();
  g_localEntityId = -1;
  g_wasDead = false;
  if (clearQueue) clearQueuedSignals();
  if (reason == ResetReason::WorldChanged ||
      reason == ResetReason::Disconnect || reason == ResetReason::Shutdown) {
    releaseWorldRef(env);
  }
  if (g_audioWorkerUsed &&
      (reason == ResetReason::WorldChanged ||
       reason == ResetReason::Disconnect || reason == ResetReason::Death ||
       reason == ResetReason::Respawn || reason == ResetReason::Shutdown)) {
    BlockHitAudioBackend::requestStop();
  }
  if (reason == ResetReason::WorldChanged)
    g_worldResetCount.fetch_add(1, std::memory_order_relaxed);
  if (Config::isBlockHitSoundDebugEnabled() && result.diagnosticCount > 0) {
    Logger::info("[BlockHitSound/heuristic] state_reset reason=%s",
                 resetReasonName(reason));
  }
}

const char *diagnosticName(DiagnosticCode code) {
  switch (code) {
  case DiagnosticCode::CandidateAccepted: return "candidate_accepted";
  case DiagnosticCode::CandidateRejectedLocalPlayer: return "candidate_local";
  case DiagnosticCode::CandidateRejectedNotPlayer: return "candidate_not_player";
  case DiagnosticCode::CandidateRejectedSameTeam: return "candidate_same_team";
  case DiagnosticCode::CandidateRejectedRange: return "candidate_range";
  case DiagnosticCode::CandidateRejectedInvalidDistance: return "candidate_distance_invalid";
  case DiagnosticCode::HurtIgnoredDifferentEntity: return "hurt_other_entity";
  case DiagnosticCode::HurtRejectedNotBlocking: return "hurt_not_blocking";
  case DiagnosticCode::HurtRejectedNotSword: return "hurt_not_sword";
  case DiagnosticCode::HurtRejectedEnvironmental: return "hurt_environmental";
  case DiagnosticCode::PendingCreated: return "pending_created";
  case DiagnosticCode::HealthDropObserved: return "health_drop_observed";
  case DiagnosticCode::HealthConfirmationMatched: return "health_matched";
  case DiagnosticCode::VelocityConfirmationMatched: return "velocity_matched";
  case DiagnosticCode::FallbackUsed: return "fallback_used";
  case DiagnosticCode::CandidateExpired: return "candidate_expired";
  case DiagnosticCode::DuplicateHurtSuppressed: return "duplicate_hurt";
  case DiagnosticCode::DebounceSuppressed: return "debounce";
  case DiagnosticCode::SoundTriggered: return "sound_triggered";
  case DiagnosticCode::StateReset: return "state_reset";
  case DiagnosticCode::InvalidTimestamp: return "timestamp_invalid";
  case DiagnosticCode::ClockRegression: return "clock_regression";
  case DiagnosticCode::InvalidHealth: return "health_invalid";
  default: return "none";
  }
}

bool shouldRateLimit(DiagnosticCode code) {
  return code != DiagnosticCode::SoundTriggered &&
         code != DiagnosticCode::StateReset &&
         code != DiagnosticCode::FallbackUsed;
}

void logDiagnostic(const Diagnostic &diagnostic, std::uint64_t nowMs) {
  if (!Config::isBlockHitSoundDebugEnabled()) return;
  const auto index = static_cast<std::size_t>(diagnostic.code);
  if (index >= g_lastDiagnosticLogMs.size()) return;
  if (shouldRateLimit(diagnostic.code) && g_lastDiagnosticLogMs[index] &&
      nowMs - g_lastDiagnosticLogMs[index] < kRejectionLogIntervalMs) {
    return;
  }
  g_lastDiagnosticLogMs[index] = nowMs;
  if (diagnostic.code == DiagnosticCode::StateReset) {
    Logger::info("[BlockHitSound/heuristic] decision=state_reset t=%lld reason=%s",
                 static_cast<long long>(diagnostic.atMs),
                 resetReasonName(diagnostic.resetReason));
    return;
  }
  Logger::info(
      "[BlockHitSound/heuristic] decision=%s t=%lld entity=%d distance=%.2f "
      "hazards=0x%x health=%d velocity=%d attackerBlockKnown=%d "
      "attackerBlocking=%d attackerSword=%d",
      diagnosticName(diagnostic.code),
      static_cast<long long>(diagnostic.atMs), diagnostic.entityId,
      diagnostic.distance, static_cast<unsigned int>(diagnostic.hazards),
      diagnostic.healthConfirmed, diagnostic.velocityConfirmed,
      diagnostic.attackerBlockingKnown, diagnostic.attackerBlocking,
      diagnostic.attackerHoldingSword);
}

void playMinecraftSound(JNIEnv *env, jobject player, float volumePercent) {
  if (!env || !player || !g_jni.playSound) return;
  const float volume =
      BlockHitAudio::sanitizeVolumePercent(volumePercent) / 100.0f;
  if (volume <= 0.0f) return;
  jstring sound = env->NewStringUTF("random.anvil_land");
  if (!sound) return;
  env->CallVoidMethod(player, g_jni.playSound, sound, volume, 1.65f);
  clearJniException(env);
  env->DeleteLocalRef(sound);
}

void playConfiguredSound(JNIEnv *env, jobject player, bool preview,
                         std::uint64_t nowMs) {
  const float volume = Config::getBlockHitSoundVolume();
  const BlockHitAudio::SoundSource source = BlockHitAudio::parseSoundSource(
      Config::getBlockHitSoundSource());
  const BlockHitAudio::PlaybackTarget target = BlockHitAudio::choosePlayback(
      Config::isBlockHitSoundEnabled(), preview, source,
      BlockHitAudioBackend::isReady(), volume);
  if (target == BlockHitAudio::PlaybackTarget::CustomWav) {
    BlockHitAudioBackend::requestPlay();
    return;
  }
  if (target == BlockHitAudio::PlaybackTarget::DefaultMinecraft) {
    if (source == BlockHitAudio::SoundSource::Custom &&
        Config::isBlockHitSoundDebugEnabled() &&
        (g_lastCustomFallbackLogMs == 0U ||
         nowMs - g_lastCustomFallbackLogMs >= 2000U)) {
      g_lastCustomFallbackLogMs = nowMs;
      Logger::info(
          "[BlockHitSound/audio] custom_unavailable fallback=default file=%s",
          Config::getBlockHitSoundFilename().c_str());
    }
    playMinecraftSound(env, player, volume);
  }
}

void notifyAudio(const char *message, NotificationType type) {
  NotificationManager::getInstance()->add("Block-Hit Sound", message, type);
}

void processAudioEvents() {
  BlockHitAudioBackend::Event event;
  while (BlockHitAudioBackend::pollEvent(event)) {
    switch (event.kind) {
    case BlockHitAudioBackend::EventKind::Loaded: {
      if (Config::isBlockHitSoundDebugEnabled()) {
        Logger::info("[BlockHitSound/audio] loaded file=%s duration_ms=%llu",
                     event.filename.c_str(),
                     static_cast<unsigned long long>(event.durationMs));
      }
      const std::string message = "Loaded " + event.filename;
      notifyAudio(message.c_str(), NotificationType::Success);
      if (g_previewWaitingForLoad) {
        g_previewWaitingForLoad = false;
        g_previewRequested.store(true, std::memory_order_release);
      }
      break;
    }
    case BlockHitAudioBackend::EventKind::LoadFailed: {
      const std::string message =
          "Custom WAV rejected: " +
          std::string(BlockHitAudio::wavErrorMessage(event.wavError));
      notifyAudio(message.c_str(), NotificationType::Warning);
      Logger::error("[BlockHitSound/audio] load_failed file=%s reason=%s",
                    event.filename.c_str(),
                    BlockHitAudio::wavErrorMessage(event.wavError));
      if (g_previewWaitingForLoad) {
        g_previewWaitingForLoad = false;
        g_previewRequested.store(true, std::memory_order_release);
      }
      break;
    }
    case BlockHitAudioBackend::EventKind::SelectionChanged:
      Config::setBlockHitSoundFilename(event.filename);
      g_reloadRequested.store(true, std::memory_order_release);
      break;
    case BlockHitAudioBackend::EventKind::NoFiles:
      notifyAudio("No .wav files found in the sounds folder",
                  NotificationType::Warning);
      break;
    case BlockHitAudioBackend::EventKind::BackendFailed:
      notifyAudio("Custom audio unavailable; using Default",
                  NotificationType::Warning);
      Logger::error("[BlockHitSound/audio] DirectSound initialization failed");
      if (g_previewWaitingForLoad) {
        g_previewWaitingForLoad = false;
        g_previewRequested.store(true, std::memory_order_release);
      }
      break;
    case BlockHitAudioBackend::EventKind::PlaybackFailed:
      notifyAudio("Custom playback failed; using Default next time",
                  NotificationType::Warning);
      Logger::error("[BlockHitSound/audio] DirectSound playback failed file=%s",
                    event.filename.c_str());
      break;
    }
  }
}

void syncAudioSettings() {
  const BlockHitAudio::SoundSource source = BlockHitAudio::parseSoundSource(
      Config::getBlockHitSoundSource());
  const std::string filename = Config::getBlockHitSoundFilename();
  const float volume = BlockHitAudio::sanitizeVolumePercent(
      Config::getBlockHitSoundVolume());
  const bool reload = g_reloadRequested.exchange(false,
                                                  std::memory_order_acq_rel);
  const bool sourceChanged =
      !g_audioSettingsInitialized || source != g_lastSoundSource;
  const bool filenameChanged =
      !g_audioSettingsInitialized || filename != g_lastSoundFilename;
  const bool volumeChanged =
      !g_audioSettingsInitialized || volume != g_lastSoundVolume;

  if (g_selectNextRequested.exchange(false, std::memory_order_acq_rel)) {
    g_audioWorkerUsed = true;
    BlockHitAudioBackend::requestSelectNext(getSoundsDirectory(), filename);
  }
  if (source == BlockHitAudio::SoundSource::Custom &&
      (sourceChanged || filenameChanged || reload)) {
    g_audioWorkerUsed = true;
    BlockHitAudioBackend::requestLoad(getSoundsDirectory(), filename, volume);
  } else if (reload) {
    // Reload remains useful while Default is selected: it validates and caches
    // the chosen file before the user switches sources or presses Preview.
    g_audioWorkerUsed = true;
    BlockHitAudioBackend::requestLoad(getSoundsDirectory(), filename, volume);
  } else if (g_audioWorkerUsed && volumeChanged) {
    BlockHitAudioBackend::requestVolume(volume);
  }
  if (g_audioWorkerUsed && sourceChanged &&
      source == BlockHitAudio::SoundSource::Default) {
    BlockHitAudioBackend::requestStop();
  }

  g_lastSoundSource = source;
  g_lastSoundFilename = filename;
  g_lastSoundVolume = volume;
  g_audioSettingsInitialized = true;
}

void applyResult(JNIEnv *env, jobject player, const Result &result,
                  std::uint64_t nowMs) {
  for (std::size_t i = 0; i < result.diagnosticCount; ++i)
    logDiagnostic(result.diagnostics[i], nowMs);
  if (result.playSound) playConfiguredSound(env, player, false, nowMs);
}

void applyAutoBlockResult(const Result &result) {
  for (std::size_t i = 0; i < result.diagnosticCount; ++i) {
    const Diagnostic &diagnostic = result.diagnostics[i];
    Anticheat::abLog(
        "detector decision=%s t=%lld eid=%d distance=%.2f health=%d "
        "velocity=%d blockKnown=%d blocking=%d sword=%d",
        diagnosticName(diagnostic.code),
        static_cast<long long>(diagnostic.atMs), diagnostic.entityId,
        diagnostic.distance, (int)diagnostic.healthConfirmed,
        (int)diagnostic.velocityConfirmed,
        (int)diagnostic.attackerBlockingKnown,
        (int)diagnostic.attackerBlocking,
        (int)diagnostic.attackerHoldingSword);
    if (diagnostic.code != DiagnosticCode::SoundTriggered ||
        diagnostic.entityId < 0 ||
        (!diagnostic.healthConfirmed && !diagnostic.velocityConfirmed)) {
      continue;
    }
    Anticheat::observeConfirmedBlockHit(
        diagnostic.entityId, static_cast<std::uint64_t>(diagnostic.atMs),
        diagnostic.distance, diagnostic.attackerBlockingKnown,
        diagnostic.attackerBlocking, diagnostic.attackerHoldingSword,
        diagnostic.healthConfirmed, diagnostic.velocityConfirmed);
  }
}

BlockHitHeuristic::SwingEvent inspectSwing(JNIEnv *env, jobject player,
                                           jobject world,
                                           const ServerSignal &signal) {
  BlockHitHeuristic::SwingEvent event;
  event.atMs = toMillis(signal.atMs);
  event.entityId = signal.entityId;
  event.isLocalPlayer = signal.entityId == g_localEntityId;
  if (event.isLocalPlayer) return event;

  jobject attacker =
      env->CallObjectMethod(world, g_jni.worldGetEntity, signal.entityId);
  if (env->ExceptionCheck() || !attacker) {
    clearJniException(env);
    event.isPlayer = false;
    return event;
  }

  event.isPlayer = env->IsInstanceOf(attacker, g_jni.playerClass) == JNI_TRUE;
  if (event.isPlayer && g_jni.isEntityAlive) {
    bool read = false;
    const bool alive = callBoolean(env, attacker, g_jni.isEntityAlive, &read);
    if (read && !alive) event.isPlayer = false;
  }
  if (event.isPlayer && g_jni.isSpectator) {
    bool read = false;
    const bool spectator = callBoolean(env, attacker, g_jni.isSpectator, &read);
    if (read && spectator) event.isPlayer = false;
  }

  if (event.isPlayer && g_jni.isOnSameTeam) {
    const jboolean sameTeam =
        env->CallBooleanMethod(player, g_jni.isOnSameTeam, attacker);
    if (!env->ExceptionCheck()) {
      event.teamKnown = true;
      event.sameTeam = sameTeam == JNI_TRUE;
    }
    clearJniException(env);
  }

  const jdouble distanceSq =
      env->CallDoubleMethod(player, g_jni.distanceSqToEntity, attacker);
  if (!env->ExceptionCheck() && distanceSq >= 0.0) {
    event.distance = std::sqrt(static_cast<double>(distanceSq));
  } else {
    event.distance = std::numeric_limits<double>::quiet_NaN();
  }
  clearJniException(env);

  if (event.isPlayer) {
    event.attackerBlocking =
        callBoolean(env, attacker, g_jni.isBlocking,
                    &event.attackerBlockingKnown);
    event.attackerHoldingSword = readHoldingSword(env, attacker);
  }
  env->DeleteLocalRef(attacker);
  return event;
}

} // namespace

void setCallbackAcceptance(bool accepting) {
  g_acceptCallbacks.store(accepting, std::memory_order_release);
  if (!accepting) clearQueuedSignals();
}

void requestCustomSoundReload() {
  g_reloadRequested.store(true, std::memory_order_release);
}

void requestPreview() {
  const BlockHitAudio::SoundSource source = BlockHitAudio::parseSoundSource(
      Config::getBlockHitSoundSource());
  if (source == BlockHitAudio::SoundSource::Custom &&
      !BlockHitAudioBackend::isReady()) {
    g_previewWaitingForLoad = true;
    g_reloadRequested.store(true, std::memory_order_release);
    return;
  }
  g_previewRequested.store(true, std::memory_order_release);
}

void requestSelectNextCustomSound() {
  g_selectNextRequested.store(true, std::memory_order_release);
}

std::string getSoundsDirectory() {
  return Config::getDataDirectory() + "\\sounds";
}

bool openSoundsDirectory() {
  const std::string directory = getSoundsDirectory();
  if (!CreateDirectoryA(directory.c_str(), nullptr) &&
      GetLastError() != ERROR_ALREADY_EXISTS) {
    return false;
  }
  const HINSTANCE result = ShellExecuteA(nullptr, "open", directory.c_str(),
                                         nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(result) > 32;
}

void enqueueServerSignal(ServerSignalKind kind, int entityId, int data1,
                         int data2, int data3, float value1, float, float,
                         std::uint64_t receivedAtMs) {
  if (!g_acceptCallbacks.load(std::memory_order_acquire)) return;
  {
    const std::size_t slot = static_cast<std::size_t>(kind);
    constexpr std::size_t kSlots =
        sizeof(g_signalArrivals) / sizeof(g_signalArrivals[0]);
    if (slot < kSlots)
      g_signalArrivals[slot].fetch_add(1, std::memory_order_relaxed);
  }
  const ServerSignal signal{kind, entityId, data1, data2, data3, value1,
                            receivedAtMs ? receivedAtMs : GetTickCount64()};
  std::lock_guard<std::mutex> lock(g_queueMutex);
  if (!g_acceptCallbacks.load(std::memory_order_relaxed)) return;
  if (isLifecycleSignal(kind) && g_signalQueue.size() >= kMaximumQueuedSignals) {
    g_signalQueue.clear();
  } else if (g_signalQueue.size() >= kMaximumQueuedSignals) {
    auto removable = g_signalQueue.begin();
    while (removable != g_signalQueue.end() &&
           isLifecycleSignal(removable->kind)) {
      ++removable;
    }
    if (removable != g_signalQueue.end())
      g_signalQueue.erase(removable);
    else {
      g_droppedSignals.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    g_droppedSignals.fetch_add(1, std::memory_order_relaxed);
  }
  g_signalQueue.push_back(signal);
}

void update(JNIEnv *env) {
  syncAudioSettings();
  processAudioEvents();
  if (!env || !lc) return;
  const std::uint64_t now = GetTickCount64();
  const Millis nowMs = toMillis(now);

  std::deque<ServerSignal> signals;
  {
    std::lock_guard<std::mutex> lock(g_queueMutex);
    signals.swap(g_signalQueue);
  }

  jobject world = Mc::theWorld(env);
  jobject player = Mc::thePlayer(env);
  if (!world || !player) {
    if (world) env->DeleteLocalRef(world);
    if (player) env->DeleteLocalRef(player);
    if (g_worldRef || g_localEntityId >= 0)
      resetBoundary(env, ResetReason::Disconnect, nowMs, true);
    return;
  }

  bool sessionChanged = false;
  if (!g_worldRef) {
    g_worldRef = env->NewGlobalRef(world);
    g_environment.clear();
    sessionChanged = true;
  } else if (!env->IsSameObject(g_worldRef, world)) {
    resetBoundary(env, ResetReason::WorldChanged, nowMs, true);
    g_worldRef = env->NewGlobalRef(world);
    sessionChanged = true;
  }

  // Pushed every tick rather than on a transition: it is a single bool on a
  // pure object, and a toggle mid-fight has to take effect on the next hurt,
  // not on the next time the feature is switched off and on again.
  g_detector.setRequireServerConfirmation(
      Config::isBlockHitWaitForServerEnabled());
  g_autoBlockDetector.setRequireServerConfirmation(true);

  const bool soundEnabled = Config::isBlockHitSoundEnabled();
  const bool autoBlockEnabled =
      Config::isAnticheatEnabled() && Config::isAnticheatAutoBlockEnabled();
  if (soundEnabled != g_soundDetectorEnabled) {
    g_soundDetectorEnabled = soundEnabled;
    const Result transition = g_detector.setEnabled(soundEnabled, nowMs);
    applyResult(env, player, transition, now);
    g_environment.clear();
    if (soundEnabled) sessionChanged = true;
    else if (g_audioWorkerUsed) BlockHitAudioBackend::requestStop();
  }
  if (autoBlockEnabled != g_autoBlockDetectorEnabled) {
    g_autoBlockDetectorEnabled = autoBlockEnabled;
    const Result transition =
        g_autoBlockDetector.setEnabled(autoBlockEnabled, nowMs);
    applyAutoBlockResult(transition);
    Anticheat::resetAutoBlockEvidence();
    if (autoBlockEnabled) sessionChanged = true;
  }
  const bool detectorEnabled = soundEnabled || autoBlockEnabled;
  if (!detectorEnabled) {
    signals.clear();
    clearQueuedSignals();
  }
  const bool preview =
      g_previewRequested.exchange(false, std::memory_order_acq_rel);
  if (!detectorEnabled && !preview) {
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    return;
  }

  if (!ensureJni(env, player, world, now)) {
    signals.clear();
    if (preview)
      g_previewRequested.store(true, std::memory_order_release);
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    return;
  }

  if (preview) playConfiguredSound(env, player, true, now);
  if (!detectorEnabled) {
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    return;
  }

  LocalSnapshot snapshot = readLocalSnapshot(env, player);
  if (snapshot.entityId < 0 || !std::isfinite(snapshot.health) ||
      snapshot.health < 0.0f) {
    if (g_localEntityId >= 0 || g_detector.hasPendingHurt() ||
        g_detector.storedSwingCount() != 0 ||
        g_detector.storedConfirmationCount() != 0 ||
        g_autoBlockDetector.hasPendingHurt() ||
        g_autoBlockDetector.storedSwingCount() != 0 ||
        g_autoBlockDetector.storedConfirmationCount() != 0) {
      resetBoundary(env, ResetReason::LocalStateUnavailable, nowMs, true);
    }
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    return;
  }
  if (g_localEntityId >= 0 && snapshot.entityId >= 0 &&
      g_localEntityId != snapshot.entityId) {
    resetBoundary(env, ResetReason::LocalEntityChanged, nowMs, true);
    signals.clear();
    sessionChanged = true;
  }
  g_localEntityId = snapshot.entityId;

  if (snapshot.health >= 0.0f && snapshot.health <= 0.0f) {
    if (!g_wasDead) resetBoundary(env, ResetReason::Death, nowMs, true);
    g_wasDead = true;
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(world);
    return;
  }
  if (g_wasDead && snapshot.health > 0.0f) {
    resetBoundary(env, ResetReason::Respawn, nowMs, true);
    signals.clear();
    g_localEntityId = snapshot.entityId;
    sessionChanged = true;
  }
  g_wasDead = false;
  g_environment.observe(snapshot, nowMs);
  if (sessionChanged) {
    g_detector.seedHealth(snapshot.health);
    g_autoBlockDetector.seedHealth(snapshot.health);
  }

  for (const ServerSignal &signal : signals) {
    Result soundResult;
    Result autoBlockResult;
    const Millis signalAt = toMillis(signal.atMs);
    switch (signal.kind) {
    case ServerSignalKind::Swing: {
      const BlockHitHeuristic::SwingEvent event =
          inspectSwing(env, player, world, signal);
      soundResult = g_detector.observeSwing(event);
      autoBlockResult = g_autoBlockDetector.observeSwing(event);
      break;
    }
    case ServerSignalKind::Hurt: {
      const BlockHitHeuristic::HurtEvent event{
          signalAt, signal.entityId == g_localEntityId, snapshot.blocking,
          snapshot.holdingSword, g_environment.evidenceAt(signalAt)};
      soundResult = g_detector.observeHurt(event);
      autoBlockResult = g_autoBlockDetector.observeHurt(event);
      break;
    }
    case ServerSignalKind::Health:
      soundResult = g_detector.observeHealth(signalAt, signal.value1);
      autoBlockResult =
          g_autoBlockDetector.observeHealth(signalAt, signal.value1);
      break;
    case ServerSignalKind::Velocity:
      soundResult = g_detector.observeVelocity(
          signalAt, signal.entityId == g_localEntityId, signal.data1,
          signal.data2, signal.data3);
      autoBlockResult = g_autoBlockDetector.observeVelocity(
          signalAt, signal.entityId == g_localEntityId, signal.data1,
          signal.data2, signal.data3);
      break;
    case ServerSignalKind::Explosion:
      soundResult = g_detector.observeExplosion(signalAt);
      autoBlockResult = g_autoBlockDetector.observeExplosion(signalAt);
      break;
    case ServerSignalKind::Respawn:
      soundResult = g_detector.reset(ResetReason::Respawn, signalAt);
      autoBlockResult =
          g_autoBlockDetector.reset(ResetReason::Respawn, signalAt);
      g_environment.clear();
      g_detector.seedHealth(snapshot.health);
      g_autoBlockDetector.seedHealth(snapshot.health);
      Anticheat::resetAutoBlockEvidence();
      if (g_audioWorkerUsed) BlockHitAudioBackend::requestStop();
      break;
    case ServerSignalKind::Disconnect:
      soundResult = g_detector.reset(ResetReason::Disconnect, signalAt);
      autoBlockResult =
          g_autoBlockDetector.reset(ResetReason::Disconnect, signalAt);
      g_environment.clear();
      Anticheat::resetAutoBlockEvidence();
      if (g_audioWorkerUsed) BlockHitAudioBackend::requestStop();
      break;
    }
    applyResult(env, player, soundResult, now);
    applyAutoBlockResult(autoBlockResult);
    if (signal.kind == ServerSignalKind::Disconnect) break;
  }

  applyResult(env, player, g_detector.advance(nowMs), now);
  applyAutoBlockResult(g_autoBlockDetector.advance(nowMs));

  const std::uint64_t dropped =
      g_droppedSignals.exchange(0, std::memory_order_relaxed);
  if (dropped && Config::isBlockHitSoundDebugEnabled()) {
    Logger::info("[BlockHitSound/heuristic] packet_queue_dropped=%llu",
                 static_cast<unsigned long long>(dropped));
  }

  // Arrival telemetry. Every 2s, dump what the Netty callback actually
  // delivered since the last dump plus the local state the hurt rule tests
  // against. Reading this against a fight tells us in one line which link of
  // the chain is broken: no hurt= means the damage packet is never matched in
  // PacketFilterHook, hurt>0 with blocking=0 means the blocking field is read
  // wrong, and a climbing worldResets means the session keeps being wiped
  // before a swing and a hurt can ever be correlated.
  if (Config::isBlockHitSoundDebugEnabled() &&
      now - g_lastTelemetryAtMs >= 2000) {
    g_lastTelemetryAtMs = now;
    auto take = [](ServerSignalKind kind) {
      return static_cast<unsigned long long>(
          g_signalArrivals[static_cast<std::size_t>(kind)].exchange(
              0, std::memory_order_relaxed));
    };
    const unsigned long long hurt = take(ServerSignalKind::Hurt);
    const unsigned long long swing = take(ServerSignalKind::Swing);
    const unsigned long long velocity = take(ServerSignalKind::Velocity);
    const unsigned long long health = take(ServerSignalKind::Health);
    const unsigned long long respawn = take(ServerSignalKind::Respawn);
    const unsigned long long explosion = take(ServerSignalKind::Explosion);
    if (hurt || swing || velocity || health || respawn || explosion) {
      Logger::info("[BlockHitSound/telemetry] hurt=%llu swing=%llu vel=%llu "
                   "health=%llu respawn=%llu boom=%llu | enabled=%d "
                   "blocking=%d sword=%d hp=%.1f entity=%d pending=%d "
                   "swings=%zu worldResets=%llu",
                   hurt, swing, velocity, health, respawn, explosion,
                   soundEnabled ? 1 : 0, snapshot.blocking ? 1 : 0,
                   snapshot.holdingSword ? 1 : 0,
                   static_cast<double>(snapshot.health), snapshot.entityId,
                   (g_detector.hasPendingHurt() ||
                    g_autoBlockDetector.hasPendingHurt()) ? 1 : 0,
                   g_detector.storedSwingCount() +
                       g_autoBlockDetector.storedSwingCount(),
                   static_cast<unsigned long long>(
                       g_worldResetCount.load(std::memory_order_relaxed)));
    }
  }

  env->DeleteLocalRef(player);
  env->DeleteLocalRef(world);
}

void shutdown(JNIEnv *env) {
  setCallbackAcceptance(false);
  resetBoundary(env, ResetReason::Shutdown, toMillis(GetTickCount64()), true);
  g_soundDetectorEnabled = false;
  g_autoBlockDetectorEnabled = false;
  g_detector.setEnabled(false, toMillis(GetTickCount64()));
  g_autoBlockDetector.setEnabled(false, toMillis(GetTickCount64()));
  g_previewWaitingForLoad = false;
  g_reloadRequested.store(false, std::memory_order_release);
  g_previewRequested.store(false, std::memory_order_release);
  g_selectNextRequested.store(false, std::memory_order_release);
  BlockHitAudioBackend::shutdown();
  g_audioWorkerUsed = false;
  g_audioSettingsInitialized = false;
}

} // namespace BlockHitSound
