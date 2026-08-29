#include "BlockHitHeuristic.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace BlockHitHeuristic {
namespace {

Millis saturatingAdd(Millis value, Millis delta) {
  if (delta > 0 && value > std::numeric_limits<Millis>::max() - delta)
    return std::numeric_limits<Millis>::max();
  return value + delta;
}

bool withinWindow(Millis evidenceAtMs, Millis hurtAtMs, Millis beforeMs,
                  Millis afterMs) {
  if (evidenceAtMs <= hurtAtMs)
    return hurtAtMs - evidenceAtMs <= beforeMs;
  return evidenceAtMs - hurtAtMs <= afterMs;
}

bool hasStrongHazard(Hazard hazards) {
  constexpr std::uint32_t mask =
      static_cast<std::uint32_t>(Hazard::Fall) |
      static_cast<std::uint32_t>(Hazard::Suffocation) |
      static_cast<std::uint32_t>(Hazard::Explosion) |
      static_cast<std::uint32_t>(Hazard::Void);
  return (static_cast<std::uint32_t>(hazards) & mask) != 0;
}

bool hasPeriodicHazard(Hazard hazards) {
  constexpr std::uint32_t mask =
      static_cast<std::uint32_t>(Hazard::Fire) |
      static_cast<std::uint32_t>(Hazard::Lava) |
      static_cast<std::uint32_t>(Hazard::Drowning);
  return (static_cast<std::uint32_t>(hazards) & mask) != 0;
}

Millis absoluteDelta(Millis left, Millis right) {
  return left >= right ? left - right : right - left;
}

template <typename T>
void boundDeque(std::deque<T> &values, std::size_t maximum) {
  while (values.size() > maximum) values.pop_front();
}

} // namespace

void Result::add(const Diagnostic &diagnostic) {
  if (diagnosticCount < diagnostics.size())
    diagnostics[diagnosticCount++] = diagnostic;
}

void Result::append(const Result &other) {
  playSound = playSound || other.playSound;
  for (std::size_t i = 0; i < other.diagnosticCount; ++i)
    add(other.diagnostics[i]);
}

bool Detector::validateTimestamp(Millis atMs, Result &result) {
  if (atMs < 0) {
    result.add({DiagnosticCode::InvalidTimestamp, atMs});
    return false;
  }
  if (lastInputAtMs_ && atMs < *lastInputAtMs_) {
    clearState();
    lastInputAtMs_ = atMs;
    Diagnostic diagnostic{DiagnosticCode::ClockRegression, atMs};
    diagnostic.resetReason = ResetReason::ClockRegression;
    result.add(diagnostic);
    return false;
  }
  lastInputAtMs_ = atMs;
  return true;
}

void Detector::clearState() {
  swings_.clear();
  healthDrops_.clear();
  velocities_.clear();
  pending_.reset();
  lastHurtAtMs_.reset();
  lastTriggerAtMs_.reset();
  lastExplosionAtMs_.reset();
  lastHealth_.reset();
}

Result Detector::setEnabled(bool enabled, Millis atMs) {
  Result result;
  if (atMs < 0) {
    result.add({DiagnosticCode::InvalidTimestamp, atMs});
    return result;
  }
  if (enabled_ == enabled) {
    if (!lastInputAtMs_ || atMs >= *lastInputAtMs_) lastInputAtMs_ = atMs;
    return result;
  }
  enabled_ = enabled;
  clearState();
  lastInputAtMs_ = atMs;
  if (!enabled) {
    Diagnostic diagnostic{DiagnosticCode::StateReset, atMs};
    diagnostic.resetReason = ResetReason::FeatureDisabled;
    result.add(diagnostic);
  }
  return result;
}

Result Detector::reset(ResetReason reason, Millis atMs) {
  Result result;
  clearState();
  if (atMs >= 0) lastInputAtMs_ = atMs;
  Diagnostic diagnostic{DiagnosticCode::StateReset, atMs};
  diagnostic.resetReason = reason;
  result.add(diagnostic);
  return result;
}

void Detector::seedHealth(float health) {
  if (std::isfinite(health)) lastHealth_ = health;
}

void Detector::prune(Millis nowMs) {
  const Millis swingRetention =
      Rules::SwingBeforeHurtMs + Rules::ConfirmationAfterHurtMs;
  while (!swings_.empty() && nowMs >= swings_.front().atMs &&
         nowMs - swings_.front().atMs > swingRetention) {
    swings_.pop_front();
  }
  const Millis confirmationRetention =
      Rules::ConfirmationBeforeHurtMs + Rules::ConfirmationAfterHurtMs;
  auto pruneConfirmations = [nowMs, confirmationRetention](auto &values) {
    while (!values.empty() && nowMs >= values.front().atMs &&
           nowMs - values.front().atMs > confirmationRetention) {
      values.pop_front();
    }
  };
  pruneConfirmations(healthDrops_);
  pruneConfirmations(velocities_);
}

void Detector::setRequireServerConfirmation(bool require) {
  requireServerConfirmation_ = require;
}

Result Detector::observeSwing(const SwingEvent &event) {
  Result result;
  if (!enabled_ || !validateTimestamp(event.atMs, result)) return result;
  prune(event.atMs);

  if (event.isLocalPlayer) {
    result.add({DiagnosticCode::CandidateRejectedLocalPlayer, event.atMs,
                event.entityId, event.distance});
    return result;
  }
  if (!event.isPlayer) {
    result.add({DiagnosticCode::CandidateRejectedNotPlayer, event.atMs,
                event.entityId, event.distance});
    return result;
  }
  if (!std::isfinite(event.distance) || event.distance < 0.0) {
    result.add({DiagnosticCode::CandidateRejectedInvalidDistance, event.atMs,
                event.entityId, event.distance});
    return result;
  }
  if (event.distance > Rules::MaximumSwingDistance) {
    result.add({DiagnosticCode::CandidateRejectedRange, event.atMs,
                event.entityId, event.distance});
    return result;
  }
  if (event.teamKnown && event.sameTeam) {
    result.add({DiagnosticCode::CandidateRejectedSameTeam, event.atMs,
                event.entityId, event.distance});
    return result;
  }

  swings_.push_back({nextSequence_++,
                     event.atMs,
                     event.entityId,
                     event.distance,
                     false,
                     event.attackerBlockingKnown,
                     event.attackerBlocking,
                     event.attackerHoldingSword});
  boundDeque(swings_, Rules::MaximumSwings);
  result.add({DiagnosticCode::CandidateAccepted, event.atMs, event.entityId,
              event.distance});
  attachBestSwing(result);
  result.append(evaluate(event.atMs));
  return result;
}

Result Detector::observeHurt(const HurtEvent &event) {
  Result result;
  if (!enabled_ || !validateTimestamp(event.atMs, result)) return result;
  prune(event.atMs);

  if (!event.targetsLocalPlayer) {
    result.add({DiagnosticCode::HurtIgnoredDifferentEntity, event.atMs});
    return result;
  }
  if (lastHurtAtMs_ && event.atMs - *lastHurtAtMs_ < Rules::DuplicateHurtMs) {
    result.add({DiagnosticCode::DuplicateHurtSuppressed, event.atMs});
    return result;
  }
  lastHurtAtMs_ = event.atMs;

  if (!event.blocking) {
    result.add({DiagnosticCode::HurtRejectedNotBlocking, event.atMs});
    return result;
  }
  if (!event.holdingSword) {
    result.add({DiagnosticCode::HurtRejectedNotSword, event.atMs});
    return result;
  }

  Hazard hazards = event.hazards;
  if (recentExplosion(event.atMs)) hazards |= Hazard::Explosion;
  if (hasStrongHazard(hazards)) {
    Diagnostic diagnostic{DiagnosticCode::HurtRejectedEnvironmental,
                          event.atMs};
    diagnostic.hazards = hazards;
    result.add(diagnostic);
    return result;
  }

  if (pending_) {
    result.add({DiagnosticCode::CandidateExpired, event.atMs,
                pending_->attackerEntityId, pending_->attackerDistance});
    pending_.reset();
  }

  pending_ = PendingHurt{event.atMs,
                         saturatingAdd(event.atMs, Rules::FallbackWaitMs),
                         saturatingAdd(event.atMs,
                                       Rules::ConfirmationAfterHurtMs),
                         hazards};
  result.add({DiagnosticCode::PendingCreated, event.atMs});
  attachBestSwing(result);
  attachConfirmations(result);
  result.append(evaluate(event.atMs));
  return result;
}

Result Detector::observeHealth(Millis atMs, float health) {
  Result result;
  if (!enabled_ || !validateTimestamp(atMs, result)) return result;
  if (!std::isfinite(health)) {
    result.add({DiagnosticCode::InvalidHealth, atMs});
    return result;
  }

  if (lastHealth_ && health < *lastHealth_ - 0.01f) {
    healthDrops_.push_back({nextSequence_++, atMs, false});
    boundDeque(healthDrops_, Rules::MaximumConfirmations);
    result.add({DiagnosticCode::HealthDropObserved, atMs});
  }
  lastHealth_ = health;
  prune(atMs);
  attachConfirmations(result);
  result.append(evaluate(atMs));
  return result;
}

Result Detector::observeVelocity(Millis atMs, bool targetsLocalPlayer,
                                 int motionX, int motionY, int motionZ) {
  Result result;
  if (!enabled_ || !validateTimestamp(atMs, result)) return result;
  if (!targetsLocalPlayer ||
      (motionX == 0 && motionY == 0 && motionZ == 0)) {
    return result;
  }
  velocities_.push_back({nextSequence_++, atMs, false});
  boundDeque(velocities_, Rules::MaximumConfirmations);
  prune(atMs);
  attachConfirmations(result);
  result.append(evaluate(atMs));
  return result;
}

Result Detector::observeExplosion(Millis atMs) {
  Result result;
  if (!enabled_ || !validateTimestamp(atMs, result)) return result;
  lastExplosionAtMs_ = atMs;
  if (pending_ && withinWindow(atMs, pending_->atMs,
                               Rules::ExplosionVetoBeforeMs,
                               Rules::ExplosionVetoAfterMs)) {
    pending_->hazards |= Hazard::Explosion;
  }
  result.append(evaluate(atMs));
  return result;
}

bool Detector::recentExplosion(Millis hurtAtMs) const {
  return lastExplosionAtMs_ &&
         withinWindow(*lastExplosionAtMs_, hurtAtMs,
                      Rules::ExplosionVetoBeforeMs,
                      Rules::ExplosionVetoAfterMs);
}

void Detector::attachBestSwing(Result &) {
  if (!pending_ || pending_->swingSequence) return;

  SwingCandidate *best = nullptr;
  for (auto &candidate : swings_) {
    if (candidate.assigned ||
        !withinWindow(candidate.atMs, pending_->atMs,
                      Rules::SwingBeforeHurtMs, Rules::SwingAfterHurtMs)) {
      continue;
    }
    if (!best || candidate.distance < best->distance ||
        (std::abs(candidate.distance - best->distance) < 0.0001 &&
         absoluteDelta(candidate.atMs, pending_->atMs) <
             absoluteDelta(best->atMs, pending_->atMs)) ||
        (std::abs(candidate.distance - best->distance) < 0.0001 &&
         absoluteDelta(candidate.atMs, pending_->atMs) ==
             absoluteDelta(best->atMs, pending_->atMs) &&
         candidate.sequence < best->sequence)) {
      best = &candidate;
    }
  }
  if (!best) return;
  best->assigned = true;
  pending_->swingSequence = best->sequence;
  pending_->attackerEntityId = best->entityId;
  pending_->attackerDistance = best->distance;
  pending_->attackerBlockingKnown = best->attackerBlockingKnown;
  pending_->attackerBlocking = best->attackerBlocking;
  pending_->attackerHoldingSword = best->attackerHoldingSword;
}

void Detector::attachConfirmations(Result &result) {
  if (!pending_) return;

  auto attach = [this, &result](auto &values, bool &target,
                                DiagnosticCode code) {
    if (target) return;
    Confirmation *best = nullptr;
    for (auto &confirmation : values) {
      if (confirmation.assigned ||
          !withinWindow(confirmation.atMs, pending_->atMs,
                        Rules::ConfirmationBeforeHurtMs,
                        Rules::ConfirmationAfterHurtMs)) {
        continue;
      }
      if (!best || absoluteDelta(confirmation.atMs, pending_->atMs) <
                       absoluteDelta(best->atMs, pending_->atMs) ||
          (absoluteDelta(confirmation.atMs, pending_->atMs) ==
               absoluteDelta(best->atMs, pending_->atMs) &&
           confirmation.sequence < best->sequence)) {
        best = &confirmation;
      }
    }
    if (!best) return;
    best->assigned = true;
    target = true;
    Diagnostic diagnostic{code, best->atMs, pending_->attackerEntityId,
                          pending_->attackerDistance};
    result.add(diagnostic);
  };

  attach(healthDrops_, pending_->healthConfirmed,
         DiagnosticCode::HealthConfirmationMatched);
  attach(velocities_, pending_->velocityConfirmed,
         DiagnosticCode::VelocityConfirmationMatched);
}

Result Detector::evaluate(Millis nowMs) {
  Result result;
  if (!pending_) return result;

  if (hasStrongHazard(pending_->hazards)) {
    Diagnostic diagnostic{DiagnosticCode::HurtRejectedEnvironmental,
                          pending_->atMs, pending_->attackerEntityId,
                          pending_->attackerDistance};
    diagnostic.hazards = pending_->hazards;
    result.add(diagnostic);
    pending_.reset();
    return result;
  }

  const bool hasSwing = pending_->swingSequence.has_value();
  const bool confirmed =
      pending_->healthConfirmed || pending_->velocityConfirmed;
  const bool periodicHazard = hasPeriodicHazard(pending_->hazards);
  bool fallback = false;
  bool shouldTrigger =
      hasSwing && confirmed &&
      (!periodicHazard ||
       (pending_->velocityConfirmed &&
        pending_->attackerDistance <= Rules::UnconfirmedSwingDistance));
  if (!shouldTrigger && hasSwing && nowMs >= pending_->fallbackAtMs &&
      !periodicHazard &&
      pending_->attackerDistance <= Rules::UnconfirmedSwingDistance) {
    fallback = true;
    shouldTrigger = true;
  }

  // With server confirmation switched off the question collapses to the one
  // the player actually asked: am I blocking, and am I taking damage? Both are
  // already established -- observeHurt rejects a hurt that is not while
  // blocking with a sword, and the environmental checks above still stand --
  // so there is nothing left to wait for. No swing has to be matched either,
  // which is the real cost: damage from something that never swung, an arrow
  // for instance, now counts. That is the trade the switch exists to make.
  if (!requireServerConfirmation_ && !periodicHazard) {
    shouldTrigger = true;
  }

  if (shouldTrigger) {
    if (lastTriggerAtMs_ &&
        pending_->atMs - *lastTriggerAtMs_ < Rules::TriggerDebounceMs) {
      result.add({DiagnosticCode::DebounceSuppressed, pending_->atMs,
                  pending_->attackerEntityId,
                  pending_->attackerDistance});
      pending_.reset();
      return result;
    }

    if (fallback) {
      result.add({DiagnosticCode::FallbackUsed, pending_->atMs,
                  pending_->attackerEntityId,
                  pending_->attackerDistance});
    }
    Diagnostic diagnostic{DiagnosticCode::SoundTriggered,
                          pending_->atMs,
                          pending_->attackerEntityId,
                          pending_->attackerDistance};
    diagnostic.healthConfirmed = pending_->healthConfirmed;
    diagnostic.velocityConfirmed = pending_->velocityConfirmed;
    diagnostic.attackerBlockingKnown = pending_->attackerBlockingKnown;
    diagnostic.attackerBlocking = pending_->attackerBlocking;
    diagnostic.attackerHoldingSword = pending_->attackerHoldingSword;
    result.add(diagnostic);
    result.playSound = true;
    lastTriggerAtMs_ = pending_->atMs;
    pending_.reset();
    return result;
  }

  if (nowMs >= pending_->expiresAtMs) {
    if (periodicHazard) {
      Diagnostic diagnostic{DiagnosticCode::HurtRejectedEnvironmental,
                            pending_->atMs, pending_->attackerEntityId,
                            pending_->attackerDistance};
      diagnostic.hazards = pending_->hazards;
      result.add(diagnostic);
    } else {
      result.add({DiagnosticCode::CandidateExpired, pending_->atMs,
                  pending_->attackerEntityId,
                  pending_->attackerDistance});
    }
    pending_.reset();
  }
  return result;
}

Result Detector::advance(Millis nowMs) {
  Result result;
  if (!enabled_ || !validateTimestamp(nowMs, result)) return result;
  prune(nowMs);
  attachBestSwing(result);
  attachConfirmations(result);
  result.append(evaluate(nowMs));
  return result;
}

} // namespace BlockHitHeuristic
