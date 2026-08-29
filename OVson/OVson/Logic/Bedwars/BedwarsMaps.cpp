#include "BedwarsCore.h"

#include <algorithm>
#include <cctype>

namespace OVson::Bedwars {
namespace {

std::string compact(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (const unsigned char c : value)
    if (std::isalnum(c) != 0)
      result.push_back(static_cast<char>(std::tolower(c)));
  return result;
}

} // namespace

const std::vector<MapHeightEntry> &builtInMapHeights() {
  // Values are player-feet Y ceilings. The highest placeable block is one
  // lower; keeping both values prevents the old ambiguous "height" behavior.
  static const std::vector<MapHeightEntry> entries = {
      {"Aquarium",111,110},{"Artemis",97,96},{"Carapace",95,94},
      {"Catalyst",102,101},{"Deposit",82,81},{"Dreamgrove",116,115},
      {"Burrow",101,100},{"Springtide",87,86},{"Eastwood",101,100},
      {"Alaric",98,97},{"Hell Temple",115,114},{"Mortuus",91,90},
      {"Harvesting",96,95},{"Invasion",116,115},{"Jurassic",95,94},
      {"Kubo",92,91},{"Lectus",91,90},{"Lost Temple",92,91},
      {"Orientwood",101,100},{"Sky Festival",95,94},{"Mystery",93,92},
      {"Nostalgia",97,96},{"Obelisk",115,114},{"Orchid",87,86},
      {"Paladin",99,98},{"Paradox",85,84},{"Raze",89,88},
      {"Beeeee",102,101},{"Boardwalk",93,92},{"Coastal",90,89},
      {"Terminal",88,87},{"Unturned",93,92},{"Whiskers",88,87},
      {"Comet",116,115},{"Loft",83,82},{"Archway",87,86},
      {"Ashore",95,94},{"Boletum",86,85},{"Build Site",97,96},
      {"Chained",91,90},{"Daolong",91,90},{"Bunnywars",100,99},
      {"Egg Run",99,98},{"Enchanted",101,100},{"Extinction",96,95},
      {"Fang Outpost",100,99},{"Fort Doon",91,90},{"Ruins",92,91},
      {"Picnic",121,120},{"Frost",94,93},{"Dragon Light",96,95},
      {"Silver Birch",115,114},{"Highland Peaks",89,88},{"Snails",91,90},
      {"Seraph",95,94},{"Antenna",89,88},{"Rigged",95,94},
      {"Frogiton",91,90},{"Graveship",124,123},{"Pumpkin Bay",86,85},
      {"Unchained",91,90},{"Holmgang",98,97},{"Horizon",101,100},
      {"Infinite",89,88},{"Katsu",97,96},{"Tigris",102,101},
      {"Usagi",97,96},{"Pharaoh",96,95},{"Planet 98",106,105},
      {"Pool Party",87,86},{"Relic",91,90},{"Rise",97,96},
      {"Stilted",81,80},{"Stonekeep",96,95},{"Sandcastle",103,102},
      {"Shark Attack",95,94},{"Swashbuckle",86,85},{"Treenan",96,95},
      {"Grotto",101,100},{"Snowkeep",96,95},{"Zen Plaza",84,83},
      {"Airshow",101,100},{"Ambush",102,101},{"Apollo",99,98},
      {"Aqil",91,90},{"Arcade",91,90},{"Casita",94,93},
      {"Crypt",96,95},{"Deadwood",84,83},{"Dragonstar",101,100},
      {"Easter Basket",94,93},{"Easter Garden",99,98},{"Egg Hunt",101,100},
      {"Meadow",81,80},{"Sunflower",96,95},{"Gateway",129,128},
      {"Glacier",106,105},{"Darkened",82,81},{"Scareshow",101,100},
      {"Screamway",91,90},{"Harvest",84,83},{"Keep",62,61},
      {"Lighthouse",111,110},{"Lucky Rush",85,84},{"Blossom",97,96},
      {"Lunarhouse",111,110},{"Tuzi",92,91},{"Yue",102,101},
      {"Meso",96,95},{"Nebuc",106,105},{"Orchestra",107,106},
      {"Pernicious",91,90},{"Polygon",94,93},{"Rooftop",92,91},
      {"Sanctum",92,91},{"Serenity",94,93},{"Sky Rise",91,90},
      {"Slumber",94,93},{"Solace",101,100},{"Speedway",91,90},
      {"Gelato",81,80},{"Montipora",99,98},{"Symphonic",107,106},
      {"Urban Plaza",84,83},{"Vigilante",88,87},{"Waterfall",101,100},
      {"Blitzen",111,110},{"Frosted",91,90},{"Nutcracker",99,98},
      {"Zarzul",115,114},{"Acropolis",101,100},{"Aetius",95,94},
      {"Amazon",94,93},{"Arid",84,83},{"Ashfire",121,120},
      {"Hanging Gardens",108,107},{"Bio-Hazard",96,95},{"Cascade",88,87},
      {"Cliffside",101,100},{"Crogorm",124,123},{"Dockyard",98,97},
      {"Ghoulish",87,86},{"Ominosity",124,123},{"Steampumpkin",100,99},
      {"Trick or Yeet",95,94},{"Hollow",89,88},{"Impere",105,104},
      {"Ironclad",88,87},{"Lightstone",96,95},{"Lotus",90,89},
      {"Toro",93,92},{"Mirage",87,86},{"Orbit",97,96},
      {"Pavilion",98,97},{"Playground",101,100},{"Rooted",96,95},
      {"Scorched Sands",93,92},{"Siege",109,108},{"Steampunk",100,99},
      {"Fruitbrawl",101,100},{"Duye",95,94},{"Terraced",90,89},
      {"Fireplace",96,95},{"Lotice",91,90},{"Tengshe",101,100},
      {"Sanctuary",92,91},{"Ivory Castle",111,110},{"Snowy Square",96,95},
      {"Sweet Wonderland",95,94},{"Lions Temple",104,103},{"Gardens",102,101},
      {"Cryptic",105,104},{"Yandi",81,80},{"Chalk Cliffs",108,107},
      {"Antheon",98,97},{"Station",114,113},{"Atlas Prime",96,95},
      {"Echo Ruins",99,98},{"Lasagne",91,90},{"Manor Royale",94,93},
      {"Shipment",102,101},{"Shipwreck",59,58},{"Archipelago",91,90},
      {"Feldspar",90,89},{"Atmos",93,92},{"Mediterranean",85,84},
      {"Off World",86,85},{"Babylon",108,107},{"Temple",106,105},
      {"Facility",101,100},{"Crimson Height",106,105},{"Bucket Bay",92,91}};
  return entries;
}

std::string normalizeMapName(const std::string &name) {
  const std::string stripped = stripFormatting(name);
  std::string result;
  result.reserve(stripped.size());
  bool space = true;
  for (const unsigned char c : stripped) {
    if (std::isalnum(c) != 0) {
      result.push_back(static_cast<char>(std::tolower(c)));
      space = false;
    } else if (c == '\'' || c == '`') {
      continue;
    } else if (!space) {
      result.push_back(' ');
      space = true;
    }
  }
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}

std::optional<std::string> parseMapScoreboardLine(const std::string &line) {
  const std::string clean = stripFormatting(line);
  const std::size_t colon = clean.find(':');
  if (colon == std::string::npos || normalizeMapName(clean.substr(0, colon)) != "map")
    return std::nullopt;
  std::string name = normalizeMapName(clean.substr(colon + 1));
  if (name.empty() || name.size() > 64)
    return std::nullopt;
  return name;
}

MapHeightResolution resolveMapHeight(const std::string &mapName) {
  MapHeightResolution result;
  const std::string normalized = normalizeMapName(mapName);
  const std::string compactName = compact(normalized);
  if (normalized.empty())
    return result;
  for (const auto &entry : builtInMapHeights()) {
    if (compact(entry.canonicalName) == compactName) {
      result.canonicalName = entry.canonicalName;
      result.maximumPlayerY = entry.maximumPlayerY;
      result.maximumPlacementY = entry.maximumPlacementY;
      return result;
    }
  }
  result.canonicalName = normalized;
  return result;
}

} // namespace OVson::Bedwars
