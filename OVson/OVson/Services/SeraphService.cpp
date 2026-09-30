#include "SeraphService.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Logic/StatsTracker.h"
#include "../Net/Http.h"
#include "../Render/NotificationManager.h"
#include "../Utils/Logger.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/ThreadTracker.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace Seraph {
struct CachedTags {
  PlayerTags data;
  std::chrono::steady_clock::time_point timestamp;
};

static std::unordered_map<std::string, CachedTags> g_cache;
static std::mutex g_cacheMutex;
static const size_t MAX_CACHE_SIZE = 200;
static const int CACHE_EXPIRY_SECONDS = 300;

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

static void pruneCacheLocked() {
  auto now = std::chrono::steady_clock::now();
  for (auto it = g_cache.begin(); it != g_cache.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.timestamp)
                   .count();
    if (age > CACHE_EXPIRY_SECONDS) {
      it = g_cache.erase(it);
    } else {
      ++it;
    }
  }
  while (g_cache.size() > MAX_CACHE_SIZE) {
    auto oldest = g_cache.begin();
    for (auto it = g_cache.begin(); it != g_cache.end(); ++it) {
      if (it->second.timestamp < oldest->second.timestamp)
        oldest = it;
    }
    g_cache.erase(oldest);
  }
}

static std::string toLower(const std::string &str) {
  std::string s = str;
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}

static std::unordered_map<std::string, std::chrono::steady_clock::time_point>
    g_pendingFetches;
static std::mutex g_pendingMutex;

std::optional<PlayerTags> getPlayerTags(const std::string &username,
                                        const std::string &uuid, bool wait) {
  if (!Config::isTagsEnabled()) {
    Logger::tagDebug("[Seraph] Fetch skipped for '%s': Tags disabled in config", username.c_str());
    return std::nullopt;
  }
  std::string activeSvc = Config::getActiveTagService();
  if (activeSvc != "Seraph" && activeSvc != "Both") {
    Logger::tagDebug("[Seraph] Fetch skipped for '%s': Active tag service is '%s'", username.c_str(), activeSvc.c_str());
    return std::nullopt;
  }
  if (uuid.empty()) {
    Logger::tagDebug("[Seraph] Fetch skipped for '%s': UUID is empty", username.c_str());
    return std::nullopt;
  }

  std::string lowerUuid = toLower(uuid);
  auto now = std::chrono::steady_clock::now();

  {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_cache.find(lowerUuid);
    if (it != g_cache.end()) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
                     now - it->second.timestamp)
                     .count();
      if (age < CACHE_EXPIRY_SECONDS) {
        Logger::tagDebug("[Seraph] Cache hit for '%s' (uuid: %s, age: %llds, tags: %zu)", username.c_str(), lowerUuid.c_str(), (long long)age, it->second.data.tags.size());
        return it->second.data;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(g_pendingMutex);
    if (g_pendingFetches.count(lowerUuid)) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
                     now - g_pendingFetches[lowerUuid])
                     .count();
      if (age < 10) {
        if (!wait) {
          Logger::tagDebug("[Seraph] Async fetch skipped (already pending) for '%s' (uuid: %s)", username.c_str(), lowerUuid.c_str());
          return std::nullopt;
        }
      }
    }
    g_pendingFetches[lowerUuid] = now;
  }

  if (wait) {
    std::string url = "https://api.seraph.si/" + lowerUuid + "/blacklist";
    std::string apiKey = Config::getSeraphApiKey();
    if (apiKey.empty()) {
      std::lock_guard<std::mutex> lock(g_pendingMutex);
      g_pendingFetches.erase(lowerUuid);
      Logger::tagDebug("[Seraph] Sync fetch failed for '%s': API key is empty", username.c_str());
      return std::nullopt;
    }

    std::string body;
    Logger::tagDebug("[Seraph] Sync GET request for '%s' (uuid: %s)", username.c_str(), lowerUuid.c_str());
    bool ok = Http::get(url, body, "seraph-api-key", apiKey);
    Logger::tagDebug("[Seraph] Http::get ok=%d, bodyLen=%zu", ok ? 1 : 0, body.size());

    PlayerTags result;
    result.uuid = lowerUuid;

    if (ok && !body.empty() &&
        body.find("\"success\":true") != std::string::npos) {
      size_t blacklistPos = body.find("\"blacklist\"");
      if (blacklistPos != std::string::npos) {
        std::string blacklistSection = body.substr(blacklistPos);
        size_t endPos = blacklistSection.find("},");
        if (endPos == std::string::npos)
          endPos = blacklistSection.find("}");
        if (endPos != std::string::npos) {
          blacklistSection = blacklistSection.substr(0, endPos + 1);
        }

        if (blacklistSection.find("\"tagged\":true") != std::string::npos) {
          std::string reportType, tooltip;
          findJsonString(blacklistSection, "report_type", reportType);
          findJsonString(blacklistSection, "tooltip", tooltip);

          size_t parenPos = tooltip.rfind('(');
          if (parenPos != std::string::npos &&
              tooltip.find("by", parenPos) != std::string::npos) {
            tooltip = tooltip.substr(0, parenPos);
            while (!tooltip.empty() && isspace(tooltip.back()))
              tooltip.pop_back();
          }

          if (reportType.empty())
            reportType = "Seraph Blacklist";
          result.tags.push_back({reportType, tooltip});
        }
      }
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        pruneCacheLocked();
        g_cache[lowerUuid] = {result, std::chrono::steady_clock::now()};
      }
      {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pendingFetches.erase(lowerUuid);
      }
      Logger::tagDebug("[Seraph] Sync Success for '%s' (uuid: %s, tags: %zu)", username.c_str(), lowerUuid.c_str(), result.tags.size());
      for (const auto &t : result.tags) {
        Logger::tagDebug("  -> Tag: type='%s', reason='%s'", t.type.c_str(), t.reason.c_str());
      }
      return result;
    } else {
      {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        pruneCacheLocked();
        g_cache[lowerUuid] = {result, std::chrono::steady_clock::now()};
      }
      {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pendingFetches.erase(lowerUuid);
      }
      Logger::tagDebug("[Seraph] Sync Failed for '%s', ok=%d, bodySnippet: %s", username.c_str(), ok ? 1 : 0, body.substr(0, 150).c_str());
    }
    return std::nullopt;
  }

  ThreadTracker::increment();
  if (ThreadTracker::g_activeThreads.load() > 12) {
    ThreadTracker::decrement();
    std::lock_guard<std::mutex> lock(g_pendingMutex);
    g_pendingFetches.erase(lowerUuid);
    Logger::tagDebug("[Seraph] Async thread skipped for '%s': Too many active threads (%d)", username.c_str(), ThreadTracker::g_activeThreads.load());
    return std::nullopt;
  }

  std::thread([username, lowerUuid]() {
    SafeGuard::installSehTranslator();
    SafeGuard::run("Seraph::worker", [&]() {
      if (ThreadTracker::shouldStop()) return;
      std::string url = "https://api.seraph.si/" + lowerUuid + "/blacklist";
      std::string apiKey = Config::getSeraphApiKey();
      if (apiKey.empty()) {
        Logger::tagDebug("[Seraph] Async worker failed for '%s': API key is empty", username.c_str());
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pendingFetches.erase(lowerUuid);
        return;
      }

      std::string body;
      Logger::tagDebug("[Seraph] Async worker started for '%s' (uuid: %s)", username.c_str(), lowerUuid.c_str());
      bool ok = Http::get(url, body, "seraph-api-key", apiKey);
      Logger::tagDebug("[Seraph] Async Http::get ok=%d, bodyLen=%zu", ok ? 1 : 0, body.size());

      PlayerTags result;
      result.uuid = lowerUuid;

      if (ok && !body.empty() &&
          body.find("\"success\":true") != std::string::npos) {
        size_t blacklistPos = body.find("\"blacklist\"");
        if (blacklistPos != std::string::npos) {
          std::string blacklistSection = body.substr(blacklistPos);
          size_t endPos = blacklistSection.find("},");
          if (endPos == std::string::npos)
            endPos = blacklistSection.find("}");
          if (endPos != std::string::npos) {
            blacklistSection = blacklistSection.substr(0, endPos + 1);
          }

          if (blacklistSection.find("\"tagged\":true") != std::string::npos) {
            std::string reportType, tooltip;
            findJsonString(blacklistSection, "report_type", reportType);
            findJsonString(blacklistSection, "tooltip", tooltip);

            size_t parenPos = tooltip.rfind('(');
            if (parenPos != std::string::npos &&
                tooltip.find("by", parenPos) != std::string::npos) {
              tooltip = tooltip.substr(0, parenPos);
              while (!tooltip.empty() && isspace(tooltip.back()))
                tooltip.pop_back();
            }

            if (reportType.empty())
              reportType = "Seraph Blacklist";
            result.tags.push_back({reportType, tooltip});
          }
        }
        {
          std::lock_guard<std::mutex> lock(g_cacheMutex);
          pruneCacheLocked();
          g_cache[lowerUuid] = {result, std::chrono::steady_clock::now()};
        }
        Logger::tagDebug("[Seraph] Async Success for '%s' (uuid: %s, tags: %zu)", username.c_str(), lowerUuid.c_str(), result.tags.size());
        for (const auto &t : result.tags) {
          Logger::tagDebug("  -> Tag: type='%s', reason='%s'", t.type.c_str(), t.reason.c_str());
        }
      } else {
        {
          std::lock_guard<std::mutex> lock(g_cacheMutex);
          pruneCacheLocked();
          g_cache[lowerUuid] = {result, std::chrono::steady_clock::now()};
        }
        Logger::tagDebug("[Seraph] Async Failed for '%s', ok=%d, bodySnippet: %s", username.c_str(), ok ? 1 : 0, body.substr(0, 150).c_str());
      }

      {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        g_pendingFetches.erase(lowerUuid);
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

bool hasAnyTags(const std::string &username, const std::string &uuid) {
  auto res = getPlayerTags(username, uuid);
  return res.has_value() && !res->tags.empty();
}
} // namespace Seraph
