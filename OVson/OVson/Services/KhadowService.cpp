#include "KhadowService.h"
#include "../Config/Config.h"
#include "../Logic/StatsTracker.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/ThreadTracker.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Khadow {

namespace {

struct CachedEntry {
  AnticheatInfo data;
  std::chrono::steady_clock::time_point timestamp;
};

constexpr size_t kCacheMax        = 200;
constexpr int    kCacheExpirySec  = 300;
constexpr int    kPendingThrottleSec = 10;
constexpr const char *kEndpoint   = "https://api.khadow.lol/stats";

std::unordered_map<std::string, CachedEntry> g_cache;
std::mutex                                   g_cacheMutex;

std::unordered_map<std::string, std::chrono::steady_clock::time_point>
    g_pending;
std::mutex g_pendingMutex;

bool findJsonString(const std::string &json, const char *key,
                    std::string &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos) return false;
  size_t colon = json.find(':', k);
  if (colon == std::string::npos) return false;
  size_t q1 = json.find('"', colon);
  if (q1 == std::string::npos) return false;
  size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos) return false;
  out = json.substr(q1 + 1, q2 - (q1 + 1));
  return true;
}

bool findJsonBool(const std::string &json, const char *key, bool &out) {
  std::string pat = std::string("\"") + key + "\"";
  size_t k = json.find(pat);
  if (k == std::string::npos) return false;
  size_t colon = json.find(':', k);
  if (colon == std::string::npos) return false;
  size_t i = colon + 1;
  while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
  if (i + 4 <= json.size() && json.compare(i, 4, "true") == 0) {
    out = true;  return true;
  }
  if (i + 5 <= json.size() && json.compare(i, 5, "false") == 0) {
    out = false; return true;
  }
  return false;
}

void pruneLocked() {
  auto now = std::chrono::steady_clock::now();
  for (auto it = g_cache.begin(); it != g_cache.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.timestamp).count();
    if (age > kCacheExpirySec) it = g_cache.erase(it);
    else ++it;
  }
  while (g_cache.size() > kCacheMax) {
    auto oldest = g_cache.begin();
    for (auto it = g_cache.begin(); it != g_cache.end(); ++it) {
      if (it->second.timestamp < oldest->second.timestamp) oldest = it;
    }
    g_cache.erase(oldest);
  }
}

AnticheatInfo parseResponse(const std::string &body) {
  AnticheatInfo info;

  findJsonBool(body, "seraph_blacklisted", info.seraphBlacklisted);
  if (info.seraphBlacklisted) {
    findJsonString(body, "seraph_report_type", info.seraphType);
    findJsonString(body, "seraph_reason",      info.seraphReason);
  }

  std::string urchStatus;
  findJsonString(body, "urchin_status", urchStatus);
  info.urchinBlacklisted = (urchStatus == "Blacklisted");
  if (info.urchinBlacklisted) {
    findJsonString(body, "urchin_type",   info.urchinType);
    findJsonString(body, "urchin_reason", info.urchinReason);
  }
  return info;
}

bool isEnabled() {
  if (!Config::isTagsEnabled()) return false;
  const std::string &svc = Config::getActiveTagService();
  return svc == "Khadow";
}

bool doFetch(const std::string &username, AnticheatInfo &out) {
  std::string jsonBody =
      "{\"username\":\"" + username + "\",\"fetch_type\":\"anticheat\"}";
  std::string body;
  if (!Http::postJson(kEndpoint, jsonBody, body)) return false;
  if (body.empty()) return false;
  if (body.find("\"error\"") != std::string::npos &&
      body.find("\"status\"") == std::string::npos) {
    return false;
  }
  out = parseResponse(body);
  return true;
}

} // namespace

std::optional<AnticheatInfo> getPlayerAnticheat(const std::string &username,
                                                bool wait) {
  if (!isEnabled()) return std::nullopt;

  auto now = std::chrono::steady_clock::now();

  // cache hit?
  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_cache.find(username);
    if (it != g_cache.end()) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
                     now - it->second.timestamp).count();
      if (age < kCacheExpirySec) {
        return it->second.data;
      }
    }
  }

  if (!wait && OVson::isInPreGameLobby()) return std::nullopt;

  if (wait) {
    AnticheatInfo info;
    if (!doFetch(username, info)) return std::nullopt;
    {
      std::lock_guard<std::mutex> lock(g_cacheMutex);
      pruneLocked();
      g_cache[username] = {info, std::chrono::steady_clock::now()};
    }
    return info;
  }

  {
    std::lock_guard<std::mutex> lock(g_pendingMutex);
    auto it = g_pending.find(username);
    if (it != g_pending.end()) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
                     now - it->second).count();
      if (age < kPendingThrottleSec) return std::nullopt;
    }
    g_pending[username] = now;
  }

  ThreadTracker::increment();
  if (ThreadTracker::g_activeThreads.load() > 12) {
    ThreadTracker::decrement();
    std::lock_guard<std::mutex> lock(g_pendingMutex);
    g_pending.erase(username);
    return std::nullopt;
  }

  std::thread([username]() {
    SafeGuard::installSehTranslator();
    SafeGuard::run("Khadow::worker", [&]() {
      if (ThreadTracker::shouldStop()) return;
      AnticheatInfo info;
      bool ok = doFetch(username, info);
      if (ThreadTracker::shouldStop()) return;
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        pruneLocked();
        g_cache[username] = {info, std::chrono::steady_clock::now()};
      }
      {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pending.erase(username);
      }
      if (ok && Config::isGlobalDebugEnabled()) {
        Logger::log(Config::DebugCategory::General,
                    "Khadow: %s urchin=%d seraph=%d",
                    username.c_str(),
                    info.urchinBlacklisted ? 1 : 0,
                    info.seraphBlacklisted ? 1 : 0);
      }
    });
    ThreadTracker::decrement();
  }).detach();

  return std::nullopt;
}

void clearCache() {
  std::lock_guard<std::mutex> lock(g_cacheMutex);
  g_cache.clear();
}

} // namespace Khadow
