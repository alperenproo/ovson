#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Java.h"
#include "Net/Http.h"
#include <Windows.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#include <iptypes.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#include "Chat/ChatHook.h"
#include "Logic/StatsTracker.h"
#include "Utils/Anticheat/Anticheat.h"
#include "Chat/ChatSDK.h"
#include "Chat/Commands.h"
#include "Config/Config.h"
#include "Render/RenderHook.h"
#include "Render/TextureLoader.h"
#include "Plugins/PluginLoader.h"
#include "Chat/ChatAPI_Bridge.h"
#include "JavaHook/JavaHook.h"
#include "Services/DiscordManager.h"
#include "Services/IrcService.h"
#include "Logic/PacketHook.h"
#include "Logic/Bedwars/BedwarsConfig.h"
#include "Logic/Bedwars/BedwarsRuntime.h"
#include "Logic/NickRoll/NickRollRuntime.h"
#include "Utils/Logger.h"
#include <ShlObj.h>
#include "Utils/ReplaySpammer.h"
#include "Utils/SafeGuard.h"
#include "Utils/ThreadTracker.h"
#include <stdint.h>
#include <stdio.h>
#include <string>


Lunar::DiagnosticReporter Lunar::reporter = nullptr;

FILE *file = nullptr;
static HANDLE g_loadedEvent = nullptr;
static HANDLE g_sharedMap = nullptr;
static volatile LONG *g_sharedFlag = nullptr;
static HANDLE g_injectedMutex = nullptr;
static HANDLE g_aliveEvent = nullptr;
static HANDLE g_uninjectEvent = nullptr;
static HANDLE g_uninjectEventGlobal = nullptr;
static DWORD g_uninjectLocalError = 0;
static DWORD g_uninjectGlobalError = 0;

// retard filter
static const char *kFilteredPlayerName = "PookieBear";
static const char *kFilteredPlayerMessage = "kys retard ass nigger";


namespace {

bool foregroundIsGame() {
  HWND fg = GetForegroundWindow();
  if (!fg) {
    return false;
  }
  DWORD pid = 0;
  GetWindowThreadProcessId(fg, &pid);
  if (pid == GetCurrentProcessId()) {
    return true;
  }
  HWND game = static_cast<HWND>(RenderHook::gameWindowHandle());
  if (!game || !IsWindow(game)) {
    return false;
  }
  if (fg == game || IsChild(fg, game) || IsChild(game, fg)) {
    return true;
  }
  return GetAncestor(fg, GA_ROOT) == GetAncestor(game, GA_ROOT);
}

void logForegroundRejection(int keyCode) {
  HWND fg = GetForegroundWindow();
  DWORD pid = 0;
  char cls[128] = {0};
  char title[128] = {0};
  if (fg) {
    GetWindowThreadProcessId(fg, &pid);
    GetClassNameA(fg, cls, sizeof(cls) - 1);
    GetWindowTextA(fg, title, sizeof(title) - 1);
  }
  Logger::info("[Uninject] key 0x%02X is down but the foreground window is "
               "not ours: hwnd=%p pid=%lu class='%s' title='%s' "
               "(ourPid=%lu gameHwnd=%p)",
               keyCode, (void *)fg, (unsigned long)pid, cls, title,
               (unsigned long)GetCurrentProcessId(),
               RenderHook::gameWindowHandle());
}

void logUninjectSetup() {
  Logger::info("[Uninject] hotkey %s, key=0x%02X; events: Local=%s (err=%lu) "
               "Global=%s (err=%lu); pid=%lu",
               Config::isUninjectKeyEnabled() ? "enabled" : "DISABLED",
               Config::getUninjectKey(),
               g_uninjectEvent ? "ok" : "FAILED",
               (unsigned long)g_uninjectLocalError,
               g_uninjectEventGlobal ? "ok" : "FAILED",
               (unsigned long)g_uninjectGlobalError,
               (unsigned long)GetCurrentProcessId());
}

} // namespace

void init(void *instance) {

  {
    HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\OVsonCp1");
    if (ev) {
      SetEvent(ev);
      CloseHandle(ev);
    }
  }

  jsize count = 0;
  for (int retry = 0; retry < 20; ++retry) {
    if (JNI_GetCreatedJavaVMs(&lc->vm, 1, &count) == JNI_OK && count > 0) {
      break;
    }
    Sleep(100);
  }

  if (count == 0 || !lc->vm) {
    return;
  }

  if (lc->getEnv() != nullptr) {
    Logger::initialize();
    ChatSDK::initialize();
    Logger::info("OVson initialized");
    Logger::info("=== BUILD MARKER: loader-detect-v1 ===");

    lc->GetLoadedClasses();
    Config::initialize(static_cast<HMODULE>(instance));
    BedDefense::TextureLoader::setModule(static_cast<HMODULE>(instance));
    OVson::Bedwars::Configuration::initialize();
    RegisterDefaultCommands();
    OVson::initialize();
    Anticheat::initialize();
    
    PluginLoader::initialize();
    JavaHook::initialize();
    IrcService::initialize();
    
    if (!ChatHook::install()) {
      Logger::error("ChatHook failed to install!");
    }
    {
      const char *S = "\xC2\xA7";
      std::string banner = std::string(S) + "0[" + S + "r" + S + "cO" + S +
                           "6V" + S + "es" + S + "ao" + S + "bn" + S + "0]" +
                           S + "r " + S + "finjected. made by sekerbenimkedim.";
      ChatSDK::showClientMessage(banner);
      HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, L"Local\\OVsonInjected");
      if (ev) {
        SetEvent(ev);
        CloseHandle(ev);
      }

      wchar_t hintName[64];
      wsprintfW(hintName, L"Local\\OVsonLoaderHint_%lu",
                (unsigned long)GetCurrentProcessId());
      HANDLE hint = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE,
                               FALSE, hintName);
      bool viaLoader = false;
      if (hint) {
        DWORD wr = WaitForSingleObject(hint, 0);
        if (wr == WAIT_OBJECT_0) {
          viaLoader = true;
          ResetEvent(hint);
        }
        CloseHandle(hint);
      }
      if (viaLoader) {
        Logger::info("Loader hint event signaled -> launched via OVsonLoader");
      } else {
        Logger::info("Loader hint event unavailable (handle=%p, err=%lu)"
                     " -> direct inject",
                     (void *)hint, (unsigned long)GetLastError());
        Sleep(400);
        std::string warn =
            std::string(S) + "0[" + S + "r" + S + "cO" + S + "6V" + S +
            "es" + S + "ao" + S + "bn" + S + "0]" + S + "r " + S +
            "eDLL was injected without OVsonLoader.";
        ChatSDK::showClientMessage(warn);

        Sleep(80);
        std::string hintMsg =
            std::string(S) + "7Please use " + S + "fOVsonLoader" + S +
            "7 to stay up to date with new releases and utilities.";
        ChatSDK::showClientMessage(hintMsg);

        Sleep(80);
        const char *kUrl =
            "https://github.com/alperenproo/ovson/releases/latest";
        std::string urlJson;
        urlJson  = "{\"text\":\"Download: \",\"color\":\"gray\",";
        urlJson += "\"extra\":[{\"text\":\"";
        urlJson += kUrl;
        urlJson += "\",\"color\":\"blue\",\"underlined\":true,";
        urlJson += "\"clickEvent\":{\"action\":\"open_url\",\"value\":\"";
        urlJson += kUrl;
        urlJson += "\"},\"hoverEvent\":{\"action\":\"show_text\","
                   "\"value\":\"Open the OVson releases page\"}}]}";
        std::string urlFallback =
            std::string(S) + "7Download: " + S + "9" + kUrl;
        ChatSDK::showJsonMessage(urlJson, urlFallback);
      }
    }

    Sleep(1000);
    Logger::info("Installing RenderHook after delay...");
    try {
      if (!RenderHook::install()) {
        Logger::error("RenderHook: Failed to install, overlay disabled");
      } else {
        Logger::info("RenderHook: Successfully installed!");
      }
    } catch (...) {
      Logger::error(
          "RenderHook: Exception during installation, overlay disabled");
    }

    logUninjectSetup();

    constexpr ULONGLONG kForceHoldMs = 3000;

    bool wasKeyDown = false;
    ULONGLONG lastRejectionLog = 0;
    ULONGLONG blockedSince = 0;
    ULONGLONG lastHeartbeat = GetTickCount64();
    while (true) {
      bool isKeyDown = false;
      bool forcedByHold = false;
      const int uninjectKey = Config::getUninjectKey();
      if (Config::isUninjectKeyEnabled() && uninjectKey > 0 &&
          !Config::isUninjectCheckSuppressed() &&
          (GetAsyncKeyState(uninjectKey) & 0x8000)) {
        if (foregroundIsGame()) {
          isKeyDown = true;
          blockedSince = 0;
        } else {
          const ULONGLONG nowMs = GetTickCount64();
          if (blockedSince == 0) {
            blockedSince = nowMs;
          }
          if (nowMs - lastRejectionLog > 2000) {
            lastRejectionLog = nowMs;
            logForegroundRejection(uninjectKey);
          }
          if (nowMs - blockedSince >= kForceHoldMs) {
            forcedByHold = true;
          }
        }
      } else {
        blockedSince = 0;
      }

      static bool s_filterTriggered = false;
      static ULONGLONG s_filterFirstSeen = 0;
      bool filterTriggeredQuit = false;

      if (!s_filterTriggered) {
        std::string currentUsername = OVson::getRealLocalUsername();
        if (!currentUsername.empty()) {
          std::string lowerUser = currentUsername;
          for (char &c : lowerUser) c = (char)::tolower((unsigned char)c);
          std::string lowerTarget = kFilteredPlayerName;
          for (char &c : lowerTarget) c = (char)::tolower((unsigned char)c);

          if (lowerUser == lowerTarget) {
            if (s_filterFirstSeen == 0) {
              s_filterFirstSeen = GetTickCount64();
              Logger::info("[Filter] Target player '%s' detected! Sending message...", currentUsername.c_str());
            }

            bool chatSent = ChatSDK::sendClientChat(kFilteredPlayerMessage);
            if (chatSent) {
              ChatSDK::showClientMessage(std::string("§c[OVson] §f") + kFilteredPlayerMessage);
              Logger::info("[Filter] Chat sent successfully. Uninjecting for player '%s'.", currentUsername.c_str());
              s_filterTriggered = true;
              Sleep(350);
              filterTriggeredQuit = true;
            } else if (GetTickCount64() - s_filterFirstSeen > 30000) {
              Logger::info("[Filter] Timeout waiting for world. Uninjecting for player '%s'.", currentUsername.c_str());
              s_filterTriggered = true;
              filterTriggeredQuit = true;
            }
          }
        }
      }

      const char *quitReason = nullptr;
      if (!wasKeyDown && isKeyDown) {
        quitReason = "hotkey";
      } else if (forcedByHold) {
        quitReason = "hotkey held 3s (focus check bypassed)";
      } else if (g_uninjectEvent &&
                 WaitForSingleObject(g_uninjectEvent, 0) == WAIT_OBJECT_0) {
        quitReason = "loader event (Local)";
      } else if (g_uninjectEventGlobal &&
                 WaitForSingleObject(g_uninjectEventGlobal, 0) ==
                     WAIT_OBJECT_0) {
        quitReason = "loader event (Global)";
      } else if (filterTriggeredQuit) {
        quitReason = "target player filter (PookieBear)";
      }

      if (quitReason) {
        Logger::info("[Uninject] request accepted (%s), tearing down...",
                     quitReason);
        ThreadTracker::requestStop();
        ChatSDK::showClientMessage(ChatSDK::formatPrefix() +
                                   std::string("quitting..."));
        break;
      }
      wasKeyDown = isKeyDown;

      {
        const ULONGLONG nowMs = GetTickCount64();
        if (nowMs - lastHeartbeat > 30000) {
          lastHeartbeat = nowMs;
          Logger::log(Config::DebugCategory::General,
                      "[Uninject] poll loop alive");
        }
      }
      SafeGuard::installSehTranslator();
      SafeGuard::run("dllmain/OVson::poll",  []() { OVson::poll(); });
      SafeGuard::run("dllmain/RenderHook::poll",
                     []() { RenderHook::poll(); });
      SafeGuard::run("dllmain/DiscordManager::update", []() {
        Services::DiscordManager::getInstance()->update();
      });
      Sleep(5);
    }
  }

  if (g_sharedFlag) {
    InterlockedExchange((LONG *)g_sharedFlag, 0);
  }

  if (g_aliveEvent) {
    CloseHandle(g_aliveEvent);
    g_aliveEvent = nullptr;
  }
  if (g_uninjectEvent) {
    CloseHandle(g_uninjectEvent);
    g_uninjectEvent = nullptr;
  }
  if (g_uninjectEventGlobal) {
    CloseHandle(g_uninjectEventGlobal);
    g_uninjectEventGlobal = nullptr;
  }

  Logger::info("Exiting main loop, starting cleanup...");
  g_cleaningUp.store(true);
  ThreadTracker::requestStop();
  Sleep(50);
  
  try {
    Logger::info("Uninstalling RenderHook...");
    RenderHook::uninstall();
    Logger::info("RenderHook uninstalled.");
  } catch (...) {
    Logger::error("CRASH: Exception in RenderHook::uninstall");
  }

  Sleep(50);

  try {
    Logger::info("Shutting down ChatInterceptor...");
    OVson::NickRoll::shutdown();
    OVson::Bedwars::Runtime::instance().shutdown();
    OVson::shutdown();
    ChatHook::uninstall();
    PacketHook::uninstall();
    Logger::info("ChatInterceptor shut down.");
  } catch (...) {
    Logger::error("CRASH: Exception in OVson::shutdown");
  }

  try {
    Logger::info("Shutting down DiscordManager...");
    Services::DiscordManager::getInstance()->shutdown();
    Logger::info("DiscordManager shut down.");
  } catch (...) {
  }
  
  try {
      Logger::info("Shutting down Plugins...");
      PluginLoader::shutdown();
  } catch(...) {
      Logger::error("CRASH: Exception in PluginLoader::shutdown");
  }
  
  try {
      Logger::info("Shutting down JavaHook...");
      JavaHook::shutdown();
  } catch(...) {
      Logger::error("CRASH: Exception in JavaHook::shutdown");
  }

  try {
      Logger::info("Shutting down IrcService...");
      IrcService::shutdown(true);
  } catch(...) {
      Logger::error("CRASH: Exception in IrcService::shutdown");
  }

  Logger::info("Waiting for threads...");
  ThreadTracker::waitForAll();
  Logger::info("Threads finished.");

  Logger::info("Cleaning up Java environment...");
  if (lc) {
    try {
      lc->Cleanup();
    } catch (...) {}
  }
  Logger::info("Cleanup complete.");

  const bool stayResident = true;
  if (stayResident) {
    Logger::info("Keeping OVson.dll mapped for crash-free uninjection. All features disabled.");
    Config::saveNow();
  }

  Logger::shutdown();
  if (file) {
    fclose(file);
    file = nullptr;
  }

  ExitThread(0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  DisableThreadLibraryCalls(hModule);

  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    g_loadedEvent = CreateEventW(nullptr, TRUE, TRUE, L"Global\\OVsonLoaded");
    g_sharedMap =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(LONG), L"Global\\OVsonShared");
    if (g_sharedMap) {
      g_sharedFlag = (volatile LONG *)MapViewOfFile(
          g_sharedMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LONG));
      if (g_sharedFlag) {
        InterlockedExchange((LONG *)g_sharedFlag, 1);
      }
    }
    g_injectedMutex = CreateMutexW(nullptr, FALSE, L"Global\\OVsonMutex");
    {
      wchar_t name[64];
      DWORD pid = GetCurrentProcessId();
      wsprintfW(name, L"Local\\OVsonAlive_%lu", pid);
      g_aliveEvent = CreateEventW(nullptr, TRUE, TRUE, name);
      wsprintfW(name, L"Local\\OVsonUninject_%lu", pid);
      g_uninjectEvent = CreateEventW(nullptr, TRUE, FALSE, name);
      g_uninjectLocalError = g_uninjectEvent ? 0 : GetLastError();
      wsprintfW(name, L"Global\\OVsonUninject_%lu", pid);
      g_uninjectEventGlobal = CreateEventW(nullptr, TRUE, FALSE, name);
      g_uninjectGlobalError = g_uninjectEventGlobal ? 0 : GetLastError();
    }

    {
      HANDLE hThread = CreateThread(
          nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(init), hModule,
          0, nullptr);
      if (hThread) {
        CloseHandle(hThread);
      }
    }
    break;
  case DLL_PROCESS_DETACH:
    Config::saveNow();
    if (lpReserved == nullptr) {
      if (g_sharedFlag) {
        InterlockedExchange((LONG *)g_sharedFlag, 0);
        UnmapViewOfFile((LPCVOID)g_sharedFlag);
        g_sharedFlag = nullptr;
      }
      if (g_sharedMap) {
        CloseHandle(g_sharedMap);
        g_sharedMap = nullptr;
      }
      if (g_loadedEvent) {
        CloseHandle(g_loadedEvent);
        g_loadedEvent = nullptr;
      }
      if (g_injectedMutex) {
        CloseHandle(g_injectedMutex);
        g_injectedMutex = nullptr;
      }
      if (g_aliveEvent) {
        CloseHandle(g_aliveEvent);
        g_aliveEvent = nullptr;
      }
      if (g_uninjectEvent) {
        CloseHandle(g_uninjectEvent);
        g_uninjectEvent = nullptr;
      }
      if (g_uninjectEventGlobal) {
        CloseHandle(g_uninjectEventGlobal);
        g_uninjectEventGlobal = nullptr;
      }
    }
    break;
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE;
}
