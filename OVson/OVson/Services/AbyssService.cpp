#include "AbyssService.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

static std::mutex g_abyssDbgMutex;
static std::ofstream g_abyssDbg;

static std::string resolveAbyssLogPath() {
  char buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
    return std::string(buf) + "\\abyss_debug.log";
  n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
    return std::string(buf) + "\\abyss_debug.log";
  return "abyss_debug.log";
}

static void abyssDbg(const char *fmt, ...) {
  std::lock_guard<std::mutex> lk(g_abyssDbgMutex);
  if (!g_abyssDbg.is_open()) {
    g_abyssDbg.open(resolveAbyssLogPath(), std::ios::app);
    if (g_abyssDbg.is_open()) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      g_abyssDbg << "\n[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
                 << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
                 << "] ===== AbyssService session start (pid="
                 << GetCurrentProcessId() << ") =====\n";
    }
  }
  if (!g_abyssDbg.is_open())
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
  g_abyssDbg << prefix << body << "\n";
  g_abyssDbg.flush();
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
} // namespace

static thread_local AbyssService::LastError t_lastError =
    AbyssService::LastError::None;

AbyssService::LastError AbyssService::lastError() { return t_lastError; }

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
AbyssService::getPlayerStats(const std::string &uuid) {
  t_lastError = LastError::None;

  if (!looksLikeRealMojangUuid(uuid)) {
    t_lastError = LastError::NoPlayerData;
    abyssDbg("SKIP uuid=%s (not a v4 Mojang UUID — nicked/offline)",
             uuid.c_str());
    return std::nullopt;
  }

  std::string body;
  std::string url = "http://api.abyssoverlay.com/player?uuid=" + uuid;
  std::string userAgent = "node-ao/2.0.3";

  abyssDbg("REQ uuid=%s", uuid.c_str());

  if (!Http::get(url, body, "", "", userAgent)) {
    Logger::error("AbyssService: Failed to fetch stats for %s", uuid.c_str());
    t_lastError = LastError::HttpFailure;
    abyssDbg("HTTP FAIL uuid=%s (Http::get returned false)", uuid.c_str());
    return std::nullopt;
  }

  std::string preview = body.substr(0, 120);
  for (char &c : preview) if (c == '\n' || c == '\r') c = ' ';
  abyssDbg("HTTP OK uuid=%s body_len=%zu preview='%s%s'", uuid.c_str(),
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
    abyssDbg("NoPlayerData uuid=%s hint=%s", uuid.c_str(), hint);
    return std::nullopt;
  }
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
      int kills = 0, deaths = 0, bb = 0, bl = 0;

      findJsonInt(bwJson, "final_kills_bedwars", fk);
      findJsonInt(bwJson, "final_deaths_bedwars", fd);
      findJsonInt(bwJson, "wins_bedwars", wins);
      findJsonInt(bwJson, "losses_bedwars", losses);
      findJsonInt(bwJson, "winstreak", ws);
      findJsonInt(bwJson, "kills_bedwars", kills);
      findJsonInt(bwJson, "deaths_bedwars", deaths);
      findJsonInt(bwJson, "beds_broken_bedwars", bb);
      findJsonInt(bwJson, "beds_lost_bedwars", bl);

      ps.bedwarsFinalKills = fk;
      ps.bedwarsFinalDeaths = fd;
      ps.bedwarsWins = wins;
      ps.bedwarsLosses = losses;
      ps.winstreak = ws;
      ps.bedwarsKills = kills;
      ps.bedwarsDeaths = deaths;
      ps.bedwarsBedsBroken = bb;
      ps.bedwarsBedsLost = bl;
    }
  }

  abyssDbg("PARSE OK uuid=%s star=%d fk=%d wins=%d", uuid.c_str(),
           ps.bedwarsStar, ps.bedwarsFinalKills, ps.bedwarsWins);
  return ps;
}
