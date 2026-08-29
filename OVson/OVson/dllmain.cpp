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
// Some setups run the loader and the game in different Terminal Services
// sessions (or the game elevated), and then a "Local\" name created here is
// simply invisible to the loader. Publishing the same event under "Global\"
// as well costs one handle and removes a whole class of "uninject button does
// nothing" reports. The loader already probes both prefixes.
static HANDLE g_uninjectEventGlobal = nullptr;
static DWORD g_uninjectLocalError = 0;
static DWORD g_uninjectGlobalError = 0;

namespace {

// GetForegroundWindow()'s process is NOT reliably ours. Badlion (and any
// launcher that hosts the Minecraft canvas inside its own frame) owns the
// top-level window from a different process, so the plain PID comparison that
// works on Lunar rejects every key press there and the hotkey looks dead.
// Accept the key when the foreground window is anywhere in the same window
// tree as the surface RenderHook actually subclassed.
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

    // Holding the key this long forces an uninject even if we could not
    // prove the game is focused. A tap is what normally triggers it; nobody
    // holds End down for three seconds by accident, and this guarantees
    // there is always a way out when window ownership is unusual.
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
          (GetAsyncKeyState(uninjectKey) & 0x8000)) {
        if (foregroundIsGame()) {
          isKeyDown = true;
          blockedSince = 0;
        } else {
          const ULONGLONG nowMs = GetTickCount64();
          if (blockedSince == 0) {
            blockedSince = nowMs;
          }
          // Throttled: this is the single most useful line when someone
          // reports that the hotkey does nothing on their client.
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
      }

      if (quitReason) {
        // Logged before anything else runs so that a teardown that hangs
        // later still leaves proof that the request was received.
        Logger::info("[Uninject] request accepted (%s), tearing down...",
                     quitReason);
        ThreadTracker::requestStop();
        ChatSDK::showClientMessage(ChatSDK::formatPrefix() +
                                   std::string("quitting..."));
        break;
      }
      wasKeyDown = isKeyDown;

      // The Bedwars/render logging all comes from the render thread, so
      // without this there is no way to tell from a log whether this loop is
      // alive or wedged inside one of the polls below.
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
  
  try {
    Logger::info("Uninstalling RenderHook...");
    RenderHook::uninstall();
    Logger::info("RenderHook uninstalled.");
  } catch (...) {
    Logger::error("CRASH: Exception in RenderHook::uninstall");
  }

  Sleep(100);

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

  g_cleaningUp.store(true);
  Sleep(50);

  Logger::info("Waiting for threads...");
  ThreadTracker::waitForAll();
  Logger::info("Threads finished.");

  Logger::info("Cleaning up Java environment...");
  if (lc) {
    JavaVM *vm = lc->vm;
    lc->Cleanup();
    if (vm) {
      vm->DetachCurrentThread();
    }
  }
  Logger::info("Cleanup complete.");

  // People play with several DLLs injected at once. If RenderHook had to
  // leave one of our hooks installed because someone else chained onto it,
  // their code path still runs through ours -- unmapping this module would
  // send them into unmapped memory the next frame or the next keystroke.
  // Staying resident costs a few hundred KB until the game closes; that is
  // strictly better than taking another mod (or the process) down with us.
  const bool stayResident = RenderHook::mustStayLoaded();
  if (stayResident) {
    Logger::info("Keeping OVson.dll mapped: another injected module is "
                 "chained to our hooks. It will be released when the game "
                 "closes.");
    // DLL_PROCESS_DETACH is what normally persists the config, and it will
    // not run for us now (the process-exit detach passes lpReserved != null
    // and skips it), so save here instead.
    Config::saveNow();
  }

  Logger::shutdown();
  if (file) {
    fclose(file);
    file = nullptr;
  }

  if (stayResident) {
    ExitThread(0);
  }

  FreeLibraryAndExitThread(static_cast<HMODULE>(instance), 0);
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
