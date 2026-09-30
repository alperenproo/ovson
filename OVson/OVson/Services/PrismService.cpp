#include "PrismService.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

static std::mutex g_prismDbgMutex;
static std::ofstream g_prismDbg;

static std::string resolvePrismLogPath() {
  char buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
    return std::string(buf) + "\\prism_debug.log";
  n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
    return std::string(buf) + "\\prism_debug.log";
  return "prism_debug.log";
}

static void prismDbg(const char *fmt, ...) {
  std::lock_guard<std::mutex> lk(g_prismDbgMutex);
  if (!g_prismDbg.is_open()) {
    g_prismDbg.open(resolvePrismLogPath(), std::ios::app);
    if (g_prismDbg.is_open()) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      g_prismDbg << "\n[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
                 << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
                 << "] ===== PrismService session start (pid="
                 << GetCurrentProcessId() << ") =====\n";
    }
  }
  if (!g_prismDbg.is_open())
    return;
  SYSTEMTIME st;
  GetLocalTime(&st);
  char prefix[48];
  sprintf_s(prefix, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute,
            st.wSecond, st.wMilliseconds);
  char body[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);
  g_prismDbg << prefix << body << "\n";
  g_prismDbg.flush();
}

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

int calculateNetworkLevel(double exp) {
  return Hypixel::calculateNetworkLevel(exp);
}

int getLevelForExp(int exp) {
    int prestiges = exp / 487000;
    int level = prestiges * 100;
    int expWithoutPrestiges = exp - (prestiges * 487000);

    if (expWithoutPrestiges < 500) return level;
    level++;
    if (expWithoutPrestiges < 1500) return level;
    level++;
    if (expWithoutPrestiges < 3500) return level;
    level++;
    if (expWithoutPrestiges < 7000) return level;
    level++;
    expWithoutPrestiges -= 7000;
    return level + (expWithoutPrestiges / 5000);
}
} // namespace

static thread_local PrismService::LastError t_lastError =
    PrismService::LastError::None;

PrismService::LastError PrismService::lastError() { return t_lastError; }

static bool looksLikeRealMojangUuid(const std::string &uuid) {
  char ver = 0;
  if (uuid.size() == 36 && uuid[8] == '-' && uuid[13] == '-' &&
      uuid[18] == '-' && uuid[23] == '-') {
    ver = uuid[14];
  } else if (uuid.size() == 32) {
    ver = uuid[12];
  } else {
    return false; // unknown shape
  }
  return ver == '4';
}

std::optional<Hypixel::PlayerStats>
PrismService::getPlayerStats(const std::string &uuid) {
  t_lastError = LastError::None;

  if (!looksLikeRealMojangUuid(uuid)) {
    t_lastError = LastError::NoPlayerData;
    prismDbg("SKIP uuid=%s (not a v4 Mojang UUID — nicked/offline)",
             uuid.c_str());
    return std::nullopt;
  }

  std::string body;
  std::string url = "https://flashlight.prismoverlay.com/v1/playerdata?uuid=" + uuid;

  prismDbg("REQ uuid=%s", uuid.c_str());

  {
    static std::mutex s_rateLimitMutex;
    static ULONGLONG s_lastRequestTime = 0;
    std::lock_guard<std::mutex> lock(s_rateLimitMutex);
    ULONGLONG now = GetTickCount64();
    if (now - s_lastRequestTime < 500) {
      Sleep((DWORD)(500 - (now - s_lastRequestTime)));
    }
    s_lastRequestTime = GetTickCount64();
  }

  if (!Http::get(url, body, "X-User-Id", "89547c5944a34976a376c5b632a4164a", "")) {
    Logger::error("PrismService: Failed to fetch stats for %s", uuid.c_str());
    t_lastError = LastError::HttpFailure;
    if (body.find("Rate limit exceeded") != std::string::npos) {
      t_lastError = LastError::RateLimited;
    } else if (body.find("failed to cache.GetOrCreate") != std::string::npos || body.find("player not stored") != std::string::npos) {
      t_lastError = LastError::InternalServerError;
    }
    prismDbg("HTTP FAIL uuid=%s err=%d", uuid.c_str(), (int)t_lastError);
    return std::nullopt;
  }

  std::string preview = body.substr(0, 120);
  for (char &c : preview) if (c == '\n' || c == '\r') c = ' ';
  prismDbg("HTTP OK uuid=%s body_len=%zu preview='%s%s'", uuid.c_str(),
           body.size(), preview.c_str(),
           body.size() > 120 ? "..." : "");

  if (body.find("\"player\":null") != std::string::npos ||
      body.find("\"player\": null") != std::string::npos) {
    t_lastError = LastError::NoPlayerData;
    prismDbg("NoPlayerData uuid=%s (player is null)", uuid.c_str());
    return std::nullopt;
  }

  size_t pPlayer = body.find("\"player\"");
  if (pPlayer == std::string::npos) {
    t_lastError = LastError::NoPlayerData;
    const char *hint = "no_player_block";
    if (body.find("\"throttle\"") != std::string::npos ||
        body.find("\"rate") != std::string::npos)
      hint = "rate_limited";
    else if (body.find("\"error\"") != std::string::npos ||
             body.find("\"cause\"") != std::string::npos)
      hint = "error_field_set";
    else if (body.size() < 32)
      hint = "tiny_body";
    prismDbg("NoPlayerData uuid=%s hint=%s", uuid.c_str(), hint);
    return std::nullopt;
  }

  Hypixel::PlayerStats ps;
  ps.uuid = uuid;

  if (!findJsonString(body, "displayname", ps.displayName) || ps.displayName.empty()) {
    t_lastError = LastError::NoPlayerData;
    prismDbg("NoPlayerData uuid=%s (no displayname)", uuid.c_str());
    return std::nullopt;
  }

  ps.isFetched = true;
  findJsonString(body, "prefix", ps.prefix);
  findJsonString(body, "rank", ps.rank);
  findJsonString(body, "monthlyPackageRank", ps.monthlyPackageRank);
  findJsonString(body, "newPackageRank", ps.newPackageRank);
  findJsonString(body, "packageRank", ps.packageRank);
  findJsonString(body, "rankPlusColor", ps.rankPlusColor);

  double level = 0.0;
  double netExp = 0.0;
  if (findJsonDouble(body, "networkLevel", level) && level > 0.0) {
    ps.networkLevel = (int)std::floor(level);
  } else if (findJsonDouble(body, "networkExp", netExp)) {
    ps.networkLevel = calculateNetworkLevel(netExp);
  } else if (findJsonDouble(body, "network_exp", netExp)) {
    ps.networkLevel = calculateNetworkLevel(netExp);
  }

  size_t pStats = body.find("\"stats\"");
  if (pStats != std::string::npos) {
    size_t pBw = body.find("\"Bedwars\"", pStats);
    if (pBw != std::string::npos) {
      int fk = 0, fd = 0, wins = 0, losses = 0, ws = 0;
      int kills = 0, deaths = 0, bb = 0, bl = 0;

      findJsonInt(body, "final_kills_bedwars", fk);
      findJsonInt(body, "final_deaths_bedwars", fd);
      findJsonInt(body, "wins_bedwars", wins);
      findJsonInt(body, "losses_bedwars", losses);
      findJsonInt(body, "winstreak", ws);
      findJsonInt(body, "kills_bedwars", kills);
      findJsonInt(body, "deaths_bedwars", deaths);
      findJsonInt(body, "beds_broken_bedwars", bb);
      findJsonInt(body, "beds_lost_bedwars", bl);

      ps.bedwarsFinalKills = fk;
      ps.bedwarsFinalDeaths = fd;
      ps.bedwarsWins = wins;
      ps.bedwarsLosses = losses;
      ps.winstreak = ws;
      ps.bedwarsKills = kills;
      ps.bedwarsDeaths = deaths;
      ps.bedwarsBedsBroken = bb;
      ps.bedwarsBedsLost = bl;

      int exp = 0;
      size_t pExp = body.find("\"Experience\"", pBw);
      if (pExp != std::string::npos) {
          size_t colon = body.find(':', pExp);
          if (colon != std::string::npos) {
              size_t end = colon + 1;
              while (end < body.size() && (body[end] == ' ' || body[end] == '\t')) ++end;
              size_t e = end;
              while (e < body.size() && (isdigit((unsigned char)body[e]) || body[e] == '-')) ++e;
              if (e > end) {
                  exp = atoi(body.substr(end, e - end).c_str());
                  ps.bedwarsStar = getLevelForExp(exp);
              }
          }
      }

      std::string bwJson = body.substr(pBw);
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

  prismDbg("PARSE OK uuid=%s star=%d active_star=%s scheme=%s fk=%d wins=%d", uuid.c_str(),
           ps.bedwarsStar, ps.activeStar.c_str(), ps.activePrestigeScheme.c_str(),
           ps.bedwarsFinalKills, ps.bedwarsWins);
  return ps;
}
