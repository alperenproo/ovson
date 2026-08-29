#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace OVson::Bedwars {

using Tick = std::uint64_t;

template <typename T, std::size_t Capacity> class BoundedQueue {
  static_assert(Capacity > 0, "a bounded queue needs positive capacity");

public:
  bool push(T value) {
    const bool dropped = m_values.size() == Capacity;
    if (dropped) {
      m_values.pop_front();
      ++m_dropped;
    }
    m_values.push_back(std::move(value));
    return dropped;
  }

  std::deque<T> take() {
    std::deque<T> result;
    result.swap(m_values);
    return result;
  }

  std::size_t size() const { return m_values.size(); }
  std::uint64_t consumeDropped() {
    const std::uint64_t result = m_dropped;
    m_dropped = 0;
    return result;
  }
  void clear() {
    m_values.clear();
    m_dropped = 0;
  }

private:
  std::deque<T> m_values;
  std::uint64_t m_dropped = 0;
};

enum class Module : std::size_t {
  EventTimers,
  ShopHelper,
  HeightOverlay,
  UpgradeAlerts,
  ConsumeAlerts,
  PickupAlerts,
  ArmorAlerts,
  TrapNotifier,
  ResourceTracker,
  ItemAlerts,
  UpgradeHud,
  Count
};

constexpr std::size_t kModuleCount = static_cast<std::size_t>(Module::Count);
const char *moduleName(Module module);
const char *moduleKey(Module module);
bool isModuleAvailable(Module module);

enum class GamePhase { Inactive, Lobby, PreGame, Active, Spectator, PostGame };

enum class TeamId : std::size_t {
  Red,
  Blue,
  Green,
  Yellow,
  Aqua,
  White,
  Pink,
  Gray,
  Unknown,
  Count
};

constexpr std::size_t kStandardTeamCount = 8;
constexpr std::size_t kTeamCount = static_cast<std::size_t>(TeamId::Count);

const char *teamName(TeamId team);
const char *teamFormattingCode(TeamId team);
std::uint32_t teamArgb(TeamId team);
TeamId normalizeTeam(const std::string &value);
std::optional<TeamId> parseDestroyedBedTeam(const std::string &message);

// Extracts the victim's username from a Hypixel Bedwars death/kill-feed
// chat line (e.g. "Steve was killed by Alex.", "Steve fell into the
// void."). Returns std::nullopt when the line doesn't match a recognized
// death phrase, or when the text preceding the phrase isn't a single
// well-formed Minecraft username (this keeps ordinary chat sentences that
// happen to contain one of these words from being misread).
std::optional<std::string> parseDeathVictim(const std::string &message);

enum class BedState { Unknown, Alive, Destroyed };

struct TeamState {
  TeamId id = TeamId::Unknown;
  BedState bed = BedState::Unknown;
  bool sharpnessObserved = false;
  bool protectionObserved = false;
  Tick latestObservation = 0;
  std::string latestSource;
  std::array<std::string, 16> playerNames{};
  std::array<int, 16> entityIds{};
  std::size_t playerCount = 0;
};

class TeamTracker {
public:
  TeamTracker();
  void observePlayer(const std::string &identity, int entityId, TeamId team,
                     bool authoritative, Tick now,
                     const std::string &source = "scoreboard");
  bool observeSharpness(TeamId team, Tick now,
                        const std::string &source = "enchanted sword");
  bool observeProtection(TeamId team, Tick now,
                         const std::string &source = "enchanted armor");
  void observeBed(TeamId team, BedState state, Tick now,
                  const std::string &source);
  void expire(Tick now, Tick maximumAgeMs = 15000);
  TeamId playerTeam(const std::string &identity) const;
  const TeamState &team(TeamId id) const;
  const std::array<TeamState, kStandardTeamCount> &teams() const {
    return m_teams;
  }
  void reset();

private:
  struct PlayerBinding {
    std::string identity;
    int entityId = -1;
    TeamId team = TeamId::Unknown;
    bool authoritative = false;
    Tick lastSeen = 0;
  };
  std::array<TeamState, kStandardTeamCount> m_teams{};
  std::array<PlayerBinding, 128> m_players{};
  std::size_t m_playerCount = 0;
};

struct SessionObservation {
  bool featureEnabled = false;
  bool worldValid = false;
  bool playerValid = false;
  bool onHypixel = false;
  bool bedwarsMode = false;
  bool inGame = false;
  bool preGame = false;
  bool replay = false;
  bool dead = false;
  int dimension = 0;
  int localEntityId = -1;
  std::uintptr_t worldIdentity = 0;
  std::string mapName;
  std::string modeName;
  std::string localTeam;
  int teamCount = 0;
};

struct LifecycleTransition {
  bool reset = false;
  bool gameStarted = false;
  bool gameEnded = false;
  bool preservedGameClock = false;
  std::string reason;
};

class Context {
public:
  LifecycleTransition observe(const SessionObservation &observation, Tick now);
  void reset(const char *reason = "manual");

  GamePhase phase() const { return m_phase; }
  Tick gameStart() const { return m_gameStart; }
  bool gameClockKnown() const { return m_gameClockKnown; }
  Tick elapsed(Tick now) const;
  std::uint64_t generation() const { return m_generation; }
  const SessionObservation &lastObservation() const { return m_last; }
  const std::string &lastResetReason() const { return m_lastResetReason; }

private:
  bool m_haveObservation = false;
  SessionObservation m_last;
  GamePhase m_phase = GamePhase::Inactive;
  Tick m_gameStart = 0;
  bool m_gameClockKnown = false;
  Tick m_lastTick = 0;
  std::uint64_t m_generation = 0;
  std::string m_lastResetReason;
};

enum class TimedEventKind {
  DiamondTwo,
  EmeraldTwo,
  DiamondThree,
  EmeraldThree,
  BedDestruction,
  SuddenDeath,
  GameEnd
};

struct TimedEvent {
  TimedEventKind kind;
  Tick offsetMs;
  const char *label;
};

struct EventCountdown {
  TimedEventKind kind;
  std::string label;
  Tick remainingMs = 0;
  bool due = false;
};

class EventSchedule {
public:
  static const std::array<TimedEvent, 7> &standardEvents();
  static std::vector<EventCountdown> countdowns(Tick elapsedMs,
                                                 bool onlyNext);
  static std::optional<EventCountdown> parseScoreboardLine(
      const std::string &line);
};

enum class Resource { Iron, Gold, Diamond, Emerald, Count };
constexpr std::size_t kResourceCount = static_cast<std::size_t>(Resource::Count);
const char *resourceName(Resource resource);

struct ResourceSnapshot {
  std::array<int, kResourceCount> inventory{};
  std::array<int, kResourceCount> enderChest{};
  bool inventoryValid = false;
  bool enderChestValid = false;
  bool containerOpen = false;
};

struct ResourceDelta {
  Resource resource = Resource::Iron;
  int amount = 0;
  bool enderChest = false;
};

class ResourceMonitor {
public:
  std::vector<ResourceDelta> observe(const ResourceSnapshot &snapshot);
  void reset();

private:
  std::optional<ResourceSnapshot> m_previous;
};

enum class UpgradeKind {
  Sharpness,
  Protection,
  Forge,
  Haste,
  HealPool,
  DragonBuff,
  FeatherFalling,
  Unknown
};

struct UpgradeState {
  int sharpness = 0;
  int protection = 0;
  int forge = 0;
  int haste = 0;
  int featherFalling = 0;
  bool sharpnessKnown = false;
  bool protectionKnown = false;
  bool forgeKnown = false;
  bool hasteKnown = false;
  bool featherFallingKnown = false;
  bool healPool = false;
  bool healPoolKnown = false;
  bool dragonBuff = false;
  bool dragonBuffKnown = false;
};

enum class TrapState { Unknown, Queued, Missing };

struct ChatSignal {
  enum class Kind { None, Upgrade, TrapTriggered, TrapQueued, TrapMissing };
  Kind kind = Kind::None;
  UpgradeKind upgrade = UpgradeKind::Unknown;
  int level = 0;
  std::string label;
};

class ChatMonitor {
public:
  std::optional<ChatSignal> observe(const std::string &message, Tick now);
  const UpgradeState &upgrades() const { return m_upgrades; }
  const std::string &knownTrap() const { return m_knownTrap; }
  TrapState trapState() const { return m_trapState; }
  void reset();

private:
  bool isDuplicate(const std::string &normalized, Tick now);
  UpgradeState m_upgrades;
  std::string m_knownTrap;
  TrapState m_trapState = TrapState::Unknown;
  std::string m_lastMessage;
  Tick m_lastMessageTick = 0;
};

enum class ArmorTier { None, Leather, Chain, Iron, Diamond };
enum class PotionKind { Unknown, Speed, Jump, Invisibility };
enum class SwordTier { None, Wood, Stone, Iron, Diamond };
enum class ImportantItem : std::size_t {
  None,
  StoneSword,
  IronSword,
  DiamondSword,
  Bow,
  EnchantedBow,
  KnockbackStick,
  SpeedPotion,
  JumpPotion,
  InvisibilityPotion,
  Tnt,
  Fireball,
  EnderPearl,
  GoldenApple,
  Milk,
  BridgeEgg,
  WaterBucket,
  DreamDefender,
  GoldenPickaxe,
  DiamondPickaxe,
  Obsidian,
  Count
};

inline constexpr std::size_t kImportantItemCount =
    static_cast<std::size_t>(ImportantItem::Count);

// Every item starts alertable; the per-item switches only ever turn things
// off, so a newly added item is never silently disabled by an old config.
constexpr std::array<bool, kImportantItemCount> allItemAlertsEnabled() {
  std::array<bool, kImportantItemCount> flags{};
  for (std::size_t i = 0; i < flags.size(); ++i)
    flags[i] = true;
  return flags;
}

// How long a player has to go without being seen holding an item before the
// same item counts as news again. Without this the per-match dedup is
// permanent, so an enemy who buys a second Ender Pearl twenty minutes into a
// game is silently ignored. It only restarts once they are actually seen
// holding something else, so simply keeping an item in hand never re-alerts.
constexpr Tick kImportantItemRepeatMs = 45000;
enum class VisibilityMode { RangeOnly, LineOfSight, CameraView };

// Where an alert is delivered. The overlay is glanceable but disappears; chat
// leaves a scrollback you can check after a fight.
enum class AlertOutput { Overlay, Chat, Both };
const char *alertOutputName(AlertOutput output);
// What the height HUD prints. The ceiling shown is always maximumPlayerY --
// the Y your feet reach standing on the highest legal block -- so the two
// numbers meeting means you are at the cap.
enum class HeightDisplay { Ratio, RatioRemaining, Remaining, Limit };
const char *heightDisplayName(HeightDisplay display);
enum class PlayerRejectReason {
  None,
  LocalPlayer,
  Teammate,
  Dead,
  Spectator,
  OutOfRange,
  NoLineOfSight,
  OutsideCameraView,
  UnknownVisibility,
  InvalidIdentity
};

const char *armorTierName(ArmorTier armor);
const char *potionName(PotionKind potion);
const char *visibilityModeName(VisibilityMode mode);
const char *playerRejectReasonName(PlayerRejectReason reason);

struct MessageSegment {
  std::string text;
  std::uint32_t argb = 0xFFE0E0E0U;
};

struct VisibleItem {
  std::string typeName;
  std::string displayName;
  int metadata = -1;
  bool enchanted = false;
};

std::string readableItemName(const VisibleItem &item);
bool isSword(const VisibleItem &item);
bool isKnockbackStick(const VisibleItem &item);
PotionKind classifyPotion(const VisibleItem &item);
SwordTier classifySwordTier(const VisibleItem &item);
ImportantItem classifyImportantItem(const VisibleItem &item);
const char *importantItemName(ImportantItem item);
// Stable identifier used as the config key for this item's on/off switch.
// Never reuse or rename one of these: an old config would silently apply a
// stored preference to the wrong item.
const char *importantItemKey(ImportantItem item);
// The colour this item is drawn in. Returns kUncolouredArgb for items where
// no colour reads as obviously "that item", which renders as normal body
// text rather than a wrong-looking guess.
std::uint32_t importantItemArgb(ImportantItem item);
std::uint32_t armorTierArgb(ArmorTier tier);
// Minecraft's sixteen chat colours are the only ones that survive the trip
// to chat, so alert colours are chosen from that palette and this maps one
// back to its formatting code. Anything outside the palette is body text.
const char *formattingCodeForArgb(std::uint32_t argb);
inline constexpr std::uint32_t kUncolouredArgb = 0xFFE0E0E0U;
bool isExplicitlyIgnoredHeldItem(const VisibleItem &item);

struct PlayerObservation {
  int entityId = -1;
  std::string identity;
  bool localPlayer = false;
  bool teammateKnown = false;
  bool teammate = false;
  bool alive = true;
  bool spectator = false;
  double distance = 0.0;
  TeamId team = TeamId::Unknown;
  bool teamAuthoritative = false;
  bool lineOfSightKnown = false;
  bool hasLineOfSight = false;
  bool cameraViewKnown = false;
  bool insideCameraView = false;
  ArmorTier armor = ArmorTier::None;
  // Any enchant on any worn piece. On Hypixel the only armour enchant a
  // player can have is the team's Protection upgrade, so the glint is a
  // direct read of an upgrade that is otherwise never announced to enemies.
  bool armorEnchanted = false;
  VisibleItem heldItem;
  bool usingItem = false;
};

struct PlayerAlert {
  enum class Kind { Armor, Upgrade, Consume, Item, Potion, KnockbackStick };
  Kind kind = Kind::Item;
  int entityId = -1;
  std::string text;
  std::vector<MessageSegment> segments;
  TeamId team = TeamId::Unknown;
};

struct PlayerAlertOptions {
  bool armor = false;
  bool upgrades = false;
  bool consumes = false;
  bool items = false;
  bool ignoreOwnTeam = true;
  // Per-item switches. `items` is still the master: turning it off silences
  // every item regardless of what is set here.
  std::array<bool, kImportantItemCount> itemEnabled = allItemAlertsEnabled();
  double maximumDistance = 32.0;
  Tick cooldownMs = 2500;
  std::size_t capacity = 128;
  VisibilityMode visibility = VisibilityMode::LineOfSight;

  bool itemAllowed(ImportantItem item) const {
    const auto index = static_cast<std::size_t>(item);
    return index < itemEnabled.size() ? itemEnabled[index] : true;
  }
};

struct PlayerMonitorDiagnostics {
  std::size_t emitted = 0;
  std::size_t cooldownRejected = 0;
  std::size_t potionDuplicates = 0;
  std::size_t knockbackDuplicates = 0;
  std::size_t sharpnessDuplicates = 0;
  std::size_t protectionDuplicates = 0;
  std::size_t observedSwordGlints = 0;
  std::size_t observedArmorGlints = 0;
  std::size_t classifiedPotions = 0;
  std::size_t classifiedKnockback = 0;
  std::size_t unknownItems = 0;
  std::size_t ignoredItems = 0;
  std::size_t itemDuplicates = 0;
};

struct PlayerRejectionDetail {
  int entityId = -1;
  std::string identity;
  PlayerRejectReason reason = PlayerRejectReason::None;
};

class PlayerMonitor {
public:
  std::vector<PlayerAlert> observe(const std::vector<PlayerObservation> &players,
                                   const PlayerAlertOptions &options,
                                   Tick now, TeamTracker *teams = nullptr);
  std::size_t trackedCount() const { return m_players.size(); }
  const std::array<std::size_t, 10> &rejectionCounts() const {
    return m_rejectionCounts;
  }
  const PlayerMonitorDiagnostics &diagnostics() const { return m_diagnostics; }
  const std::vector<PlayerRejectionDetail> &rejectionDetails() const {
    return m_rejectionDetails;
  }
  void reset();

  // Drops everything we've learned about one player (their dedup state for
  // held-item/potion/knockback-stick alerts, and their armor/sword tier
  // baseline) without touching anyone else. Intended for when chat tells us
  // the player died: on respawn their gear resets, so re-seeding them lets
  // the normal first-sight alert logic report their post-respawn loadout
  // instead of staying silent because the old loadout was already
  // reported. Safe to call with a name that isn't currently tracked.
  // Full removal: the player is gone as far as we are concerned (left the
  // server, match reset). The next sighting is treated as a first sighting.
  void forgetPlayer(const std::string &identity);

  // Death/respawn reset: clears only the state a Bedwars death actually takes
  // away (sword tier, held item, per-match item flags) and deliberately keeps
  // the armor baseline, because armor survives death and re-alerting it after
  // every kill is noise.
  void forgetPlayerLoadout(const std::string &identity);

private:
  struct TrackedPlayer {
    std::string identity;
    int entityId = -1;
    ArmorTier armor = ArmorTier::None;
    VisibleItem heldItem;
    bool usingItem = false;
    SwordTier highestSwordTier = SwordTier::None;
    // Tick of the last alert per important item, 0 meaning never alerted.
    // This used to be a plain bool, which made every item a once-per-match
    // event and was the reason re-bought items went unreported.
    std::array<Tick, static_cast<std::size_t>(ImportantItem::Count)>
        importantItemAlertedAt{};
    // The cooldown remembers WHICH event it last fired for, not just when.
    // Keyed on time alone, two different things happening at once -- a team
    // revealing Sharpness and Protection in the same second, say -- collapsed
    // into one alert, because the second was read as a repeat of the first.
    struct AlertMemory {
      std::string event;
      Tick at = 0;
    };
    Tick lastSeen = 0;
    std::array<AlertMemory, 6> lastAlert{};
  };
  std::unordered_map<std::string, TrackedPlayer> m_players;
  std::unordered_map<int, std::string> m_entityOwners;
  std::array<std::size_t, 10> m_rejectionCounts{};
  PlayerMonitorDiagnostics m_diagnostics;
  std::vector<PlayerRejectionDetail> m_rejectionDetails;
};

struct HeightResult {
  int currentY = 0;
  std::optional<int> limit;
  std::optional<int> remaining;
};

HeightResult evaluateHeight(double playerY, std::optional<int> configuredLimit);

struct MapHeightEntry {
  const char *canonicalName;
  int maximumPlayerY;
  int maximumPlacementY;
};

struct MapHeightResolution {
  std::string canonicalName;
  std::optional<int> maximumPlayerY;
  std::optional<int> maximumPlacementY;
  bool overridden = false;
};

const std::vector<MapHeightEntry> &builtInMapHeights();
std::string normalizeMapName(const std::string &name);
std::optional<std::string> parseMapScoreboardLine(const std::string &line);
MapHeightResolution resolveMapHeight(const std::string &mapName);

struct NotificationRecord {
  Tick created = 0;
  Tick durationMs = 3000;
  std::string key;
};

class NotificationTimeline {
public:
  void add(const std::string &key, Tick now, Tick durationMs,
           std::size_t maximumVisible);
  void expire(Tick now);
  std::size_t size() const { return m_records.size(); }
  const std::vector<NotificationRecord> &records() const { return m_records; }

private:
  std::vector<NotificationRecord> m_records;
};

enum class HudId : std::size_t {
  EventTimer,
  Height,
  Resource,
  TeamState,
  Count
};

constexpr std::size_t kHudCount = static_cast<std::size_t>(HudId::Count);
const char *hudName(HudId hud);

struct HudLayout {
  bool visible = false;
  float x = 0.02F;
  float y = 0.20F;
  float scale = 1.0F;
};

HudLayout sanitizeHudLayout(const HudLayout &layout, float normalizedWidth,
                            float normalizedHeight);

enum class ShopCurrency { Iron, Gold, Diamond, Emerald, Unknown };

struct ShopOffer {
  bool valid = false;
  int cost = 0;
  ShopCurrency currency = ShopCurrency::Unknown;
  bool affordable = false;
  bool permanent = false;
  bool alreadyOwned = false;
  std::string itemName;
};

ShopOffer evaluateShopOffer(const std::string &itemName,
                            const std::vector<std::string> &lore,
                            const ResourceSnapshot &resources);
bool shouldBlockDuplicatePurchase(const ShopOffer &offer,
                                  bool preventionEnabled);

std::string stripFormatting(const std::string &text);
std::string normalizeText(const std::string &text);

} // namespace OVson::Bedwars
