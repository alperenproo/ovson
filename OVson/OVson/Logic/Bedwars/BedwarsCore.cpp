#include "BedwarsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace OVson::Bedwars {
namespace {

constexpr std::array<const char *, kModuleCount> kModuleNames = {
    "Event Timers",   "Shop Helper",      "Height Overlay",
    "Upgrade Alerts", "Consume Alerts",   "Pickup Alerts",
    "Armor Alerts",   "Trap Notifier",    "Resource Tracker",
    "Item Alerts",    "Upgrade HUD"};

constexpr std::array<const char *, kModuleCount> kModuleKeys = {
    "eventTimers",   "shopHelper",      "heightOverlay", "upgradeAlerts",
    "consumeAlerts", "pickupAlerts",    "armorAlerts",   "trapNotifier",
    "resourceTracker", "itemAlerts",    "upgradeHud"};

std::size_t indexOf(Module module) {
  const auto index = static_cast<std::size_t>(module);
  return index < kModuleCount ? index : 0;
}

bool contains(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

int resourceIndex(ShopCurrency currency) {
  switch (currency) {
  case ShopCurrency::Iron:
    return static_cast<int>(Resource::Iron);
  case ShopCurrency::Gold:
    return static_cast<int>(Resource::Gold);
  case ShopCurrency::Diamond:
    return static_cast<int>(Resource::Diamond);
  case ShopCurrency::Emerald:
    return static_cast<int>(Resource::Emerald);
  default:
    return -1;
  }
}

bool isConsumable(const std::string &item) {
  const std::string normalized = normalizeText(item);
  return contains(normalized, "potion") || contains(normalized, "milk") ||
         contains(normalized, "golden apple") || contains(normalized, "apple");
}

int armorRank(ArmorTier armor) { return static_cast<int>(armor); }

std::size_t teamIndex(TeamId team) {
  const auto index = static_cast<std::size_t>(team);
  return index < kStandardTeamCount ? index : kStandardTeamCount;
}

std::string playerText(const PlayerObservation &player,
                       const std::string &tail) {
  return player.identity + tail;
}

// The player's name in their team colour, then the rest of the sentence, with
// the thing the alert is actually about given its own colour so it is
// readable at a glance instead of being one more white word in a white
// sentence.
std::vector<MessageSegment> playerItemSegments(const PlayerObservation &player,
                                               const std::string &prefix,
                                               const std::string &label,
                                               std::uint32_t labelArgb) {
  return {{player.identity, teamArgb(player.team)},
          {prefix, kUncolouredArgb},
          {label, labelArgb}};
}

} // namespace

const char *teamName(TeamId team) {
  static constexpr std::array<const char *, kTeamCount> names = {
      "Red", "Blue", "Green", "Yellow", "Aqua", "White", "Pink",
      "Gray", "Unknown"};
  const auto index = static_cast<std::size_t>(team);
  return names[index < names.size() ? index : names.size() - 1];
}

const char *teamFormattingCode(TeamId team) {
  static constexpr std::array<const char *, kTeamCount> codes = {
      "c", "9", "a", "e", "b", "f", "d", "7", "7"};
  const auto index = static_cast<std::size_t>(team);
  return codes[index < codes.size() ? index : codes.size() - 1];
}

std::uint32_t teamArgb(TeamId team) {
  static constexpr std::array<std::uint32_t, kTeamCount> colors = {
      0xFFFF5555U, 0xFF5555FFU, 0xFF55FF55U, 0xFFFFFF55U, 0xFF55FFFFU,
      0xFFFFFFFFU, 0xFFFF55FFU, 0xFFAAAAAAU, 0xFFC8C8D0U};
  const auto index = static_cast<std::size_t>(team);
  return colors[index < colors.size() ? index : colors.size() - 1];
}

TeamId normalizeTeam(const std::string &value) {
  std::string text = normalizeText(value);
  if (text.empty())
    return TeamId::Unknown;
  if (text.size() == 1) {
    switch (text[0]) {
    case 'r': return TeamId::Red;
    case 'b': return TeamId::Blue;
    case 'g': return TeamId::Green;
    case 'y': return TeamId::Yellow;
    case 'a': return TeamId::Aqua;
    case 'w': return TeamId::White;
    case 'p': return TeamId::Pink;
    default: break;
    }
  }
  if (text == "red" || text.find("red team") != std::string::npos)
    return TeamId::Red;
  if (text == "blue" || text.find("blue team") != std::string::npos)
    return TeamId::Blue;
  if (text == "green" || text.find("green team") != std::string::npos)
    return TeamId::Green;
  if (text == "yellow" || text.find("yellow team") != std::string::npos)
    return TeamId::Yellow;
  if (text == "aqua" || text == "cyan" || text == "light blue" ||
      text.find("aqua team") != std::string::npos)
    return TeamId::Aqua;
  if (text == "white" || text.find("white team") != std::string::npos)
    return TeamId::White;
  if (text == "pink" || text == "magenta" ||
      text.find("pink team") != std::string::npos)
    return TeamId::Pink;
  if (text == "gray" || text == "grey" || text == "silver" ||
      text == "light gray" || text == "light grey" ||
      text.find("gray team") != std::string::npos ||
      text.find("grey team") != std::string::npos)
    return TeamId::Gray;
  return TeamId::Unknown;
}

std::optional<TeamId> parseDestroyedBedTeam(const std::string &message) {
  const std::string text = normalizeText(message);
  if (text.find("bed destruction") == std::string::npos)
    return std::nullopt;
  std::optional<TeamId> result;
  for (std::size_t i = 0; i < kStandardTeamCount; ++i) {
    const TeamId team = static_cast<TeamId>(i);
    const std::string marker =
        normalizeText(std::string(teamName(team)) + " bed");
    if (text.find(marker) == std::string::npos)
      continue;
    if (result)
      return std::nullopt;
    result = team;
  }
  return result;
}

namespace {
bool looksLikeUsername(const std::string &candidate) {
  if (candidate.empty() || candidate.size() > 16)
    return false;
  for (const unsigned char c : candidate) {
    if (!(std::isalnum(c) != 0 || c == '_'))
      return false;
  }
  return true;
}
} // namespace

std::optional<std::string> parseDeathVictim(const std::string &message) {
  // Hypixel Bedwars kill-feed lines are unprefixed, e.g. "Steve was killed
  // by Alex.", "Steve fell into the void.", "Steve blew up.". The victim's
  // name is always the single token immediately before one of these
  // phrases. Matching is case-insensitive (server wording can vary in
  // capitalization) but the extracted name keeps its original casing so it
  // lines up with the identity strings PlayerMonitor already tracks.
  static const char *kDeathPhrases[] = {
      " was killed by ",         " was slain by ",
      " was shot by ",           " was fireballed by ",
      " was pummeled by ",       " was blown up by ",
      " blew up",                " was killed",
      " fell into the void",     " fell out of the world",
      " fell from a high place", " fell off ",
      " tried to swim in lava",  " went up in flames",
      " walked into a cactus",   " burned to death",
      " suffocated in a wall",   " starved to death",
      " withered away",          " was struck by lightning",
      " was doomed to fall",     " was pricked to death",
      " was squashed by a falling anvil",
      " was killed by magic",    " discovered floor was lava",
      " died",
  };

  const std::string stripped = stripFormatting(message);
  std::string lowered = stripped;
  for (char &c : lowered)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  std::size_t bestIndex = std::string::npos;
  for (const char *phrase : kDeathPhrases) {
    const std::size_t index = lowered.find(phrase);
    if (index != std::string::npos &&
        (bestIndex == std::string::npos || index < bestIndex))
      bestIndex = index;
  }
  if (bestIndex == std::string::npos || bestIndex == 0)
    return std::nullopt;

  std::string candidate = stripped.substr(0, bestIndex);
  while (!candidate.empty() &&
         std::isspace(static_cast<unsigned char>(candidate.back())))
    candidate.pop_back();

  if (!looksLikeUsername(candidate))
    return std::nullopt;
  return candidate;
}

TeamTracker::TeamTracker() { reset(); }

void TeamTracker::reset() {
  m_teams = {};
  for (std::size_t i = 0; i < m_teams.size(); ++i) {
    m_teams[i].id = static_cast<TeamId>(i);
    m_teams[i].entityIds.fill(-1);
  }
  m_players = {};
  m_playerCount = 0;
}

void TeamTracker::observePlayer(const std::string &identity, int entityId,
                                TeamId team, bool authoritative, Tick now,
                                const std::string &source) {
  if (identity.empty() || entityId < 0)
    return;
  std::size_t slot = m_playerCount;
  for (std::size_t i = 0; i < m_playerCount; ++i) {
    if (m_players[i].identity == identity ||
        m_players[i].entityId == entityId) {
      slot = i;
      break;
    }
  }
  if (slot == m_playerCount) {
    if (m_playerCount < m_players.size()) {
      ++m_playerCount;
    } else {
      slot = static_cast<std::size_t>(std::min_element(
                 m_players.begin(), m_players.end(),
                 [](const PlayerBinding &a, const PlayerBinding &b) {
                   return a.lastSeen < b.lastSeen;
                 }) - m_players.begin());
    }
  }
  PlayerBinding &binding = m_players[slot];
  const bool mayReplace = binding.identity != identity || authoritative ||
                          !binding.authoritative || binding.team == team;
  if (mayReplace) {
    binding.identity = identity;
    binding.entityId = entityId;
    binding.team = team;
    binding.authoritative = authoritative;
  }
  binding.lastSeen = now;

  const std::size_t index = teamIndex(binding.team);
  if (index >= m_teams.size())
    return;
  TeamState &state = m_teams[index];
  std::size_t playerSlot = state.playerCount;
  for (std::size_t i = 0; i < state.playerCount; ++i) {
    if (state.playerNames[i] == identity || state.entityIds[i] == entityId) {
      playerSlot = i;
      break;
    }
  }
  if (playerSlot == state.playerCount && state.playerCount < state.playerNames.size())
    ++state.playerCount;
  if (playerSlot < state.playerNames.size()) {
    state.playerNames[playerSlot] = identity;
    state.entityIds[playerSlot] = entityId;
  }
  state.latestObservation = now;
  state.latestSource = source.substr(0, 64);
}

bool TeamTracker::observeSharpness(TeamId team, Tick now,
                                   const std::string &source) {
  const std::size_t index = teamIndex(team);
  if (index >= m_teams.size())
    return false;
  TeamState &state = m_teams[index];
  state.latestObservation = now;
  state.latestSource = source.substr(0, 64);
  if (state.sharpnessObserved)
    return false;
  state.sharpnessObserved = true;
  return true;
}

bool TeamTracker::observeProtection(TeamId team, Tick now,
                                    const std::string &source) {
  // Protection is a team-wide purchase, so the first enemy seen wearing an
  // enchanted piece reveals it for their whole team and everyone after them
  // is redundant. Kept separate from the Sharpness flag on purpose: a team
  // can buy either upgrade first, and folding them together would let
  // whichever arrived first hide the other.
  const std::size_t index = teamIndex(team);
  if (index >= m_teams.size())
    return false;
  TeamState &state = m_teams[index];
  state.latestObservation = now;
  state.latestSource = source.substr(0, 64);
  if (state.protectionObserved)
    return false;
  state.protectionObserved = true;
  return true;
}

void TeamTracker::observeBed(TeamId team, BedState state, Tick now,
                             const std::string &source) {
  const std::size_t index = teamIndex(team);
  if (index >= m_teams.size())
    return;
  m_teams[index].bed = state;
  m_teams[index].latestObservation = now;
  m_teams[index].latestSource = source.substr(0, 64);
}

void TeamTracker::expire(Tick now, Tick maximumAgeMs) {
  const Tick age = std::clamp<Tick>(maximumAgeMs, 1000, 120000);
  std::size_t write = 0;
  for (std::size_t read = 0; read < m_playerCount; ++read) {
    const auto &binding = m_players[read];
    if (now >= binding.lastSeen && now - binding.lastSeen <= age)
      m_players[write++] = binding;
  }
  for (std::size_t i = write; i < m_playerCount; ++i)
    m_players[i] = {};
  m_playerCount = write;
  for (TeamState &state : m_teams) {
    state.playerCount = 0;
    state.playerNames.fill({});
    state.entityIds.fill(-1);
  }
  for (std::size_t i = 0; i < m_playerCount; ++i) {
    const auto &binding = m_players[i];
    const std::size_t index = teamIndex(binding.team);
    if (index >= m_teams.size())
      continue;
    TeamState &state = m_teams[index];
    if (state.playerCount < state.playerNames.size()) {
      state.playerNames[state.playerCount] = binding.identity;
      state.entityIds[state.playerCount] = binding.entityId;
      ++state.playerCount;
    }
  }
}

TeamId TeamTracker::playerTeam(const std::string &identity) const {
  for (std::size_t i = 0; i < m_playerCount; ++i)
    if (m_players[i].identity == identity)
      return m_players[i].team;
  return TeamId::Unknown;
}

const TeamState &TeamTracker::team(TeamId id) const {
  static const TeamState unknown{};
  const std::size_t index = teamIndex(id);
  return index < m_teams.size() ? m_teams[index] : unknown;
}

const char *moduleName(Module module) { return kModuleNames[indexOf(module)]; }
const char *moduleKey(Module module) { return kModuleKeys[indexOf(module)]; }
bool isModuleAvailable(Module module) {
  return module != Module::ShopHelper;
}

LifecycleTransition Context::observe(const SessionObservation &observation,
                                     Tick now) {
  LifecycleTransition transition;
  const GamePhase previousPhase = m_phase;
  auto doReset = [&](const char *reason, bool preserveGameClock = false) {
    const Tick gameStart = m_gameStart;
    const bool gameClockKnown = m_gameClockKnown;
    const GamePhase phase = m_phase;
    reset(reason);
    if (preserveGameClock) {
      m_gameStart = gameStart;
      m_gameClockKnown = gameClockKnown;
      m_phase = phase;
    }
    transition.reset = true;
    transition.preservedGameClock = preserveGameClock;
    transition.reason = reason;
  };

  if (m_haveObservation && now < m_lastTick) {
    doReset("clock regression");
  }

  if (m_haveObservation && !transition.reset) {
    if (!observation.featureEnabled && m_last.featureEnabled)
      doReset("feature disabled");
    else if (!observation.worldValid && m_last.worldValid)
      doReset("world unavailable");
    else if (!observation.playerValid && m_last.playerValid)
      doReset("local player unavailable", true);
    else if (observation.worldValid && m_last.worldValid &&
             observation.worldIdentity != m_last.worldIdentity)
      doReset("world changed");
    else if (observation.dimension != m_last.dimension)
      doReset("dimension changed");
    else if (observation.localEntityId != m_last.localEntityId)
      doReset("local entity changed", true);
    else if (observation.dead && !m_last.dead)
      doReset("death", true);
    else if (!observation.dead && m_last.dead)
      doReset("respawn", true);
    else if (!observation.onHypixel && m_last.onHypixel)
      doReset("left server");
    else if (!observation.bedwarsMode && m_last.bedwarsMode)
      doReset("left Bedwars");
    else if (observation.preGame && !m_last.preGame)
      doReset("returned to pre-game");
  }

  GamePhase next = GamePhase::Inactive;
  if (observation.featureEnabled && observation.worldValid &&
      observation.playerValid && observation.onHypixel &&
      observation.bedwarsMode && !observation.replay) {
    if (observation.preGame)
      next = GamePhase::PreGame;
    else if (observation.inGame && observation.dead)
      next = GamePhase::Spectator;
    else if (observation.inGame)
      next = GamePhase::Active;
    else
      next = GamePhase::Lobby;
  }

  const GamePhase previous = previousPhase;
  // A first observation in an already-running match is late injection, not
  // evidence of the match start time. Only the observed pre-game -> active
  // transition establishes the fallback clock.
  if (next == GamePhase::Active && m_gameStart == 0 && m_haveObservation &&
      previousPhase == GamePhase::PreGame) {
    m_gameStart = now;
    m_gameClockKnown = true;
    transition.gameStarted = true;
  }
  if ((previous == GamePhase::Active || previous == GamePhase::Spectator) &&
      next != GamePhase::Active && next != GamePhase::Spectator) {
    transition.gameEnded = true;
  }

  m_phase = next;
  m_last = observation;
  m_lastTick = now;
  m_haveObservation = true;
  return transition;
}

void Context::reset(const char *reason) {
  m_phase = GamePhase::Inactive;
  m_gameStart = 0;
  m_gameClockKnown = false;
  m_haveObservation = false;
  m_last = {};
  m_lastTick = 0;
  ++m_generation;
  m_lastResetReason = reason ? reason : "manual";
}

Tick Context::elapsed(Tick now) const {
  if (m_gameStart == 0 || now < m_gameStart)
    return 0;
  return now - m_gameStart;
}

const std::array<TimedEvent, 7> &EventSchedule::standardEvents() {
  static constexpr std::array<TimedEvent, 7> events = {{
      {TimedEventKind::DiamondTwo, 6ULL * 60ULL * 1000ULL, "Diamond II"},
      {TimedEventKind::EmeraldTwo, 12ULL * 60ULL * 1000ULL, "Emerald II"},
      {TimedEventKind::DiamondThree, 18ULL * 60ULL * 1000ULL, "Diamond III"},
      {TimedEventKind::EmeraldThree, 24ULL * 60ULL * 1000ULL, "Emerald III"},
      {TimedEventKind::BedDestruction, 30ULL * 60ULL * 1000ULL,
       "Bed Destruction"},
      {TimedEventKind::SuddenDeath, 40ULL * 60ULL * 1000ULL, "Sudden Death"},
      {TimedEventKind::GameEnd, 50ULL * 60ULL * 1000ULL, "Game End"},
  }};
  return events;
}

std::vector<EventCountdown> EventSchedule::countdowns(Tick elapsedMs,
                                                       bool onlyNext) {
  std::vector<EventCountdown> result;
  for (const auto &event : standardEvents()) {
    if (elapsedMs >= event.offsetMs)
      continue;
    result.push_back(
        {event.kind, event.label, event.offsetMs - elapsedMs, false});
    if (onlyNext)
      break;
  }
  return result;
}

std::optional<EventCountdown>
EventSchedule::parseScoreboardLine(const std::string &line) {
  const std::string text = normalizeText(line);
  TimedEventKind kind;
  std::string label;
  if (contains(text, "diamond ii")) {
    kind = TimedEventKind::DiamondTwo;
    label = "Diamond II";
  } else if (contains(text, "emerald ii")) {
    kind = TimedEventKind::EmeraldTwo;
    label = "Emerald II";
  } else if (contains(text, "diamond iii")) {
    kind = TimedEventKind::DiamondThree;
    label = "Diamond III";
  } else if (contains(text, "emerald iii")) {
    kind = TimedEventKind::EmeraldThree;
    label = "Emerald III";
  } else if (contains(text, "bed destruction") || contains(text, "beds gone")) {
    kind = TimedEventKind::BedDestruction;
    label = "Bed Destruction";
  } else if (contains(text, "sudden death")) {
    kind = TimedEventKind::SuddenDeath;
    label = "Sudden Death";
  } else if (contains(text, "game end") || contains(text, "game over")) {
    kind = TimedEventKind::GameEnd;
    label = "Game End";
  } else {
    return std::nullopt;
  }

  for (std::size_t i = 0; i < text.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(text[i])))
      continue;
    std::size_t colon = text.find(':', i);
    if (colon == std::string::npos || colon - i > 2 || colon + 2 >= text.size())
      continue;
    std::size_t end = colon + 1;
    while (end < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[end])))
      ++end;
    if (end == colon + 1 || end - colon - 1 > 2)
      continue;
    try {
      const int minutes = std::stoi(text.substr(i, colon - i));
      const int seconds = std::stoi(text.substr(colon + 1, end - colon - 1));
      if (minutes < 0 || seconds < 0 || seconds > 59)
        return std::nullopt;
      const Tick remaining =
          (static_cast<Tick>(minutes) * 60ULL + static_cast<Tick>(seconds)) *
          1000ULL;
      return EventCountdown{kind, label, remaining, remaining == 0};
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

const char *resourceName(Resource resource) {
  static constexpr std::array<const char *, kResourceCount> names = {
      "Iron", "Gold", "Diamond", "Emerald"};
  const auto index = static_cast<std::size_t>(resource);
  return names[index < names.size() ? index : 0];
}

std::vector<ResourceDelta>
ResourceMonitor::observe(const ResourceSnapshot &snapshot) {
  std::vector<ResourceDelta> deltas;
  if (!m_previous) {
    m_previous = snapshot;
    return deltas;
  }

  const ResourceSnapshot previous = *m_previous;
  m_previous = snapshot;
  if (snapshot.inventoryValid && previous.inventoryValid) {
    for (std::size_t i = 0; i < kResourceCount; ++i) {
      const int inventoryDelta = snapshot.inventory[i] - previous.inventory[i];
      if (inventoryDelta <= 0)
        continue;
      if ((snapshot.containerOpen || previous.containerOpen) &&
          (!snapshot.enderChestValid || !previous.enderChestValid))
        continue;
      const int chestDelta =
          snapshot.enderChestValid && previous.enderChestValid
              ? snapshot.enderChest[i] - previous.enderChest[i]
              : 0;
      if ((snapshot.containerOpen || previous.containerOpen) &&
          chestDelta == -inventoryDelta)
        continue;
      deltas.push_back(
          {static_cast<Resource>(i), inventoryDelta, false});
    }
  }
  if (snapshot.enderChestValid && previous.enderChestValid &&
      !snapshot.containerOpen && !previous.containerOpen) {
    for (std::size_t i = 0; i < kResourceCount; ++i) {
      const int delta = snapshot.enderChest[i] - previous.enderChest[i];
      if (delta > 0)
        deltas.push_back({static_cast<Resource>(i), delta, true});
    }
  }
  return deltas;
}

void ResourceMonitor::reset() { m_previous.reset(); }

bool ChatMonitor::isDuplicate(const std::string &normalized, Tick now) {
  if (now < m_lastMessageTick) {
    m_lastMessage.clear();
    m_lastMessageTick = 0;
  }
  const bool duplicate = normalized == m_lastMessage &&
                         now - m_lastMessageTick <= 1000;
  if (!duplicate) {
    m_lastMessage = normalized;
    m_lastMessageTick = now;
  }
  return duplicate;
}

std::optional<ChatSignal> ChatMonitor::observe(const std::string &message,
                                               Tick now) {
  const std::string text = normalizeText(message);
  if (text.empty() || isDuplicate(text, now))
    return std::nullopt;

  ChatSignal signal;
  if (contains(text, "trap triggered") || contains(text, "trap was set off") ||
      contains(text, "trap has been triggered")) {
    signal.kind = ChatSignal::Kind::TrapTriggered;
    signal.label = "Trap triggered";
    m_knownTrap.clear();
    m_trapState = TrapState::Unknown;
    return signal;
  }
  if (contains(text, "purchased") && contains(text, "trap")) {
    signal.kind = ChatSignal::Kind::TrapQueued;
    signal.label = "Trap queued";
    m_knownTrap = signal.label;
    m_trapState = TrapState::Queued;
    return signal;
  }
  if (contains(text, "no traps") || contains(text, "don't have any trap") ||
      contains(text, "do not have any trap")) {
    signal.kind = ChatSignal::Kind::TrapMissing;
    signal.label = "No trap queued";
    m_knownTrap.clear();
    m_trapState = TrapState::Missing;
    return signal;
  }

  if (!contains(text, "purchased") && !contains(text, "team upgrade"))
    return std::nullopt;

  signal.kind = ChatSignal::Kind::Upgrade;
  if (contains(text, "sharpened swords") || contains(text, "sharpness")) {
    signal.upgrade = UpgradeKind::Sharpness;
    signal.level = m_upgrades.sharpness = 1;
    m_upgrades.sharpnessKnown = true;
    signal.label = "Sharpness";
  } else if (contains(text, "reinforced armor") ||
             contains(text, "protection")) {
    signal.upgrade = UpgradeKind::Protection;
    signal.level = std::min(4, ++m_upgrades.protection);
    m_upgrades.protectionKnown = true;
    signal.label = "Protection";
  } else if (contains(text, "maniac miner") || contains(text, "haste")) {
    signal.upgrade = UpgradeKind::Haste;
    signal.level = std::min(2, ++m_upgrades.haste);
    m_upgrades.hasteKnown = true;
    signal.label = "Haste";
  } else if (contains(text, "iron forge") || contains(text, "golden forge") ||
             contains(text, "emerald forge") || contains(text, "molten forge")) {
    signal.upgrade = UpgradeKind::Forge;
    signal.level = std::min(4, ++m_upgrades.forge);
    m_upgrades.forgeKnown = true;
    signal.label = "Forge";
  } else if (contains(text, "heal pool")) {
    signal.upgrade = UpgradeKind::HealPool;
    signal.level = 1;
    signal.label = "Heal Pool";
    m_upgrades.healPool = true;
    m_upgrades.healPoolKnown = true;
  } else if (contains(text, "dragon buff")) {
    signal.upgrade = UpgradeKind::DragonBuff;
    signal.level = 1;
    signal.label = "Dragon Buff";
    m_upgrades.dragonBuff = true;
    m_upgrades.dragonBuffKnown = true;
  } else if (contains(text, "feather falling")) {
    signal.upgrade = UpgradeKind::FeatherFalling;
    signal.level = std::min(2, ++m_upgrades.featherFalling);
    m_upgrades.featherFallingKnown = true;
    signal.label = "Feather Falling";
  } else {
    return std::nullopt;
  }
  return signal;
}

void ChatMonitor::reset() {
  m_upgrades = {};
  m_knownTrap.clear();
  m_trapState = TrapState::Unknown;
  m_lastMessage.clear();
  m_lastMessageTick = 0;
}

const char *armorTierName(ArmorTier armor) {
  static constexpr std::array<const char *, 5> names = {
      "No", "Leather", "Chain", "Iron", "Diamond"};
  const auto index = static_cast<std::size_t>(armor);
  return names[index < names.size() ? index : 0];
}

namespace {

// Minecraft's sixteen chat colours. Alert colours are chosen from this table
// and nowhere else: the overlay can draw any ARGB it likes, but chat can only
// express these, so picking a colour outside the table would make the same
// alert look different on the two surfaces.
struct PaletteEntry {
  std::uint32_t argb;
  const char *code;
};

constexpr std::array<PaletteEntry, 16> kPalette = {{
    {0xFF000000U, "0"}, {0xFF0000AAU, "1"}, {0xFF00AA00U, "2"},
    {0xFF00AAAAU, "3"}, {0xFFAA0000U, "4"}, {0xFFAA00AAU, "5"},
    {0xFFFFAA00U, "6"}, {0xFFAAAAAAU, "7"}, {0xFF555555U, "8"},
    {0xFF5555FFU, "9"}, {0xFF55FF55U, "a"}, {0xFF55FFFFU, "b"},
    {0xFFFF5555U, "c"}, {0xFFFF55FFU, "d"}, {0xFFFFFF55U, "e"},
    {0xFFFFFFFFU, "f"},
}};

constexpr std::uint32_t kDarkAqua = 0xFF00AAAAU;
constexpr std::uint32_t kDarkPurple = 0xFFAA00AAU;
constexpr std::uint32_t kGold = 0xFFFFAA00U;
constexpr std::uint32_t kGray = 0xFFAAAAAAU;
constexpr std::uint32_t kDarkGray = 0xFF555555U;
constexpr std::uint32_t kBlue = 0xFF5555FFU;
constexpr std::uint32_t kGreen = 0xFF55FF55U;
constexpr std::uint32_t kAqua = 0xFF55FFFFU;
constexpr std::uint32_t kRed = 0xFFFF5555U;
constexpr std::uint32_t kLightPurple = 0xFFFF55FFU;
constexpr std::uint32_t kYellow = 0xFFFFFF55U;
constexpr std::uint32_t kWhite = 0xFFFFFFFFU;

} // namespace

const char *formattingCodeForArgb(std::uint32_t argb) {
  for (const auto &entry : kPalette) {
    if (entry.argb == argb)
      return entry.code;
  }
  return "f";
}

std::uint32_t armorTierArgb(ArmorTier tier) {
  switch (tier) {
  case ArmorTier::Diamond: return kAqua;
  case ArmorTier::Iron: return kWhite;
  case ArmorTier::Chain: return kGray;
  case ArmorTier::Leather: return kGold;
  default: return kUncolouredArgb;
  }
}

std::uint32_t importantItemArgb(ImportantItem item) {
  switch (item) {
  // Material colours, so the alert reads at a glance the way the item does
  // in an inventory.
  case ImportantItem::DiamondSword:
  case ImportantItem::DiamondPickaxe: return kAqua;
  case ImportantItem::IronSword: return kWhite;
  case ImportantItem::StoneSword: return kGray;
  case ImportantItem::GoldenPickaxe:
  case ImportantItem::GoldenApple: return kYellow;
  case ImportantItem::EnchantedBow: return kLightPurple;
  case ImportantItem::Bow: return kGold;
  case ImportantItem::KnockbackStick: return kGold;
  case ImportantItem::Tnt: return kRed;
  case ImportantItem::Fireball: return kGold;
  case ImportantItem::EnderPearl: return kDarkAqua;
  case ImportantItem::Obsidian: return kDarkPurple;
  case ImportantItem::WaterBucket: return kBlue;
  case ImportantItem::Milk: return kWhite;
  case ImportantItem::SpeedPotion: return kAqua;
  case ImportantItem::JumpPotion: return kGreen;
  case ImportantItem::InvisibilityPotion: return kDarkGray;
  case ImportantItem::DreamDefender: return kGreen;
  // A Bridge Egg is an ordinary egg and reads as nothing in particular, so
  // it stays body-coloured rather than wearing a colour that means nothing.
  default: return kUncolouredArgb;
  }
}

const char *potionName(PotionKind potion) {
  switch (potion) {
  case PotionKind::Speed: return "Speed";
  case PotionKind::Jump: return "Jump";
  case PotionKind::Invisibility: return "Invisibility";
  default: return "Unknown";
  }
}

const char *alertOutputName(AlertOutput output) {
  switch (output) {
  case AlertOutput::Chat: return "Chat";
  case AlertOutput::Both: return "Chat + Alert";
  default: return "Alert";
  }
}

const char *heightDisplayName(HeightDisplay display) {
  switch (display) {
  case HeightDisplay::RatioRemaining: return "Y / Limit + Left";
  case HeightDisplay::Remaining: return "Blocks Left";
  case HeightDisplay::Limit: return "Limit Only";
  default: return "Y / Limit";
  }
}

const char *visibilityModeName(VisibilityMode mode) {
  switch (mode) {
  case VisibilityMode::RangeOnly: return "Range Only";
  case VisibilityMode::LineOfSight: return "Line of Sight";
  case VisibilityMode::CameraView: return "Camera View";
  default: return "Range Only";
  }
}

const char *playerRejectReasonName(PlayerRejectReason reason) {
  switch (reason) {
  case PlayerRejectReason::LocalPlayer: return "local player";
  case PlayerRejectReason::Teammate: return "teammate";
  case PlayerRejectReason::Dead: return "dead";
  case PlayerRejectReason::Spectator: return "spectator";
  case PlayerRejectReason::OutOfRange: return "out of range";
  case PlayerRejectReason::NoLineOfSight: return "no line of sight";
  case PlayerRejectReason::OutsideCameraView: return "outside camera view";
  case PlayerRejectReason::UnknownVisibility: return "visibility unavailable";
  case PlayerRejectReason::InvalidIdentity: return "invalid identity";
  default: return "accepted";
  }
}

std::string readableItemName(const VisibleItem &item) {
  return importantItemName(classifyImportantItem(item));
}

bool isSword(const VisibleItem &item) {
  return classifySwordTier(item) != SwordTier::None;
}

bool isKnockbackStick(const VisibleItem &item) {
  const std::string type = normalizeText(item.typeName);
  if (type != "stick" && type != "item stick")
    return false;
  // An enchanted stick is enough on its own. The shop item is a plain stick
  // with Knockback on it, and the enchantment is the whole point of carrying
  // one -- there is no other reason to hold an enchanted stick in a Bedwars
  // game. Requiring the display name to also read "Knockback Stick" made this
  // depend on the server bothering to rename the stack, which it does not
  // always do, so the alert silently never fired.
  if (item.enchanted)
    return true;
  const std::string name = normalizeText(item.displayName);
  return name == "knockback stick" || name == "kb stick" ||
         name.rfind("knockback stick ", 0) == 0;
}

PotionKind classifyPotion(const VisibleItem &item) {
  const std::string type = normalizeText(item.typeName);
  const std::string display = normalizeText(item.displayName);
  // 1.8's ItemPotion overrides getUnlocalizedName(ItemStack) to return an
  // already-translated string, so the type can arrive as anything from
  // "potion" to "potion of swiftness" depending on the client's language
  // files -- and a renamed stack only shows up in the display name. Accept
  // the word from either side rather than betting on one of them.
  const bool looksLikePotion = type.find("potion") != std::string::npos ||
                               display.find("potion") != std::string::npos;
  if (!looksLikePotion)
    return PotionKind::Unknown;

  // Name first, when the name actually says which potion it is. Hypixel
  // labels its potions plainly ("Speed II Potion"), and a name that says
  // Speed is better evidence than a damage value we may have failed to read.
  auto mentions = [](const std::string &text, const char *needle) {
    return text.find(needle) != std::string::npos;
  };
  for (const std::string &text : {display, type}) {
    if (mentions(text, "invisib"))
      return PotionKind::Invisibility;
    if (mentions(text, "jump") || mentions(text, "leaping"))
      return PotionKind::Jump;
    if (mentions(text, "speed") || mentions(text, "swift"))
      return PotionKind::Speed;
  }

  if (item.metadata < 0)
    return PotionKind::Unknown;
  // Only the low four bits carry the potion type. Bit 5 is the level-II flag
  // and bit 6 is the extended-duration flag, so masking with 0x7F folded those
  // into the comparison: Hypixel's Speed II reads 8226, whose low seven bits
  // are 34 rather than 2, and the classification fell through to Unknown.
  // Invisibility has no level II, which is exactly why that one kept working
  // while Speed and Jump Boost never alerted.
  const int potionType = item.metadata & 0x0F;
  if (potionType == 14) return PotionKind::Invisibility;
  if (potionType == 11) return PotionKind::Jump;
  if (potionType == 2) return PotionKind::Speed;
  return PotionKind::Unknown;
}

SwordTier classifySwordTier(const VisibleItem &item) {
  const std::string type = normalizeText(item.typeName);
  if (type == "sworddiamond" || type == "diamond sword" ||
      type == "item sworddiamond")
    return SwordTier::Diamond;
  if (type == "swordiron" || type == "iron sword" ||
      type == "item swordiron")
    return SwordTier::Iron;
  if (type == "swordstone" || type == "stone sword" ||
      type == "item swordstone")
    return SwordTier::Stone;
  if (type == "swordwood" || type == "wooden sword" ||
      type == "wood sword" || type == "item swordwood")
    return SwordTier::Wood;
  return SwordTier::None;
}

ImportantItem classifyImportantItem(const VisibleItem &item) {
  const SwordTier sword = classifySwordTier(item);
  if (sword == SwordTier::Stone)
    return ImportantItem::StoneSword;
  if (sword == SwordTier::Iron)
    return ImportantItem::IronSword;
  if (sword == SwordTier::Diamond)
    return ImportantItem::DiamondSword;
  if (isKnockbackStick(item))
    return ImportantItem::KnockbackStick;

  const PotionKind potion = classifyPotion(item);
  if (potion == PotionKind::Speed)
    return ImportantItem::SpeedPotion;
  if (potion == PotionKind::Jump)
    return ImportantItem::JumpPotion;
  if (potion == PotionKind::Invisibility)
    return ImportantItem::InvisibilityPotion;

  const std::string type = normalizeText(item.typeName);
  if (type == "bow" || type == "item bow")
    return item.enchanted ? ImportantItem::EnchantedBow : ImportantItem::Bow;
  // Every egg in a Bedwars game is a Bridge Egg and every spawn egg is a
  // Dream Defender; neither item exists in the mode for any other reason, so
  // the base item id identifies them without needing the display name.
  if (type == "egg" || type == "item egg") return ImportantItem::BridgeEgg;
  if (type == "monsterplacer" || type == "spawn egg" ||
      type == "item monsterplacer")
    return ImportantItem::DreamDefender;
  if (type == "bucketwater" || type == "water bucket" ||
      type == "item bucketwater")
    return ImportantItem::WaterBucket;
  // Only the two upper pickaxe tiers matter. Everyone can end up with a
  // wooden or stone pickaxe from the default shop, but gold (tier 3) and
  // diamond (tier 4) are deliberate purchases and tell you the holder is
  // equipped to mine through a defence.
  if (type == "pickaxegold" || type == "golden pickaxe" ||
      type == "gold pickaxe" || type == "item pickaxegold")
    return ImportantItem::GoldenPickaxe;
  if (type == "pickaxediamond" || type == "diamond pickaxe" ||
      type == "item pickaxediamond")
    return ImportantItem::DiamondPickaxe;
  // Obsidian is the strongest block in the mode, so someone carrying it is
  // about to make a bed a great deal harder to reach.
  if (type == "obsidian" || type == "tile obsidian")
    return ImportantItem::Obsidian;
  if (type == "tnt" || type == "tile tnt") return ImportantItem::Tnt;
  if (type == "fireball" || type == "fire charge" ||
      type == "item fireball")
    return ImportantItem::Fireball;
  if (type == "enderpearl" || type == "ender pearl" ||
      type == "item enderpearl")
    return ImportantItem::EnderPearl;
  if (type == "applegold" || type == "golden apple" ||
      type == "item applegold")
    return ImportantItem::GoldenApple;
  // Hypixel's Magic Milk is a plain milk bucket, so the base item is the
  // only thing that identifies it. The bucket has no other use in the mode.
  if (type == "milk" || type == "milk bucket" || type == "bucketmilk" ||
      type == "item milk")
    return ImportantItem::Milk;
  return ImportantItem::None;
}

const char *importantItemName(ImportantItem item) {
  switch (item) {
  case ImportantItem::StoneSword: return "Stone Sword";
  case ImportantItem::IronSword: return "Iron Sword";
  case ImportantItem::DiamondSword: return "Diamond Sword";
  case ImportantItem::Bow: return "Bow";
  case ImportantItem::EnchantedBow: return "Enchanted Bow";
  case ImportantItem::KnockbackStick: return "Knockback Stick";
  case ImportantItem::SpeedPotion: return "Speed Potion";
  case ImportantItem::JumpPotion: return "Jump Potion";
  case ImportantItem::InvisibilityPotion: return "Invisibility Potion";
  case ImportantItem::Tnt: return "TNT";
  case ImportantItem::Fireball: return "Fireball";
  case ImportantItem::EnderPearl: return "Ender Pearl";
  case ImportantItem::GoldenApple: return "Golden Apple";
  case ImportantItem::Milk: return "Magic Milk";
  case ImportantItem::BridgeEgg: return "Bridge Egg";
  case ImportantItem::WaterBucket: return "Water Bucket";
  case ImportantItem::DreamDefender: return "Dream Defender";
  case ImportantItem::GoldenPickaxe: return "Golden Pickaxe";
  case ImportantItem::DiamondPickaxe: return "Diamond Pickaxe";
  case ImportantItem::Obsidian: return "Obsidian";
  default: return "";
  }
}

const char *importantItemKey(ImportantItem item) {
  switch (item) {
  case ImportantItem::StoneSword: return "stoneSword";
  case ImportantItem::IronSword: return "ironSword";
  case ImportantItem::DiamondSword: return "diamondSword";
  case ImportantItem::Bow: return "bow";
  case ImportantItem::EnchantedBow: return "enchantedBow";
  case ImportantItem::KnockbackStick: return "knockbackStick";
  case ImportantItem::SpeedPotion: return "speedPotion";
  case ImportantItem::JumpPotion: return "jumpPotion";
  case ImportantItem::InvisibilityPotion: return "invisibilityPotion";
  case ImportantItem::Tnt: return "tnt";
  case ImportantItem::Fireball: return "fireball";
  case ImportantItem::EnderPearl: return "enderPearl";
  case ImportantItem::GoldenApple: return "goldenApple";
  case ImportantItem::Milk: return "magicMilk";
  case ImportantItem::BridgeEgg: return "bridgeEgg";
  case ImportantItem::WaterBucket: return "waterBucket";
  case ImportantItem::DreamDefender: return "dreamDefender";
  case ImportantItem::GoldenPickaxe: return "goldenPickaxe";
  case ImportantItem::DiamondPickaxe: return "diamondPickaxe";
  case ImportantItem::Obsidian: return "obsidian";
  default: return "";
  }
}

bool isExplicitlyIgnoredHeldItem(const VisibleItem &item) {
  return !item.typeName.empty() &&
         classifyImportantItem(item) == ImportantItem::None &&
         classifySwordTier(item) == SwordTier::None;
}

std::vector<PlayerAlert>
PlayerMonitor::observe(const std::vector<PlayerObservation> &players,
                       const PlayerAlertOptions &options, Tick now,
                       TeamTracker *teams) {
  std::vector<PlayerAlert> alerts;
  m_rejectionCounts.fill(0);
  m_diagnostics = {};
  m_rejectionDetails.clear();
  const double maximumDistance = std::clamp(options.maximumDistance, 1.0, 256.0);
  const Tick cooldown = std::clamp<Tick>(options.cooldownMs, 250, 60000);

  // `event` identifies WHAT happened, not just which category it belongs to.
  // The cooldown exists to stop the same event repeating while it stays true,
  // so it must only apply when the new event matches the one it last fired
  // for. Comparing on category alone meant a player who triggered two
  // different alerts of the same category within the cooldown -- their team
  // revealing Sharpness and Protection together, an item alert landing right
  // after a potion -- had the second one silently dropped as a repeat.
  auto emit = [&](TrackedPlayer &tracked, PlayerAlert::Kind kind,
                   const std::string &event, int entityId, TeamId team,
                   const std::string &text,
                   std::vector<MessageSegment> segments,
                   bool respectCooldown = true) {
    const auto index = static_cast<std::size_t>(kind);
    auto &memory = tracked.lastAlert[index];
    if (respectCooldown && memory.at != 0 && memory.event == event &&
        now >= memory.at && now - memory.at < cooldown) {
      ++m_diagnostics.cooldownRejected;
      return;
    }
    memory.event = event;
    memory.at = now;
    alerts.push_back({kind, entityId, text, std::move(segments), team});
    ++m_diagnostics.emitted;
  };

  auto reject = [&](const PlayerObservation &player,
                    PlayerRejectReason reason) {
    const auto index = static_cast<std::size_t>(reason);
    if (index < m_rejectionCounts.size())
      ++m_rejectionCounts[index];
    if (m_rejectionDetails.size() < 16)
      m_rejectionDetails.push_back(
          {player.entityId, player.identity.substr(0, 64), reason});
  };

  for (const auto &player : players) {
    if (player.entityId < 0 || player.identity.empty()) {
      reject(player, PlayerRejectReason::InvalidIdentity);
      continue;
    }
    if (player.localPlayer) {
      reject(player, PlayerRejectReason::LocalPlayer);
      continue;
    }
    if (!player.alive) {
      reject(player, PlayerRejectReason::Dead);
      continue;
    }
    if (player.spectator) {
      reject(player, PlayerRejectReason::Spectator);
      continue;
    }
    if (options.ignoreOwnTeam && player.teammateKnown && player.teammate) {
      reject(player, PlayerRejectReason::Teammate);
      continue;
    }
    if (!std::isfinite(player.distance) || player.distance > maximumDistance) {
      reject(player, PlayerRejectReason::OutOfRange);
      continue;
    }
    if (options.visibility != VisibilityMode::RangeOnly) {
      if (!player.lineOfSightKnown) {
        reject(player, PlayerRejectReason::UnknownVisibility);
        continue;
      }
      if (!player.hasLineOfSight) {
        reject(player, PlayerRejectReason::NoLineOfSight);
        continue;
      }
    }
    if (options.visibility == VisibilityMode::CameraView) {
      if (!player.cameraViewKnown) {
        reject(player, PlayerRejectReason::UnknownVisibility);
        continue;
      }
      if (!player.insideCameraView) {
        reject(player, PlayerRejectReason::OutsideCameraView);
        continue;
      }
    }

    if (teams)
      teams->observePlayer(player.identity, player.entityId, player.team,
                           player.teamAuthoritative, now);

    // Claim this entity id for the current occupant. Per-player alert state
    // lives in m_players keyed by identity, never by entity id, so a recycled
    // id simply re-points here and the previous occupant keeps their own
    // history for when they are seen again under a new entity id.
    m_entityOwners[player.entityId] = player.identity;

    auto [it, inserted] = m_players.try_emplace(player.identity);
    TrackedPlayer &tracked = it->second;
    if (inserted) {
      tracked = {};
      tracked.identity = player.identity;
      tracked.entityId = player.entityId;
      // Armor and sword tier are monotonic progressions: whatever tier a
      // player already has the first time they are seen is a baseline, not
      // new information, so seeding it here (without alerting) is correct
      // and only later upgrades should alert.
      //
      // Held items such as potions, the knockback stick, and other
      // important items are NOT progressions: seeing one for the first
      // time is itself the alert-worthy event. Previously this branch also
      // pre-marked the player's current held item as "seen" and skipped
      // straight to the next player, which meant an item already in an
      // enemy's hand the first time they were scanned could never alert
      // for that item again (the per-match dedup treated it as already
      // reported). Leaving importantItemAlertedAt untouched here lets the
      // normal detection logic below run on this same, first, tick.
      tracked.armor = player.armor;
      tracked.highestSwordTier = classifySwordTier(player.heldItem);
      tracked.lastSeen = now;
    }
    if (tracked.entityId != player.entityId) {
      m_entityOwners.erase(tracked.entityId);
      tracked.entityId = player.entityId;
      m_entityOwners[player.entityId] = player.identity;
    }

    if (options.armor && armorRank(player.armor) > armorRank(tracked.armor)) {
      const std::string label =
          std::string(armorTierName(player.armor)) + " Armor";
      const std::string tail = " now has " + label;
      emit(tracked, PlayerAlert::Kind::Armor, armorTierName(player.armor),
           player.entityId, player.team, playerText(player, tail),
           playerItemSegments(player, " now has ", label,
                              armorTierArgb(player.armor)));
    }
    if (options.upgrades && player.heldItem.enchanted &&
        isSword(player.heldItem)) {
      ++m_diagnostics.observedSwordGlints;
      if (teams && teams->observeSharpness(player.team, now)) {
        const std::string tail = std::string(" revealed ") +
                                 teamName(player.team) + " Team's Sharpness";
        emit(tracked, PlayerAlert::Kind::Upgrade, "sharpness", player.entityId,
             player.team, playerText(player, tail),
             {{player.identity, teamArgb(player.team)},
              {" revealed ", 0xFFE0E0E0U},
              {std::string(teamName(player.team)) + " Team",
               teamArgb(player.team)},
              {"'s Sharpness", 0xFFE0E0E0U}},
             false);
      } else if (teams) {
        ++m_diagnostics.sharpnessDuplicates;
      }
    }
    // Enemy Protection is never announced anywhere a client can read it --
    // the purchase message only goes to the buying team. The one thing it
    // does leave behind is a glint on every piece of that team's armour, and
    // armour is worn constantly rather than swapped like a sword, so this is
    // a steadier read than the Sharpness one it sits next to.
    if (options.upgrades && player.armorEnchanted &&
        player.armor != ArmorTier::None) {
      ++m_diagnostics.observedArmorGlints;
      if (teams && teams->observeProtection(player.team, now)) {
        const std::string tail = std::string(" revealed ") +
                                 teamName(player.team) + " Team's Protection";
        emit(tracked, PlayerAlert::Kind::Upgrade, "protection",
             player.entityId, player.team, playerText(player, tail),
             {{player.identity, teamArgb(player.team)},
              {" revealed ", 0xFFE0E0E0U},
              {std::string(teamName(player.team)) + " Team",
               teamArgb(player.team)},
              {"'s Protection", 0xFFE0E0E0U}},
             false);
      } else if (teams) {
        ++m_diagnostics.protectionDuplicates;
      }
    }
    const ImportantItem importantItem = classifyImportantItem(player.heldItem);
    const auto importantIndex = static_cast<std::size_t>(importantItem);
    const PotionKind potion = classifyPotion(player.heldItem);
    const std::string consumableName = readableItemName(player.heldItem);

    // "Have they just switched to this?" Only a switch counts as news, so an
    // enemy who simply keeps an item in hand never re-alerts, while the same
    // item picked up again later does. tracked.heldItem is still the previous
    // observation at this point -- it is only overwritten at the end of the
    // loop body -- which is exactly what makes this comparison meaningful.
    const bool switchedTo =
        classifyImportantItem(tracked.heldItem) != importantItem;

    // Should this item be reported? Yes if it has never been reported, or if
    // it was reported long enough ago AND they are holding it afresh rather
    // than having kept it in hand the whole time.
    auto itemIsNews = [&](std::size_t index) {
      if (index >= tracked.importantItemAlertedAt.size())
        return false;
      const Tick last = tracked.importantItemAlertedAt[index];
      if (last == 0)
        return true;
      if (!switchedTo)
        return false;
      // A clock that has gone backwards means the match state was rebuilt
      // under us; treat the old timestamp as stale rather than trusting a
      // negative interval.
      if (now < last)
        return true;
      return now - last >= kImportantItemRepeatMs;
    };

    auto markItemReported = [&](std::size_t index) {
      if (index < tracked.importantItemAlertedAt.size())
        tracked.importantItemAlertedAt[index] = now;
    };

    const bool consumeStarted =
        options.consumes && player.usingItem && !tracked.usingItem &&
        isConsumable(consumableName) && options.itemAllowed(importantItem);
    if (consumeStarted && !consumableName.empty()) {
      const std::string tail = " is using " + consumableName;
      emit(tracked, PlayerAlert::Kind::Consume, consumableName, player.entityId,
           player.team, playerText(player, tail),
           playerItemSegments(player, " is using ", consumableName,
                              importantItemArgb(importantItem)));
      // When both modules are enabled, the stronger use signal replaces the
      // same-tick held-item alert and prevents a delayed duplicate.
      markItemReported(importantIndex);
    }
    if (options.items && !consumeStarted && potion != PotionKind::Unknown &&
        options.itemAllowed(importantItem)) {
      ++m_diagnostics.classifiedPotions;
      if (itemIsNews(importantIndex)) {
        markItemReported(importantIndex);
        const std::string prefix = potion == PotionKind::Invisibility
                                       ? " is holding an "
                                       : " is holding a ";
        const std::string label = std::string(potionName(potion)) + " Potion";
        const std::string tail = prefix + label;
        emit(tracked, PlayerAlert::Kind::Potion, potionName(potion),
             player.entityId, player.team, playerText(player, tail),
             playerItemSegments(player, prefix, label,
                                importantItemArgb(importantItem)),
             false);
      } else {
        ++m_diagnostics.potionDuplicates;
      }
    }
    if (options.items && isKnockbackStick(player.heldItem) &&
        options.itemAllowed(ImportantItem::KnockbackStick)) {
      ++m_diagnostics.classifiedKnockback;
      if (itemIsNews(importantIndex)) {
        markItemReported(importantIndex);
        const std::string tail = " has a Knockback Stick";
        emit(tracked, PlayerAlert::Kind::KnockbackStick, "knockback stick",
             player.entityId, player.team, playerText(player, tail),
             playerItemSegments(
                 player, " has a ", "Knockback Stick",
                 importantItemArgb(ImportantItem::KnockbackStick)),
             false);
      } else {
        ++m_diagnostics.knockbackDuplicates;
      }
    }
    const SwordTier swordTier = classifySwordTier(player.heldItem);
    const bool swordUpgraded =
        static_cast<int>(swordTier) > static_cast<int>(tracked.highestSwordTier);
    if (swordUpgraded)
      tracked.highestSwordTier = swordTier;
    // Stone counts. Everyone starts on wood, so a stone sword is already a
    // real change in what that player can do to you, and leaving it out was
    // why buying a stone sword produced no alert at all.
    if (options.items && swordUpgraded && swordTier != SwordTier::None &&
        swordTier != SwordTier::Wood && options.itemAllowed(importantItem)) {
      const std::string label = importantItemName(importantItem);
      const std::string tail = " upgraded to " + label;
      emit(tracked, PlayerAlert::Kind::Item, importantItemName(importantItem),
           player.entityId, player.team, playerText(player, tail),
           playerItemSegments(player, " upgraded to ", label,
                              importantItemArgb(importantItem)),
           false);
      markItemReported(importantIndex);
    } else if (options.items && !consumeStarted &&
               options.itemAllowed(importantItem) &&
               importantItem != ImportantItem::None &&
               potion == PotionKind::Unknown &&
               !isKnockbackStick(player.heldItem) &&
               swordTier == SwordTier::None) {
      if (itemIsNews(importantIndex)) {
        markItemReported(importantIndex);
        const std::string label = importantItemName(importantItem);
        const std::string tail = " is holding " + label;
        emit(tracked, PlayerAlert::Kind::Item, importantItemName(importantItem),
             player.entityId, player.team, playerText(player, tail),
             playerItemSegments(player, " is holding ", label,
                                importantItemArgb(importantItem)),
             false);
      } else {
        ++m_diagnostics.itemDuplicates;
      }
    } else if (options.items && !player.heldItem.typeName.empty() &&
               importantItem == ImportantItem::None) {
      if (isExplicitlyIgnoredHeldItem(player.heldItem))
        ++m_diagnostics.ignoredItems;
      else
        ++m_diagnostics.unknownItems;
    }

    if (armorRank(player.armor) > armorRank(tracked.armor))
      tracked.armor = player.armor;
    tracked.heldItem = player.heldItem;
    tracked.usingItem = player.usingItem;
    tracked.lastSeen = now;
  }

  const std::size_t capacity = std::clamp<std::size_t>(options.capacity, 8, 512);
  while (m_players.size() > capacity) {
    auto oldest = std::min_element(
        m_players.begin(), m_players.end(),
        [](const auto &a, const auto &b) {
          return a.second.lastSeen < b.second.lastSeen;
        });
    if (oldest == m_players.end())
      break;
    m_players.erase(oldest);
  }
  for (auto it = m_entityOwners.begin(); it != m_entityOwners.end();) {
    if (m_players.find(it->second) == m_players.end())
      it = m_entityOwners.erase(it);
    else
      ++it;
  }
  return alerts;
}

void PlayerMonitor::reset() {
  m_players.clear();
  m_entityOwners.clear();
  m_rejectionCounts.fill(0);
  m_diagnostics = {};
  m_rejectionDetails.clear();
}

void PlayerMonitor::forgetPlayerLoadout(const std::string &identity) {
  if (identity.empty())
    return;
  const auto it = m_players.find(identity);
  if (it == m_players.end())
    return;
  TrackedPlayer &tracked = it->second;

  // A Bedwars death only takes what was in the player's inventory. Armor is a
  // permanent team upgrade: whoever respawns is wearing exactly the armor they
  // died in, so dropping the armor baseline here would re-alert gear the user
  // has already been told about, once per death, for the rest of the match.
  // Keep tracked.armor -- it is what the armor rule actually dedups against.
  //
  // Everything else really is gone. They respawn with the default wooden sword
  // and an empty inventory, so the sword tier, the held item and the
  // per-match "already reported this item" flags all have to be cleared, or a
  // re-bought diamond sword or a second invisibility potion would stay silent
  // for the rest of the game.
  tracked.heldItem = {};
  tracked.usingItem = false;
  tracked.highestSwordTier = SwordTier::None;
  tracked.importantItemAlertedAt.fill(0);

  // The alert cooldowns are per-kind. Clear the loadout-driven ones so a
  // fresh sighting is not swallowed by a cooldown started before the death,
  // but keep the armor slot in step with the armor baseline above.
  constexpr auto armorSlot = static_cast<std::size_t>(PlayerAlert::Kind::Armor);
  TrackedPlayer::AlertMemory armorMemory;
  if (armorSlot < tracked.lastAlert.size())
    armorMemory = tracked.lastAlert[armorSlot];
  tracked.lastAlert.fill({});
  if (armorSlot < tracked.lastAlert.size())
    tracked.lastAlert[armorSlot] = armorMemory;
}

void PlayerMonitor::forgetPlayer(const std::string &identity) {
  if (identity.empty())
    return;
  const auto it = m_players.find(identity);
  if (it == m_players.end())
    return;
  const int entityId = it->second.entityId;
  m_players.erase(it);
  const auto ownerIt = m_entityOwners.find(entityId);
  if (ownerIt != m_entityOwners.end() && ownerIt->second == identity)
    m_entityOwners.erase(ownerIt);
}

HeightResult evaluateHeight(double playerY,
                            std::optional<int> configuredLimit) {
  HeightResult result;
  result.currentY = std::isfinite(playerY) ? static_cast<int>(std::floor(playerY))
                                            : 0;
  if (configuredLimit && *configuredLimit >= 1 && *configuredLimit <= 512) {
    result.limit = *configuredLimit;
    result.remaining = std::max(0, *configuredLimit - result.currentY);
  }
  return result;
}

void NotificationTimeline::add(const std::string &key, Tick now,
                               Tick durationMs,
                               std::size_t maximumVisible) {
  expire(now);
  NotificationRecord record;
  record.key = key.substr(0, 128);
  record.created = now;
  record.durationMs = std::clamp<Tick>(durationMs, 1000, 15000);
  const std::size_t maximum = std::clamp<std::size_t>(maximumVisible, 1, 20);
  while (m_records.size() >= maximum)
    m_records.erase(m_records.begin());
  m_records.push_back(std::move(record));
}

void NotificationTimeline::expire(Tick now) {
  m_records.erase(
      std::remove_if(m_records.begin(), m_records.end(), [now](const auto &item) {
        return now < item.created || now - item.created >= item.durationMs;
      }),
      m_records.end());
}

const char *hudName(HudId hud) {
  static constexpr std::array<const char *, kHudCount> names = {
      "Event Timer", "Height", "Resources", "Team State"};
  const auto index = static_cast<std::size_t>(hud);
  return names[index < names.size() ? index : 0];
}

HudLayout sanitizeHudLayout(const HudLayout &layout, float normalizedWidth,
                            float normalizedHeight) {
  HudLayout result = layout;
  const float width = std::clamp(normalizedWidth, 0.02F, 1.0F);
  const float height = std::clamp(normalizedHeight, 0.02F, 1.0F);
  result.x = std::isfinite(result.x)
                 ? std::clamp(result.x, 0.0F, std::max(0.0F, 1.0F - width))
                 : 0.02F;
  result.y = std::isfinite(result.y)
                 ? std::clamp(result.y, 0.0F, std::max(0.0F, 1.0F - height))
                 : 0.02F;
  result.scale = std::isfinite(result.scale)
                     ? std::clamp(result.scale, 0.5F, 2.5F)
                     : 1.0F;
  return result;
}

ShopOffer evaluateShopOffer(const std::string &itemName,
                            const std::vector<std::string> &lore,
                            const ResourceSnapshot &resources) {
  ShopOffer offer;
  offer.itemName = stripFormatting(itemName);
  for (const auto &rawLine : lore) {
    const std::string line = normalizeText(rawLine);
    if (contains(line, "permanent"))
      offer.permanent = true;
    if (contains(line, "already owned") || contains(line, "unlocked") ||
        contains(line, "purchased"))
      offer.alreadyOwned = true;
    if (!contains(line, "cost"))
      continue;

    int cost = -1;
    std::istringstream words(line);
    std::string word;
    while (words >> word) {
      if (!word.empty() &&
          std::all_of(word.begin(), word.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
          })) {
        try {
          cost = std::stoi(word);
        } catch (...) {
          cost = -1;
        }
        break;
      }
    }
    if (cost < 0 || cost > 100000)
      continue;
    offer.cost = cost;
    if (contains(line, "iron"))
      offer.currency = ShopCurrency::Iron;
    else if (contains(line, "gold"))
      offer.currency = ShopCurrency::Gold;
    else if (contains(line, "diamond"))
      offer.currency = ShopCurrency::Diamond;
    else if (contains(line, "emerald"))
      offer.currency = ShopCurrency::Emerald;
    if (offer.currency != ShopCurrency::Unknown)
      offer.valid = true;
  }

  const int index = resourceIndex(offer.currency);
  if (offer.valid && resources.inventoryValid && index >= 0)
    offer.affordable = resources.inventory[static_cast<std::size_t>(index)] >=
                       offer.cost;
  return offer;
}

bool shouldBlockDuplicatePurchase(const ShopOffer &offer,
                                  bool preventionEnabled) {
  return preventionEnabled && offer.valid && offer.permanent &&
         offer.alreadyOwned;
}

std::string stripFormatting(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == 0xC2 && i + 2 < text.size() &&
        static_cast<unsigned char>(text[i + 1]) == 0xA7) {
      i += 2;
      continue;
    }
    if (c == 0xA7 && i + 1 < text.size()) {
      ++i;
      continue;
    }
    result.push_back(static_cast<char>(c));
  }
  return result;
}

std::string normalizeText(const std::string &text) {
  const std::string stripped = stripFormatting(text);
  std::string result;
  result.reserve(stripped.size());
  bool previousSpace = true;
  for (const unsigned char c : stripped) {
    if (std::isspace(c) != 0) {
      if (!previousSpace)
        result.push_back(' ');
      previousSpace = true;
    } else {
      result.push_back(static_cast<char>(std::tolower(c)));
      previousSpace = false;
    }
  }
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}

} // namespace OVson::Bedwars
