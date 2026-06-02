#include "Logger.h"
#include "../Chat/ChatSDK.h"
#include <Windows.h>
#include <ShlObj.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Shell32.lib")

static HANDLE g_logHandle = INVALID_HANDLE_VALUE;

static void rawWrite(const char *msg, int len) {
  if (g_logHandle == INVALID_HANDLE_VALUE || len <= 0) return;
  DWORD written = 0;
  WriteFile(g_logHandle, msg, (DWORD)len, &written, nullptr);
  FlushFileBuffers(g_logHandle);
}

static void vwrite(const char *level, const char *fmt, va_list args) {
  char buffer[2048];
  vsnprintf(buffer, sizeof(buffer), fmt, args);

  char finalMsg[2560];
  int n = sprintf_s(finalMsg, "[OVson] [%s] %s\n", level, buffer);

  OutputDebugStringA(finalMsg);

  if (n > 0) {
    rawWrite(finalMsg, n);
  }
}

bool Logger::initialize(const char *logFileName) {
  if (g_logHandle != INVALID_HANDLE_VALUE) return true;

  wchar_t appData[MAX_PATH] = L"";
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                  SHGFP_TYPE_CURRENT, appData))) {
    wchar_t logDir[MAX_PATH];
    swprintf_s(logDir, L"%s\\OVson", appData);
    CreateDirectoryW(logDir, nullptr);
    swprintf_s(logDir, L"%s\\OVson\\logs", appData);
    CreateDirectoryW(logDir, nullptr);
    wchar_t logPath[MAX_PATH];
    swprintf_s(logPath, L"%s\\OVson_%lu.log", logDir,
               (unsigned long)GetCurrentProcessId());

    g_logHandle = CreateFileW(logPath, GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE
                                | FILE_SHARE_DELETE,
                              nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
  }

  if (g_logHandle == INVALID_HANDLE_VALUE && logFileName) {
    int len = MultiByteToWideChar(CP_UTF8, 0, logFileName, -1, nullptr, 0);
    if (len > 0 && len < MAX_PATH) {
      wchar_t wpath[MAX_PATH];
      MultiByteToWideChar(CP_UTF8, 0, logFileName, -1, wpath, MAX_PATH);
      g_logHandle = CreateFileW(wpath, GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE
                                  | FILE_SHARE_DELETE,
                                nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    }
  }
  return g_logHandle != INVALID_HANDLE_VALUE;
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
  if (g_logHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(g_logHandle);
    g_logHandle = INVALID_HANDLE_VALUE;
  }
}
