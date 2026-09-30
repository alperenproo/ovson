#include "Hypixel.h"
#include "../Config/Config.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include <algorithm>
#include <string>
#include <vector>


static bool findJsonString(const std::string &json, const char *key,
                           std::string &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t q1 = json.find('"', json.find(':', k));
  if (q1 == std::string::npos)
    return false;
  size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos)
    return false;
  out = json.substr(q1 + 1, q2 - (q1 + 1));
  return true;
}

static bool findJsonInt(const std::string &json, const char *key, int &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t c = json.find(':', k);
  if (c == std::string::npos)
    return false;
  size_t end = c + 1;
  while (end < json.size() && (json[end] == ' ' || json[end] == '\t'))
    ++end;
  size_t e = end;
  while (e < json.size() && isdigit((unsigned char)json[e]))
    ++e;
  if (e == end)
    return false;
  out = atoi(json.substr(end, e - end).c_str());
  return true;
}

static bool findJsonDouble(const std::string &json, const char *key, double &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos)
    return false;
  size_t c = json.find(':', k);
  if (c == std::string::npos)
    return false;
  size_t end = c + 1;
  while (end < json.size() && (json[end] == ' ' || json[end] == '\t' || json[end] == '"'))
    ++end;
  if (end >= json.size())
    return false;
  char *endPtr = nullptr;
  double val = strtod(json.c_str() + end, &endPtr);
  if (endPtr == json.c_str() + end)
    return false;
  out = val;
  return true;
}

std::optional<std::string> Hypixel::getUuidByName(const std::string &name) {
  return getUuidByName(name, nullptr);
}

std::optional<std::string> Hypixel::getUuidByName(const std::string &name, std::string *outExactName) {
  std::string body;
  std::string url = "https://api.mojang.com/users/profiles/minecraft/" + name;

  if (!Http::get(url, body))
    return std::nullopt;

  std::string id;
  if (!findJsonString(body, "id", id))
    return std::nullopt;

  if (outExactName) {
    findJsonString(body, "name", *outExactName);
  }

  return id;
}

std::optional<Hypixel::PlayerStats>
Hypixel::getPlayerStats(const std::string &apiKey, const std::string &uuid) {
  std::string body;
  std::string url = "https://api.hypixel.net/player?uuid=" + uuid;

  if (!Http::get(url, body, "API-Key", apiKey))
    return std::nullopt;

  if (body.find("\"success\":true") == std::string::npos &&
      body.find("\"success\": true") == std::string::npos) {
    return std::nullopt;
  }

  if (body.find("\"player\":null") != std::string::npos ||
      body.find("\"player\": null") != std::string::npos) {
    return std::nullopt;
  }

  size_t pPlayer = body.find("\"player\"");
  if (pPlayer == std::string::npos) {
    return std::nullopt;
  }

  PlayerStats ps;
  ps.uuid = uuid;

  if (!findJsonString(body, "displayname", ps.displayName) || ps.displayName.empty()) {
    return std::nullopt;
  }

  ps.isFetched = true;

  double level = 0.0;
  double netExp = 0.0;
  if (findJsonDouble(body, "networkLevel", level) && level > 0.0) {
    ps.networkLevel = (int)std::floor(level);
  } else if (findJsonDouble(body, "networkExp", netExp)) {
    ps.networkLevel = calculateNetworkLevel(netExp);
  } else if (findJsonDouble(body, "network_exp", netExp)) {
    ps.networkLevel = calculateNetworkLevel(netExp);
  }

  size_t pAch = body.find("\"achievements\"");
  if (pAch != std::string::npos) {
    int bwStar = 0;
    if (findJsonInt(body.substr(pAch), "bedwars_level", bwStar))
      ps.bedwarsStar = bwStar;
  }

  findJsonString(body, "prefix", ps.prefix);
  findJsonString(body, "rank", ps.rank);
  findJsonString(body, "monthlyPackageRank", ps.monthlyPackageRank);
  findJsonString(body, "newPackageRank", ps.newPackageRank);
  findJsonString(body, "packageRank", ps.packageRank);
  findJsonString(body, "rankPlusColor", ps.rankPlusColor);

  size_t pStats = body.find("\"stats\"");
  if (pStats != std::string::npos) {
    size_t pBw = body.find("\"Bedwars\"", pStats);
    if (pBw != std::string::npos) {
      int fk = 0, fd = 0, wins = 0, losses = 0;
      int kills = 0, deaths = 0, bedsBroken = 0, bedsLost = 0;
      std::string bwJson = body.substr(pBw);

      findJsonInt(bwJson, "final_kills_bedwars", fk);
      findJsonInt(bwJson, "final_deaths_bedwars", fd);
      findJsonInt(bwJson, "kills_bedwars", kills);
      findJsonInt(bwJson, "deaths_bedwars", deaths);
      findJsonInt(bwJson, "beds_broken_bedwars", bedsBroken);
      findJsonInt(bwJson, "beds_lost_bedwars", bedsLost);
      findJsonInt(bwJson, "wins_bedwars", wins);
      findJsonInt(bwJson, "losses_bedwars", losses);
      findJsonInt(bwJson, "winstreak", ps.winstreak);

      ps.bedwarsFinalKills = fk;
      ps.bedwarsFinalDeaths = fd;
      ps.bedwarsKills = kills;
      ps.bedwarsDeaths = deaths;
      ps.bedwarsBedsBroken = bedsBroken;
      ps.bedwarsBedsLost = bedsLost;
      ps.bedwarsWins = wins;
      ps.bedwarsLosses = losses;

      findJsonString(bwJson, "active_star", ps.activeStar);
      findJsonString(bwJson, "active_prestige_scheme", ps.activePrestigeScheme);
      findJsonString(bwJson, "active_prestige_bracket", ps.activePrestigeBracket);

      findJsonInt(bwJson, "iron_resources_collected_bedwars", ps.ironCollected);
      findJsonInt(bwJson, "gold_resources_collected_bedwars", ps.goldCollected);
      findJsonInt(bwJson, "diamond_resources_collected_bedwars", ps.diamondCollected);
      findJsonInt(bwJson, "emerald_resources_collected_bedwars", ps.emeraldCollected);
      findJsonInt(bwJson, "resources_collected_bedwars", ps.resourcesCollected);
      if (ps.resourcesCollected == 0) {
        ps.resourcesCollected = ps.ironCollected + ps.goldCollected + ps.diamondCollected + ps.emeraldCollected;
      }
      size_t slumPos = bwJson.find("\"slumber\"");
      if (slumPos != std::string::npos) {
        std::string slumSub = bwJson.substr(slumPos, 4000);
        if (!findJsonInt(slumSub, "tickets", ps.slumberTickets)) {
          findJsonInt(slumSub, "total_tickets_earned", ps.slumberTickets);
        }
      }
      if (ps.slumberTickets == 0) {
        findJsonInt(bwJson, "slumber_tickets", ps.slumberTickets);
      }
      if (ps.slumberTickets == 0) {
        findJsonInt(body, "bedwars_slumber_ticket_master", ps.slumberTickets);
      }

      if (!findJsonString(bwJson, "activeKillEffect", ps.activeKillEffect))
        findJsonString(bwJson, "active_kill_effect", ps.activeKillEffect);

      if (!findJsonString(bwJson, "activeDeathCry", ps.activeDeathCry))
        findJsonString(bwJson, "active_death_cry", ps.activeDeathCry);

      if (!findJsonString(bwJson, "activeVictoryDance", ps.activeVictoryDance))
        findJsonString(bwJson, "active_victory_dance", ps.activeVictoryDance);

      if (!findJsonString(bwJson, "activeProjectileTrail", ps.activeProjectileTrail))
        findJsonString(bwJson, "active_projectile_trail", ps.activeProjectileTrail);

      if (!findJsonString(bwJson, "activeIslandTopper", ps.activeIslandTopper))
        findJsonString(bwJson, "active_island_topper", ps.activeIslandTopper);

      if (!findJsonString(bwJson, "activeGlyph", ps.activeGlyph))
        findJsonString(bwJson, "active_glyph", ps.activeGlyph);

      if (!findJsonString(bwJson, "activeBedDestroy", ps.activeBedDestroy))
        findJsonString(bwJson, "active_bed_destroy", ps.activeBedDestroy);

      if (!findJsonString(bwJson, "active_star", ps.activeStar))
        findJsonString(bwJson, "activeStar", ps.activeStar);

      findJsonString(bwJson, "favourites_2", ps.quickBuy);
      findJsonString(bwJson, "favorite_slots", ps.favoriteSlots);
    }
  }

  return ps;
}
