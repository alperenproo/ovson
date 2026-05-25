#include "NumberDenicker.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Logic/StatsTracker.h"
#include "../Services/AuroraService.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/ThreadTracker.h"

#include <algorithm>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace NumberDenicker {

struct ResolutionContext {
  std::vector<std::string> exactCandidates;

  bool finalsProcessed = false;
  bool bedsProcessed = false;

  std::vector<std::string> fuzzyFinals;
  std::vector<std::string> fuzzyBeds;

  bool hasFuzzyFinals = false;
  bool hasFuzzyBeds = false;
};

static bool s_isSessionActive = false;
static std::mutex s_trackerMutex;
static std::unordered_map<std::string, ResolutionContext> s_activeTrackers;

static const std::regex
    REGEX_FINAL_KILL(R"(^(\w+) was ([\w-]+)'s final #([\d,]+)\. FINAL KILL!$)");
static const std::regex REGEX_BED_BROKEN(
    R"(^(?:BED DESTRUCTION > )?(\w+) (?:Bed|bed) was bed #([\d,]+) destroyed by ([\w-]+)!$)");

static std::string StripFormatCodes(const std::string &source) {
  std::string result;
  result.reserve(source.size());
  for (size_t i = 0; i < source.size();) {
    if ((unsigned char)source[i] == 0xC2 && i + 1 < source.size() &&
        (unsigned char)source[i + 1] == 0xA7) {
      i += 3;
    } else if ((unsigned char)source[i] == 0xA7 && i + 1 < source.size()) {
      i += 2;
    } else {
      result += source[i++];
    }
  }
  return result;
}

static std::string FilterCommas(const std::string &input) {
  std::string result = input;
  result.erase(std::remove(result.begin(), result.end(), ','), result.end());
  return result;
}

static bool DetectMatchStart(const std::string &msg) {
  if (msg.find("Protect your bed and destroy the enemy beds.") !=
      std::string::npos)
    return true;
  if (msg.find("You will respawn because you still have a bed!") !=
          std::string::npos &&
      msg.find(":") == std::string::npos &&
      msg.find("SHOUT") == std::string::npos)
    return true;
  return false;
}

static bool VerifyPlayerNickState(const std::string &playerName) {
  std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
  auto it = OVson::g_playerStatsMap.find(playerName);
  return (it != OVson::g_playerStatsMap.end() && it->second.isNicked);
}

static void InitiateResolutionTask(const std::string &statType,
                                   const std::string &alias,
                                   const std::string &rawValue) {
  std::lock_guard<std::mutex> lock(s_trackerMutex);
  if (s_activeTrackers.find(alias) == s_activeTrackers.end())
    return;

  ThreadTracker::increment();
  if (ThreadTracker::g_activeThreads.load() > 12) {
    ThreadTracker::decrement();
    return;
  }

  std::string taskType = statType;
  std::string taskAlias = alias;
  std::string taskValue = rawValue;

  std::thread([taskType, taskAlias, taskValue]() {
    SafeGuard::installSehTranslator();
    SafeGuard::run("NumberDenicker::Task", [&]() {
      std::string authKey = Config::getAuroraApiKey();
      if (authKey.empty()) {
        ChatSDK::showPrefixed(
            "\xA7"
            "cAurora API key missing. Set it with .aurora <key>");
        return;
      }

      int tolerance = 200;
      int limit = 20;

      auto apiResponse =
          Aurora::queryStats(taskType, taskValue, tolerance, limit, authKey);
      if (!apiResponse || !apiResponse->success) {
        ChatSDK::showPrefixed("\xA7"
                              "cAurora lookup failed for " +
                              taskAlias);
        return;
      }

      std::vector<std::string> resultsFuzzy;
      std::vector<std::string> resultsExact;

      std::vector<std::string> snippets;
      std::string currentLine;

      for (const auto &entry : apiResponse->data) {
        if (entry.distance <= tolerance) {
          resultsFuzzy.push_back(entry.name);

          std::string segment = "\xA7"
                                "a" +
                                entry.name +
                                " \xA7"
                                "7(" +
                                std::to_string(entry.distance) + ")";
          if (currentLine.length() + segment.length() > 80) {
            snippets.push_back(currentLine);
            currentLine = segment;
          } else {
            if (!currentLine.empty())
              currentLine += ", ";
            currentLine += segment;
          }
        }
        if (entry.distance <= 0) {
          resultsExact.push_back(entry.name);
        }
      }
      if (!currentLine.empty())
        snippets.push_back(currentLine);

      if (!snippets.empty()) {
        ChatSDK::showPrefixed("\xA7"
                              "aPossible " +
                              taskType + " origins:");
        for (const auto &s : snippets) {
          ChatSDK::showPrefixed(" \xA7"
                                "7> " +
                                s);
        }
      }

      {
        std::lock_guard<std::mutex> lock(s_trackerMutex);
        auto it = s_activeTrackers.find(taskAlias);
        if (it == s_activeTrackers.end())
          return;

        ResolutionContext &ctx = it->second;

        if (taskType == "finals") {
          ctx.fuzzyFinals = resultsFuzzy;
          ctx.hasFuzzyFinals = true;
          ctx.finalsProcessed = true;
        } else if (taskType == "beds") {
          ctx.fuzzyBeds = resultsFuzzy;
          ctx.hasFuzzyBeds = true;
          ctx.bedsProcessed = true;
        }

        if (!resultsExact.empty()) {
          if (ctx.exactCandidates.empty() && !ctx.finalsProcessed &&
              !ctx.bedsProcessed) {
            ctx.exactCandidates = resultsExact;
          } else if (!ctx.exactCandidates.empty()) {
            std::vector<std::string> shared;
            for (const auto &existing : ctx.exactCandidates) {
              for (const auto &match : resultsExact) {
                if (existing == match) {
                  shared.push_back(existing);
                  break;
                }
              }
            }
            ctx.exactCandidates = shared;
          } else {
            ctx.exactCandidates = resultsExact;
          }
        }

        if (ctx.finalsProcessed && ctx.bedsProcessed) {
          if (!ctx.exactCandidates.empty()) {
            ChatSDK::showPrefixed("\xA7"
                                  "6" +
                                  ctx.exactCandidates[0] +
                                  "\xA7"
                                  "7 identified as " +
                                  taskAlias);
          } else if (ctx.hasFuzzyFinals && ctx.hasFuzzyBeds) {
            std::vector<std::string> sharedFuzzy;
            for (const auto &f : ctx.fuzzyFinals) {
              for (const auto &b : ctx.fuzzyBeds) {
                if (f == b) {
                  sharedFuzzy.push_back(f);
                  break;
                }
              }
            }
            if (!sharedFuzzy.empty()) {
              std::string candidateList;
              for (const auto &name : sharedFuzzy) {
                if (!candidateList.empty())
                  candidateList += ", ";
                candidateList += name;
              }
              ChatSDK::showPrefixed("\xA7"
                                    "aFuzzy resolution for " +
                                    taskAlias + ": " + candidateList);
            } else {
              ChatSDK::showPrefixed("\xA7"
                                    "cNo convergent matches for " +
                                    taskAlias);
            }
          } else {
            ChatSDK::showPrefixed("\xA7"
                                  "cResolution failed for " +
                                  taskAlias);
          }
        }
      }
    });
    ThreadTracker::decrement();
  }).detach();
}

bool isEnabled() { return Config::isNumberDenickerEnabled(); }

void onWorldChange() {
  std::lock_guard<std::mutex> lock(s_trackerMutex);
  s_isSessionActive = false;
  s_activeTrackers.clear();
}

void onChatMessage(const std::string &input) {
  if (!isEnabled())
    return;

  std::string cleanMsg = StripFormatCodes(input);

  size_t startIdx = cleanMsg.find_first_not_of(" \t\r\n");
  size_t endIdx = cleanMsg.find_last_not_of(" \t\r\n");
  if (startIdx == std::string::npos)
    return;
  cleanMsg = cleanMsg.substr(startIdx, endIdx - startIdx + 1);

  if (DetectMatchStart(cleanMsg)) {
    std::lock_guard<std::mutex> lock(s_trackerMutex);
    s_isSessionActive = true;
    return;
  }

  if (!s_isSessionActive)
    return;

  std::smatch matchData;
  if (std::regex_match(cleanMsg, matchData, REGEX_FINAL_KILL)) {
    std::string alias = matchData[2].str();
    std::string statVal = FilterCommas(matchData[3].str());

    try {
      if (std::stoi(statVal) >= 100 && OVson::isInGame(alias) &&
          VerifyPlayerNickState(alias)) {
        {
          std::lock_guard<std::mutex> lock(s_trackerMutex);
          auto &ctx = s_activeTrackers[alias];
          if (ctx.finalsProcessed && ctx.hasFuzzyFinals)
            return;
        }
        ChatSDK::showPrefixed("\xA7"
                              "aAnalyzing " +
                              alias + " via " + statVal + " finals...");
        InitiateResolutionTask("finals", alias, statVal);
      }
    } catch (...) {
    }
    return;
  }

  if (std::regex_match(cleanMsg, matchData, REGEX_BED_BROKEN)) {
    std::string alias = matchData[3].str();
    std::string statVal = FilterCommas(matchData[2].str());

    if (OVson::isInGame(alias) && VerifyPlayerNickState(alias)) {
      {
        std::lock_guard<std::mutex> lock(s_trackerMutex);
        auto &ctx = s_activeTrackers[alias];
        if (ctx.bedsProcessed && ctx.hasFuzzyBeds)
          return;
      }
      ChatSDK::showPrefixed("\xA7"
                            "aAnalyzing " +
                            alias + " via " + statVal + " beds...");
      InitiateResolutionTask("beds", alias, statVal);
    }
    return;
  }
}

} // namespace NumberDenicker
