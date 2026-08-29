#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace BlockHitHeuristic {

using Millis = std::int64_t;

struct Rules {
  static constexpr Millis SwingBeforeHurtMs = 500;
  static constexpr Millis SwingAfterHurtMs = 180;
  static constexpr Millis ConfirmationBeforeHurtMs = 140;
  static constexpr Millis ConfirmationAfterHurtMs = 260;
  static constexpr Millis FallbackWaitMs = 190;
  static constexpr Millis DuplicateHurtMs = 250;
  static constexpr Millis TriggerDebounceMs = 350;
  static constexpr Millis ExplosionVetoBeforeMs = 250;
  static constexpr Millis ExplosionVetoAfterMs = 250;
  static constexpr double MaximumSwingDistance = 5.0;
  static constexpr double UnconfirmedSwingDistance = 3.75;
  static constexpr std::size_t MaximumSwings = 32;
  static constexpr std::size_t MaximumConfirmations = 32;
};

enum class Hazard : std::uint32_t {
  None = 0,
  Fall = 1u << 0,
  Fire = 1u << 1,
  Lava = 1u << 2,
  Drowning = 1u << 3,
  Suffocation = 1u << 4,
  Explosion = 1u << 5,
  Void = 1u << 6,
};

constexpr Hazard operator|(Hazard left, Hazard right) {
  return static_cast<Hazard>(static_cast<std::uint32_t>(left) |
                             static_cast<std::uint32_t>(right));
}

constexpr Hazard &operator|=(Hazard &left, Hazard right) {
  left = left | right;
  return left;
}

constexpr bool hasHazard(Hazard hazards) {
  return hazards != Hazard::None;
}

enum class ResetReason {
  WorldChanged,
  Respawn,
  Disconnect,
  Death,
  LocalEntityChanged,
  LocalStateUnavailable,
  FeatureDisabled,
  Shutdown,
  ClockRegression,
};

enum class DiagnosticCode {
  None,
  CandidateAccepted,
  CandidateRejectedLocalPlayer,
  CandidateRejectedNotPlayer,
  CandidateRejectedSameTeam,
  CandidateRejectedRange,
  CandidateRejectedInvalidDistance,
  HurtIgnoredDifferentEntity,
  HurtRejectedNotBlocking,
  HurtRejectedNotSword,
  HurtRejectedEnvironmental,
  PendingCreated,
  HealthDropObserved,
  HealthConfirmationMatched,
  VelocityConfirmationMatched,
  FallbackUsed,
  CandidateExpired,
  DuplicateHurtSuppressed,
  DebounceSuppressed,
  SoundTriggered,
  StateReset,
  InvalidTimestamp,
  ClockRegression,
  InvalidHealth,
  Count,
};

struct Diagnostic {
  DiagnosticCode code = DiagnosticCode::None;
  Millis atMs = 0;
  int entityId = -1;
  double distance = 0.0;
  Hazard hazards = Hazard::None;
  bool healthConfirmed = false;
  bool velocityConfirmed = false;
  ResetReason resetReason = ResetReason::WorldChanged;
  bool attackerBlockingKnown = false;
  bool attackerBlocking = false;
  bool attackerHoldingSword = false;
};

struct Result {
  bool playSound = false;
  std::array<Diagnostic, 8> diagnostics{};
  std::size_t diagnosticCount = 0;

  void add(const Diagnostic &diagnostic);
  void append(const Result &other);
};

struct SwingEvent {
  Millis atMs = 0;
  int entityId = -1;
  double distance = 0.0;
  bool isPlayer = true;
  bool isLocalPlayer = false;
  bool teamKnown = false;
  bool sameTeam = false;
  bool attackerBlockingKnown = false;
  bool attackerBlocking = false;
  bool attackerHoldingSword = false;
};

struct HurtEvent {
  Millis atMs = 0;
  bool targetsLocalPlayer = true;
  bool blocking = false;
  bool holdingSword = false;
  Hazard hazards = Hazard::None;
};

class Detector {
public:
  Result setEnabled(bool enabled, Millis atMs);
  Result observeSwing(const SwingEvent &event);
  Result observeHurt(const HurtEvent &event);
  Result observeHealth(Millis atMs, float health);
  Result observeVelocity(Millis atMs, bool targetsLocalPlayer, int motionX,
                         int motionY, int motionZ);
  Result observeExplosion(Millis atMs);
  Result advance(Millis nowMs);
  Result reset(ResetReason reason, Millis atMs);

  void seedHealth(float health);

  // Whether a hurt has to be corroborated by the server before the sound
  // plays. On (the default) the detector waits for the health drop or the
  // knockback packet that says the server actually registered the hit -- the
  // behaviour this feature has always had. Off, a hurt while blocking with a
  // sword is enough on its own: no swing has to be matched and nothing has to
  // be waited for, so the sound is immediate at the cost of also firing on
  // damage no player dealt. Hazard filtering and the trigger debounce apply
  // either way.
  void setRequireServerConfirmation(bool require);
  [[nodiscard]] bool requiresServerConfirmation() const {
    return requireServerConfirmation_;
  }

  [[nodiscard]] bool enabled() const { return enabled_; }
  [[nodiscard]] bool hasPendingHurt() const { return pending_.has_value(); }
  [[nodiscard]] std::size_t storedSwingCount() const { return swings_.size(); }
  [[nodiscard]] std::size_t storedConfirmationCount() const {
    return healthDrops_.size() + velocities_.size();
  }

private:
  struct SwingCandidate {
    std::uint64_t sequence = 0;
    Millis atMs = 0;
    int entityId = -1;
    double distance = 0.0;
    bool assigned = false;
    bool attackerBlockingKnown = false;
    bool attackerBlocking = false;
    bool attackerHoldingSword = false;
  };

  struct Confirmation {
    std::uint64_t sequence = 0;
    Millis atMs = 0;
    bool assigned = false;
  };

  struct PendingHurt {
    Millis atMs = 0;
    Millis fallbackAtMs = 0;
    Millis expiresAtMs = 0;
    Hazard hazards = Hazard::None;
    std::optional<std::uint64_t> swingSequence;
    int attackerEntityId = -1;
    double attackerDistance = 0.0;
    bool healthConfirmed = false;
    bool velocityConfirmed = false;
    bool attackerBlockingKnown = false;
    bool attackerBlocking = false;
    bool attackerHoldingSword = false;
  };

  bool validateTimestamp(Millis atMs, Result &result);
  void clearState();
  void prune(Millis nowMs);
  void attachBestSwing(Result &result);
  void attachConfirmations(Result &result);
  Result evaluate(Millis nowMs);
  bool recentExplosion(Millis hurtAtMs) const;

  bool enabled_ = false;
  bool requireServerConfirmation_ = true;
  std::uint64_t nextSequence_ = 1;
  std::deque<SwingCandidate> swings_;
  std::deque<Confirmation> healthDrops_;
  std::deque<Confirmation> velocities_;
  std::optional<PendingHurt> pending_;
  std::optional<Millis> lastInputAtMs_;
  std::optional<Millis> lastHurtAtMs_;
  std::optional<Millis> lastTriggerAtMs_;
  std::optional<Millis> lastExplosionAtMs_;
  std::optional<float> lastHealth_;
};

} // namespace BlockHitHeuristic
