#include "AbyssService.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
bool findJsonString(const std::string &json, const char *key,
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

bool findJsonInt(const std::string &json, const char *key, int &out) {
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
  while (e < json.size() && (isdigit((unsigned char)json[e]) || json[e] == '-'))
    ++e;
  if (e == end)
    return false;
  out = atoi(json.substr(end, e - end).c_str());
  return true;
}

int calculateNetworkLevel(long long exp) {
  if (exp <= 0)
    return 1;
  return (int)std::floor((std::sqrt(2.0 * (double)exp + 30625.0) / 50.0) - 2.5);
}
} // namespace

std::optional<Hypixel::PlayerStats>
AbyssService::getPlayerStats(const std::string &uuid) {
  std::string body;
  std::string url = "http://api.abyssoverlay.com/player?uuid=" + uuid;
  std::string userAgent = "node-ao/2.0.3";

  if (!Http::get(url, body, "", "", userAgent)) {
    Logger::error("AbyssService: Failed to fetch stats for %s", uuid.c_str());
    return std::nullopt;
  }

  Hypixel::PlayerStats ps;
  ps.uuid = uuid;

  size_t pPlayer = body.find("\"player\"");
  if (pPlayer == std::string::npos)
    return std::nullopt;
  std::string playerJson = body.substr(pPlayer);

  findJsonString(playerJson, "displayname", ps.displayName);
  findJsonString(playerJson, "prefix", ps.prefix);
  findJsonString(playerJson, "rank", ps.rank);
  findJsonString(playerJson, "monthlyPackageRank", ps.monthlyPackageRank);
  findJsonString(playerJson, "newPackageRank", ps.newPackageRank);
  findJsonString(playerJson, "rankPlusColor", ps.rankPlusColor);

  int networkExp = 0;
  if (findJsonInt(playerJson, "networkExp", networkExp)) {
    ps.networkLevel = calculateNetworkLevel(networkExp);
  }

  size_t pAch = playerJson.find("\"achievements\"");
  if (pAch != std::string::npos) {
    int bwStar = 0;
    if (findJsonInt(playerJson.substr(pAch), "bedwars_level", bwStar)) {
      ps.bedwarsStar = bwStar;
    }
  }

  size_t pStats = playerJson.find("\"stats\"");
  if (pStats != std::string::npos) {
    size_t pBw = playerJson.find("\"Bedwars\"", pStats);
    if (pBw != std::string::npos) {
      std::string bwJson = playerJson.substr(pBw);
      int fk = 0, fd = 0, wins = 0, losses = 0, ws = 0;

      findJsonInt(bwJson, "final_kills_bedwars", fk);
      findJsonInt(bwJson, "final_deaths_bedwars", fd);
      findJsonInt(bwJson, "wins_bedwars", wins);
      findJsonInt(bwJson, "losses_bedwars", losses);
      findJsonInt(bwJson, "winstreak", ws);

      ps.bedwarsFinalKills = fk;
      ps.bedwarsFinalDeaths = fd;
      ps.bedwarsWins = wins;
      ps.bedwarsLosses = losses;
      ps.winstreak = ws;
    }
  }

  return ps;
}
