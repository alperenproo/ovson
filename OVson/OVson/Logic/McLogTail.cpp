#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Logic/AutoGG.h"
#include "../Utils/Logger.h"
#include "../Utils/NumberDenicker.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace OVson {

std::string getUserProfileDir() {
  char *up = nullptr;
  size_t sz = 0;
  std::string out;
  if (_dupenv_s(&up, &sz, "USERPROFILE") == 0 && up)
    out = up;
  if (up)
    free(up);
  return out;
}

std::string getAppDataDir() {
  char *ad = nullptr;
  size_t sz = 0;
  std::string out;
  if (_dupenv_s(&ad, &sz, "APPDATA") == 0 && ad)
    out = ad;
  if (ad)
    free(ad);
  return out;
}

std::vector<std::string> getLogDirectoryCandidates() {
  std::vector<std::string> candidates;

  // Lunar
  std::string up = getUserProfileDir();
  if (!up.empty()) {
    std::string lunar = up + "\\.lunarclient\\profiles\\lunar\\1.8\\logs";
    DWORD attr = GetFileAttributesA(lunar.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(lunar);
  }

  // Badlion + Vanilla
  std::string ad = getAppDataDir();
  if (!ad.empty()) {
    std::string blmc = ad + "\\.minecraft\\logs\\blclient\\minecraft";
    DWORD attrBl = GetFileAttributesA(blmc.c_str());
    if (attrBl != INVALID_FILE_ATTRIBUTES &&
        (attrBl & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(blmc);

    std::string mc = ad + "\\.minecraft\\logs";
    DWORD attr = GetFileAttributesA(mc.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
      candidates.push_back(mc);
  }

  return candidates;
}

std::string findNewestLogFile(const std::string &dir) {
  WIN32_FIND_DATAA fd{};
  std::string pattern = dir + "\\*.log";
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE)
    return std::string();
  FILETIME best{};
  std::string bestName;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (CompareFileTime(&fd.ftLastWriteTime, &best) > 0) {
        best = fd.ftLastWriteTime;
        bestName = fd.cFileName;
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  if (bestName.empty())
    return std::string();
  return dir + "\\" + bestName;
}

bool ensureLogOpen() {
  std::vector<std::string> candidates = getLogDirectoryCandidates();
  if (candidates.empty())
    return false;

  std::string absoluteBestFile;
  FILETIME absoluteBestTime = {0, 0};

  for (const auto &dir : candidates) {
    std::string newestInDir = findNewestLogFile(dir);
    if (newestInDir.empty())
      continue;

    HANDLE hFile =
        CreateFileA(newestInDir.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
      FILETIME ftWrite;
      if (GetFileTime(hFile, nullptr, nullptr, &ftWrite)) {
        if (CompareFileTime(&ftWrite, &absoluteBestTime) > 0) {
          absoluteBestTime = ftWrite;
          absoluteBestFile = newestInDir;
        }
      }
      CloseHandle(hFile);
    }
  }

  if (absoluteBestFile.empty())
    return false;

  if (g_logFilePath != absoluteBestFile) {
    if (g_logHandle != INVALID_HANDLE_VALUE) {
      CloseHandle(g_logHandle);
      g_logHandle = INVALID_HANDLE_VALUE;
    }
    g_logFilePath = absoluteBestFile;
    g_logOffset = 0;
    g_logBuf.clear();
    Logger::info("Newest log detected across all clients: %s",
                 absoluteBestFile.c_str());
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::General, "Switched to Log File: %s",
                  absoluteBestFile.c_str());
    }
  }

  if (g_logHandle == INVALID_HANDLE_VALUE) {
    if (Config::isGlobalDebugEnabled()) {
      Logger::log(Config::DebugCategory::General, "Opening Log: %s",
                  absoluteBestFile.c_str());
    }
    g_logHandle =
        CreateFileA(g_logFilePath.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_logHandle == INVALID_HANDLE_VALUE)
      return false;
    LARGE_INTEGER sz{};
    if (GetFileSizeEx(g_logHandle, &sz))
      g_logOffset = (long long)sz.QuadPart; // start tailing from end-of-file
  }
  return true;
}

void parsePlayersFromOnlineLine(const std::string &joined) {
  if (!g_inHypixelGame && !g_inPreGameLobby) {
    g_onlinePlayers.clear();
    return;
  }
  std::string listStr = joined.substr(joined.find("ONLINE:") + 7);
  while (!listStr.empty() && listStr.front() == ' ')
    listStr.erase(listStr.begin());
  while (!listStr.empty() && listStr.back() == ' ')
    listStr.pop_back();
  std::vector<std::string> names;
  size_t start = 0;
  for (;;) {
    size_t comma = listStr.find(',', start);
    std::string token =
        listStr.substr(start, comma == std::string::npos ? std::string::npos
                                                         : (comma - start));
    while (!token.empty() && token.front() == ' ')
      token.erase(token.begin());
    while (!token.empty() && token.back() == ' ')
      token.pop_back();
    if (!token.empty())
      names.push_back(token);
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  if (!names.empty()) {
    std::vector<std::string> sorted = names;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::string> prev = g_onlinePlayers;
    std::sort(prev.begin(), prev.end());
    if (sorted == prev)
      return;

    {
      std::lock_guard<std::mutex> lock(g_statsMutex);
      g_onlinePlayers = names;
    }
  }
}

void tailLogOnce() {
  if (!ensureLogOpen())
    return;
  LARGE_INTEGER pos{};
  pos.QuadPart = g_logOffset;
  SetFilePointerEx(g_logHandle, pos, nullptr, FILE_BEGIN);
  char buf[4096];
  DWORD read = 0;
  if (!ReadFile(g_logHandle, buf, sizeof(buf), &read, nullptr) || read == 0)
    return;
  g_logOffset += read;
  g_logBuf.append(buf, buf + read);

  size_t nl;
  while ((nl = g_logBuf.find('\n')) != std::string::npos) {
    std::string line = g_logBuf.substr(0, nl);
    g_logBuf.erase(0, nl + 1);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.find("[CHAT]") == std::string::npos)
      continue;
    size_t p = line.find("[CHAT]");
    std::string chat = (p != std::string::npos) ? line.substr(p + 6) : line;

    NumberDenicker::onChatMessage(chat);

    if (Config::isPreGameChatStatsEnabled()) {
      if (g_inPreGameLobby) {
        std::string cleanChat;
        for (size_t i = 0; i < chat.length(); ++i) {
          unsigned char c = (unsigned char)chat[i];
          if (c == 0xC2 && i + 1 < chat.length() &&
              (unsigned char)chat[i + 1] == 0xA7) {
            i += 2;
            continue;
          }
          if (c == 0xA7) {
            i += 1;
            continue;
          }
          cleanChat += (char)c;
        }

        ULONGLONG nowDbg = GetTickCount64();

        if (Config::isDebugging()) {
          static ULONGLONG lastParseCheck = 0;
          if (nowDbg - lastParseCheck > 100) {
            bool isOVson = (cleanChat.find("[OVson]") != std::string::npos);
            bool isTo = (cleanChat.find("To ") == 0);
            bool isFrom = (cleanChat.find("From ") == 0);
            if (isOVson || isTo || isFrom) {
              ChatSDK::showClientMessage(
                  ChatSDK::formatPrefix() + "\xC2\xA7" +
                  "7[DEBUG] Skipped Line (is DM/Internal): " +
                  cleanChat.substr(0, (std::min)((int)cleanChat.size(), 20)));
            }
          }
        }

        if (cleanChat.find("[OVson]") == std::string::npos &&
            cleanChat.find("To ") != 0 && cleanChat.find("From ") != 0) {
          size_t firstColon = cleanChat.find(": ");
          if (firstColon != std::string::npos && firstColon > 0) {
            std::string prefix = cleanChat.substr(0, firstColon);

            size_t pStart = prefix.find_first_not_of(' ');
            size_t pEnd = prefix.find_last_not_of(' ');
            if (pStart == std::string::npos)
              continue;
            prefix = prefix.substr(pStart, pEnd - pStart + 1);

            std::string username;
            size_t firstBracket = prefix.find('[');

            if (firstBracket != std::string::npos) {
              if (firstBracket > 0)
                continue;
              size_t lastBracket = prefix.find_last_of(']');
              if (lastBracket == std::string::npos)
                continue;
              username = prefix.substr(lastBracket + 1);
              size_t uStart = username.find_first_not_of(' ');
              if (uStart != std::string::npos)
                username = username.substr(uStart);
              if (username.find(' ') != std::string::npos || username.empty()) {
                continue;
              }
            } else {
              if (prefix.find(' ') != std::string::npos)
                continue;
              username = prefix;
            }

            bool valid = (username.length() >= 3 && username.length() <= 16);
            for (char c : username) {
              if (!isalnum((unsigned char)c) && c != '_') {
                valid = false;
                break;
              }
            }

            static ULONGLONG lastParseDbg = 0;
            if (Config::isDebugging() && (nowDbg - lastParseDbg > 1000)) {
              ChatSDK::showClientMessage(
                  ChatSDK::formatPrefix() + "\xC2\xA7" +
                  "e[DEBUG] PreGame Chat Parsed. User: " + username +
                  " Valid: " + (valid ? "Yes" : "No"));
              lastParseDbg = nowDbg;
            }

            if (valid) {
              if (g_chatPrintedPlayers.find(username) ==
                  g_chatPrintedPlayers.end()) {
                g_chatPrintedPlayers.insert(username);

                if (std::find(g_manualPushedPlayers.begin(),
                              g_manualPushedPlayers.end(),
                              username) == g_manualPushedPlayers.end()) {
                  g_manualPushedPlayers.push_back(username);
                }
                if (std::find(g_onlinePlayers.begin(), g_onlinePlayers.end(),
                              username) == g_onlinePlayers.end()) {
                  g_onlinePlayers.push_back(username);
                }

                g_forceChatOutputPlayers.insert(username);

                {
                  std::lock_guard<std::mutex> lockA(g_activeFetchesMutex);
                  if (g_activeFetches.find(username) == g_activeFetches.end()) {
                    g_activeFetches.insert(username);
                    std::thread(fetchWorker, username, "").detach();
                  }
                }
              } else {
                static ULONGLONG lastAlreadyDbg = 0;
                if (Config::isDebugging() && (nowDbg - lastAlreadyDbg > 2000)) {
                  ChatSDK::showClientMessage(
                      ChatSDK::formatPrefix() + "\xC2\xA7" +
                      "7[DEBUG] Chat Processed Already: " + username);
                  lastAlreadyDbg = nowDbg;
                }
              }
            } else if (Config::isDebugging()) {
              static ULONGLONG lastInvalidDbg = 0;
              if (nowDbg - lastInvalidDbg > 3000) {
                ChatSDK::showClientMessage(
                    ChatSDK::formatPrefix() + "\xC2\xA7" +
                    "c[DEBUG] Invalid Username Parsed: " + username);
                lastInvalidDbg = nowDbg;
              }
            }
          }
        }
      } else if (Config::isDebugging()) {
        static ULONGLONG lastLobbyWarn = 0;
        if (GetTickCount64() - lastLobbyWarn > 10000) {
          ChatSDK::showClientMessage(
              ChatSDK::formatPrefix() + "\xC2\xA7" +
              "7[DEBUG] Chat skipped: g_inPreGameLobby is FALSE");
          lastLobbyWarn = GetTickCount64();
        }
      }
    }

    detectTeamsFromLine(chat);
    detectFinalKillsFromLine(chat);
    detectBedDestructionFromLine(chat);
    Logic::AutoGG::handleChat(chat);

    if (chat.find("ONLINE:") != std::string::npos) {
      if (line != g_lastOnlineLine) {
        g_lastOnlineLine = line;
        Logger::log(Config::DebugCategory::GameDetection,
                    "Detected ONLINE list, parsing players...");
        parsePlayersFromOnlineLine(chat);
        g_nextFetchIdx = 0;
        g_processedPlayers.clear();
      }
    }
  }
}

} // namespace OVson
