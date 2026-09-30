#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"
#include "../Chat/ChatSDK.h"
#include "../Chat/Commands.h"
#include "../Config/Config.h"
#include "../Chat/ChatHook.h"
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
#include <fstream>

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

  std::string customLunar = Config::getLunarLogPath();
  if (!customLunar.empty()) {
    DWORD attr = GetFileAttributesA(customLunar.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
      if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) return { customLunar };
      candidates.push_back(customLunar);
    }
  } else {
    std::string up = getUserProfileDir();
    if (!up.empty()) {
      std::string lunar = up + "\\.lunarclient\\profiles\\1.8\\logs";
      DWORD attr = GetFileAttributesA(lunar.c_str());
      if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        candidates.push_back(lunar);
    }
  }

  std::string customBadlion = Config::getBadlionLogPath();
  if (!customBadlion.empty()) {
    DWORD attr = GetFileAttributesA(customBadlion.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
      if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) candidates.push_back(customBadlion);
      else candidates.push_back(customBadlion);
    }
  } else {
    std::string ad = getAppDataDir();
    if (!ad.empty()) {
      std::string blmc = ad + "\\.minecraft\\logs\\blclient\\minecraft";
      DWORD attrBl = GetFileAttributesA(blmc.c_str());
      if (attrBl != INVALID_FILE_ATTRIBUTES &&
          (attrBl & FILE_ATTRIBUTE_DIRECTORY))
        candidates.push_back(blmc);
    }
  }

  std::string customLegacy = Config::getLegacyBadlionLogPath();
  if (!customLegacy.empty()) {
    DWORD attr = GetFileAttributesA(customLegacy.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
      candidates.push_back(customLegacy);
    }
  } else {
    std::string ad = getAppDataDir();
    if (!ad.empty()) {
      std::string mc = ad + "\\.minecraft\\logs";
      DWORD attr = GetFileAttributesA(mc.c_str());
      if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        candidates.push_back(mc);
    }
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
  ULONGLONG now = GetTickCount64();
  static ULONGLONG lastScan = 0;
  if (g_logHandle != INVALID_HANDLE_VALUE && (now - lastScan < 3000)) {
    return true;
  }
  lastScan = now;

  std::vector<std::string> candidates = getLogDirectoryCandidates();
  if (candidates.empty())
    return (g_logHandle != INVALID_HANDLE_VALUE);

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
    return (g_logHandle != INVALID_HANDLE_VALUE);

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
      std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
      g_onlinePlayers = names;
    }
  }
}

ULONGLONG g_lastNativeChatReceipt = 0;
static std::mutex s_nativeChatMutex;
static std::vector<std::string> s_nativeChatQueue;

void enqueueNativeChat(const std::string &chat) {
  std::lock_guard<std::mutex> lock(s_nativeChatMutex);
  s_nativeChatQueue.push_back(chat);
}

static bool isIgnoredChatLine(const std::string &cleanChat) {
  std::string trimmed = cleanChat;
  size_t p = trimmed.find_first_not_of(" \t\r\n");
  if (p != std::string::npos) {
    trimmed = trimmed.substr(p);
  }

  if (trimmed.empty()) return true;
  if (trimmed.find("[OVson]") != std::string::npos) return true;

  if (trimmed.rfind("Party", 0) == 0 || trimmed.rfind("[Party]", 0) == 0) return true;

  if (trimmed.rfind("Guild", 0) == 0 || trimmed.rfind("[Guild]", 0) == 0) return true;

  if (trimmed.rfind("To ", 0) == 0 || trimmed.rfind("To:", 0) == 0 ||
      trimmed.rfind("From ", 0) == 0 || trimmed.rfind("From:", 0) == 0 ||
      trimmed.rfind("[To]", 0) == 0 || trimmed.rfind("[From]", 0) == 0) return true;

  if (trimmed.rfind("Officer", 0) == 0 || trimmed.rfind("Co-op", 0) == 0) return true;

  if (trimmed.rfind("Your Online Status", 0) == 0) return true;

  return false;
}

void processRawChatLine(const std::string &chat, const std::string &rawLogLine) {

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

        if (!isIgnoredChatLine(cleanChat)) {
          size_t firstColon = cleanChat.find(": ");
          if (firstColon != std::string::npos && firstColon > 0) {
            std::string prefix = cleanChat.substr(0, firstColon);

            size_t pStart = prefix.find_first_not_of(' ');
            size_t pEnd = prefix.find_last_not_of(' ');
            if (pStart != std::string::npos) {
              prefix = prefix.substr(pStart, pEnd - pStart + 1);
            }

            if (prefix.find("Party") != std::string::npos ||
                prefix.find("Guild") != std::string::npos ||
                prefix.find("Leader") != std::string::npos ||
                prefix.find("Members") != std::string::npos ||
                prefix.find("Moderator") != std::string::npos) {
            } else {

            std::string username;
            size_t lastBracket = prefix.find_last_of(']');

            if (lastBracket != std::string::npos) {
              username = prefix.substr(lastBracket + 1);
            } else {
              username = prefix;
            }

            size_t uStart = username.find_first_not_of(" \t\r\n");
            size_t uEnd = username.find_last_not_of(" \t\r\n");
            if (uStart != std::string::npos) {
              username = username.substr(uStart, uEnd - uStart + 1);
            } else {
              username.clear();
            }

            if (username.find(' ') != std::string::npos) {
              size_t spacePos = username.rfind(' ');
              if (spacePos != std::string::npos) {
                username = username.substr(spacePos + 1);
              }
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
              std::string realName = getRealLocalUsername();
              std::string msgBody = cleanChat.substr(firstColon + 2);
              bool sentBySelf = ChatHook::wasMessageSentRecentlyBySelf(msgBody);
              bool isSelf = (!realName.empty() && username == realName) ||
                            (g_isNicked && !g_activeNick.empty() && username == g_activeNick) ||
                            (!g_localName.empty() && username == g_localName) ||
                            sentBySelf;

              if (sentBySelf && !realName.empty() && username != realName) {
                g_isNicked = true;
                g_activeNick = username;
                g_localName = username;
              }

              std::string queryName = (isSelf && !realName.empty()) ? realName : username;

              if (g_chatPrintedPlayers.find(username) == g_chatPrintedPlayers.end() &&
                  g_chatPrintedPlayers.find(queryName) == g_chatPrintedPlayers.end()) {
                g_chatPrintedPlayers.insert(username);
                if (queryName != username) {
                  g_chatPrintedPlayers.insert(queryName);
                }

                if (std::find(g_manualPushedPlayers.begin(),
                              g_manualPushedPlayers.end(),
                              queryName) == g_manualPushedPlayers.end()) {
                  g_manualPushedPlayers.push_back(queryName);
                }
                if (std::find(g_onlinePlayers.begin(), g_onlinePlayers.end(),
                              queryName) == g_onlinePlayers.end()) {
                  g_onlinePlayers.push_back(queryName);
                }

                g_forceChatOutputPlayers.insert(queryName);

                {
                  std::lock_guard<std::mutex> lockA(g_activeFetchesMutex);
                  if (g_activeFetches.find(queryName) == g_activeFetches.end()) {
                    g_activeFetches.insert(queryName);

                    std::thread(fetchWorker, queryName, "").detach();
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
        }
      } else {

        if (Config::isDebugging()) {
          static ULONGLONG lastLobbyWarn = 0;
          if (GetTickCount64() - lastLobbyWarn > 10000) {
            ChatSDK::showClientMessage(
                ChatSDK::formatPrefix() + "\xC2\xA7" +
                "7[DEBUG] Chat skipped: g_inPreGameLobby is FALSE");
            lastLobbyWarn = GetTickCount64();
          }
        }
      }
    }


    if (!g_inHypixelGame && !g_inPreGameLobby && Config::isLobbyMentionStatsEnabled()) {
      if (!g_localName.empty() && chat.find(":") != std::string::npos) {
        std::string cleanChat;
        for (size_t i = 0; i < chat.length(); ++i) {
          unsigned char c = (unsigned char)chat[i];
          if (c == 0xC2 && i + 2 < chat.length() && (unsigned char)chat[i + 1] == 0xA7) { i += 2; continue; }
          if (c == 0xA7 && i + 1 < chat.length()) { i += 1; continue; }
          cleanChat += (char)c;
        }

        if (!isIgnoredChatLine(cleanChat)) {
          size_t firstColon = cleanChat.find(": ");
          if (firstColon != std::string::npos && firstColon > 0) {
            std::string prefix = cleanChat.substr(0, firstColon);
            size_t pStart = prefix.find_first_not_of(' ');
            size_t pEnd = prefix.find_last_not_of(' ');
            if (pStart != std::string::npos) prefix = prefix.substr(pStart, pEnd - pStart + 1);

            if (prefix.find("Party") == std::string::npos &&
                prefix.find("Guild") == std::string::npos &&
                prefix.find("Leader") == std::string::npos &&
                prefix.find("Members") == std::string::npos &&
                prefix.find("Moderator") == std::string::npos) {

            std::string username;
            size_t lastBracket = prefix.find_last_of(']');
            if (lastBracket != std::string::npos) username = prefix.substr(lastBracket + 1);
            else username = prefix;

            size_t uStart = username.find_first_not_of(" \t\r\n");
            size_t uEnd = username.find_last_not_of(" \t\r\n");
            if (uStart != std::string::npos) username = username.substr(uStart, uEnd - uStart + 1);
            else username.clear();

            if (username.find(' ') != std::string::npos) {
              size_t spacePos = username.rfind(' ');
              if (spacePos != std::string::npos) username = username.substr(spacePos + 1);
            }

            bool valid = (username.length() >= 3 && username.length() <= 16);
            for (char c : username) {
              if (!isalnum((unsigned char)c) && c != '_') { valid = false; break; }
            }
            std::string realLocal = getRealLocalUsername();
            std::string msgBodyStr = cleanChat.substr(firstColon + 2);
            bool sentByMe = ChatHook::wasMessageSentRecentlyBySelf(msgBodyStr);
            if (username == g_localName || (!realLocal.empty() && username == realLocal) || (g_isNicked && !g_activeNick.empty() && username == g_activeNick) || sentByMe) {
              valid = false;
            }

            if (valid) {
              std::string msgBody = cleanChat.substr(firstColon + 2);
              std::string lowerMsg = msgBody;
              std::string lowerName = g_localName;
              for (char& c : lowerMsg) c = (char)tolower((unsigned char)c);
              for (char& c : lowerName) c = (char)tolower((unsigned char)c);

              if (lowerMsg.find(lowerName) != std::string::npos) {
                ULONGLONG currentTick = GetTickCount64();
                bool canTrigger = false;
                if (g_autoStatsCooldowns.find(username) == g_autoStatsCooldowns.end()) canTrigger = true;
                else if (currentTick - g_autoStatsCooldowns[username] >= 600000ULL) canTrigger = true;
                
                if (canTrigger) {
                  g_autoStatsCooldowns[username] = currentTick;
                  std::string cmd = Config::getCommandPrefix() + "stats -s " + username;
                  CommandRegistry::instance().tryDispatch(cmd);
                }
              }
            }
            }
          }
        }
      }
    }

    detectTeamsFromLine(chat);
    detectFinalKillsFromLine(chat);
    detectBedDestructionFromLine(chat);
    Logic::AutoGG::handleChat(chat);

    if (chat.find("ONLINE:") != std::string::npos) {
      std::string compLine = rawLogLine.empty() ? chat : rawLogLine;
      if (compLine != g_lastOnlineLine) {
        g_lastOnlineLine = compLine;
        Logger::log(Config::DebugCategory::GameDetection,
                    "Detected ONLINE list, parsing players...");
        parsePlayersFromOnlineLine(chat);
        g_nextFetchIdx = 0;
        g_processedPlayers.clear();
      }
    }
}

void tailLogOnce() {
  {
    std::vector<std::string> pendingChats;
    {
      std::lock_guard<std::mutex> lock(s_nativeChatMutex);
      pendingChats = s_nativeChatQueue;
      s_nativeChatQueue.clear();
    }
    for (const auto &chat : pendingChats) {
      g_lastNativeChatReceipt = GetTickCount64();
      processRawChatLine(chat, "");
    }
  }

  if (!ensureLogOpen())
    return;
    
  bool nativeIsWorking = (GetTickCount64() - g_lastNativeChatReceipt < 60000);

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
      
    if (nativeIsWorking)
      continue;

    size_t p = line.find("[CHAT]");
    std::string chat = (p != std::string::npos) ? line.substr(p + 7) : line;
    while (!chat.empty() && chat.front() == ' ') chat.erase(chat.begin());

    processRawChatLine(chat, line);
  }
}
} // namespace OVson
