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
#include "Services/DiscordManager.h"
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
    RegisterDefaultCommands();
    OVson::initialize();
    Anticheat::initialize();
    Logger::info("ChatHook disabled (safe mode)");

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

    bool wasEndDown = false;
    while (true) {
      bool isEndDown = false;
      if (GetAsyncKeyState(VK_END) & 0x8000) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        if (pid == GetCurrentProcessId()) {
          isEndDown = true;
        }
      }
      bool shouldQuit = (!wasEndDown && isEndDown);
      if (!shouldQuit && g_uninjectEvent) {
        if (WaitForSingleObject(g_uninjectEvent, 0) == WAIT_OBJECT_0) {
          shouldQuit = true;
        }
      }
      if (shouldQuit) {
        ThreadTracker::requestStop();
        ChatSDK::showClientMessage(ChatSDK::formatPrefix() +
                                   std::string("quitting..."));
        break;
      }
      wasEndDown = isEndDown;
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

  Logger::info("Exiting main loop, starting cleanup...");
  g_cleaningUp.store(true);
  Sleep(50);


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
    OVson::shutdown();
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

  Logger::shutdown();
  if (file) {
    fclose(file);
    file = nullptr;
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
    }
    break;
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE;
}
