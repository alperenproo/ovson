#include "Logger.h"
#include "../Chat/ChatSDK.h"
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <winsock2.h>
#include <ws2tcpip.h>

static FILE *g_logFile = nullptr;

static void vwrite(const char *level, const char *fmt, va_list args) {
  char buffer[2048];
  vsnprintf(buffer, sizeof(buffer), fmt, args);

  char finalMsg[2560];
  sprintf_s(finalMsg, "[OVson] [%s] %s\n", level, buffer);

  OutputDebugStringA(finalMsg);

  if (g_logFile) {
    fprintf(g_logFile, "%s", finalMsg);
    fflush(g_logFile);
  }
}

bool Logger::initialize(const char *logFileName) {
  if (!g_logFile) {
    fopen_s(&g_logFile, logFileName, "w");
  }
  return g_logFile != nullptr;
}

void Logger::info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vwrite("INFO", fmt, args);
  va_end(args);
}

void Logger::error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vwrite("ERROR", fmt, args);
  va_end(args);
}

void Logger::log(Config::DebugCategory cat, const char *fmt, ...) {
  if (!Config::isDebugEnabled(cat))
    return;

  const char *level = "GENERAL";
  switch (cat) {
  case Config::DebugCategory::GameDetection:
    level = "GAME_DETECTION";
    break;
  case Config::DebugCategory::BedDetection:
    level = "BED_DETECTION";
    break;
  case Config::DebugCategory::Urchin:
    level = "URCHIN";
    break;
  case Config::DebugCategory::GUI:
    level = "GUI";
    break;
  case Config::DebugCategory::BedDefense:
    level = "BED_DEFENSE";
    break;
  }

  va_list args;
  va_start(args, fmt);
  vwrite(level, fmt, args);
  va_end(args);

  if (Config::isGlobalDebugEnabled()) {
    char buffer[2048];
    va_list chatArgs;
    va_start(chatArgs, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, chatArgs);
    va_end(chatArgs);

    Sleep(50);
    std::string chatMsg =
        "§7[" + std::string(level) + "] §f" + std::string(buffer);
    ChatSDK::showPrefixed(chatMsg);
  }
}

void Logger::shutdown() {
  if (g_logFile) {
    fclose(g_logFile);
    g_logFile = nullptr;
  }
}
