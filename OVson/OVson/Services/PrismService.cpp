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

int calculateNetworkLevel(long long exp) {
  if (exp <= 0)
    return 1;
  return (int)std::floor((std::sqrt(2.0 * (double)exp + 30625.0) / 50.0) - 2.5);
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

  Hypixel::PlayerStats ps;
  ps.uuid = uuid;
  ps.isFetched = true;

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

  findJsonString(body, "displayname", ps.displayName);
  findJsonString(body, "prefix", ps.prefix);
  findJsonString(body, "rank", ps.rank);
  findJsonString(body, "monthlyPackageRank", ps.monthlyPackageRank);
  findJsonString(body, "newPackageRank", ps.newPackageRank);
  findJsonString(body, "rankPlusColor", ps.rankPlusColor);

  int networkExp = 0;
  if (findJsonInt(body, "networkExp", networkExp)) {
    ps.networkLevel = calculateNetworkLevel(networkExp);
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
    }
  }

  prismDbg("PARSE OK uuid=%s star=%d fk=%d wins=%d", uuid.c_str(),
           ps.bedwarsStar, ps.bedwarsFinalKills, ps.bedwarsWins);
  return ps;
}
