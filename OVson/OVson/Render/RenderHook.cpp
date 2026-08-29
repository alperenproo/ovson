#include "RenderHook.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../SDK/McAccess.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Logic/PacketHook.h"
#include "../Logic/StatsTracker.h"
#include "../Utils/Anticheat/AcInternal.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/Logger.h"
#include "../Utils/ReplaySpammer.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/SensitivityFix.h"
#include "../Utils/Watchdog.h"
#include "BetterTab.h"
#include "../ClickGUI/ClickGUI.h"
#include "BedwarsOverlay.h"
#include "MediaOverlay.h"
#include "../Logic/MediaSession.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"
#include "../Logic/NickRoll/NickRollRuntime.h"
#include "DefenseRenderer.h"
#include "NameTagRenderer.h"
#include "NotificationManager.h"
#include "StatsOverlay.h"
#include "TechOverlay.h"
#include "Shader.h"
#include <cstring>
#include <fstream>
#include <functional>
#include "GL.h"
#include <mutex>
#include <windows.h>
#include "../Utils/Timer.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

static std::ofstream g_debugLog;
static bool g_hookInstalled = false;

static std::queue<std::function<void()>> g_taskQueue;
static std::mutex g_queueMutex;
static float g_frameDelta = 0.0f;
static std::atomic<bool> g_unloading{false};
static std::atomic<int> g_threadsInHook{0};
static std::atomic<bool> g_tabDown{false};

template <class Fn>
static inline void runSubsystem(const char *site, Fn &&body) {
  Watchdog::SubsystemScope _wd(site);
  SafeGuard::run(site, std::forward<Fn>(body));
}

static int g_tabVK = VK_TAB;
static int g_tabScan = 0x0F;
static jobject g_tabKbGlobal = nullptr;
static jfieldID g_tabPressedFid = nullptr;
static std::atomic<bool> g_suppressVanillaTab{false};

static void suppressVanillaTab() {
  if (!g_tabKbGlobal || !g_tabPressedFid) return;
  JNIEnv *env = lc ? lc->getEnv() : nullptr;
  if (!env) return;
  env->SetBooleanField(g_tabKbGlobal, g_tabPressedFid, JNI_FALSE);
  if (env->ExceptionCheck()) env->ExceptionClear();
}

static jfieldID g_tabKeyCodeFid = nullptr;

static int getPlayerListVK() {
  static bool s_logged = false;
  static ULONGLONG s_last = 0;
  
  JNIEnv *env = lc ? lc->getEnv() : nullptr;
  if (!env) return g_tabVK;

  if (g_tabKbGlobal && g_tabKeyCodeFid) {
    jint lwjglCode = env->GetIntField(g_tabKbGlobal, g_tabKeyCodeFid);
    if (env->ExceptionCheck()) env->ExceptionClear();
    else if (lwjglCode > 0 && lwjglCode < 256) {
      UINT mapped = MapVirtualKeyA((UINT)lwjglCode, MAPVK_VSC_TO_VK);
      if (mapped != 0) {
        g_tabScan = lwjglCode;
        g_tabVK = (int)mapped;
        return g_tabVK;
      }
    }
  }

  ULONGLONG now = GetTickCount64();
  if (now - s_last < 3000) return g_tabVK;
  s_last = now;

  #define TABLOG(...) do { if (!s_logged) Logger::info(__VA_ARGS__); } while(0)

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) { TABLOG("[TabKey] Minecraft class NOT FOUND -> fallback"); return g_tabVK; }
  jfieldID f_theMc = lc->GetStaticFieldID(
      mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;",
      "field_71432_P", "S", "Lave;");
  if (!f_theMc) { if (env->ExceptionCheck()) env->ExceptionClear(); TABLOG("[TabKey] theMinecraft field NOT FOUND -> fallback"); return g_tabVK; }
  jobject mc = env->GetStaticObjectField(mcCls, f_theMc);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!mc) { TABLOG("[TabKey] theMinecraft instance null -> fallback"); return g_tabVK; }

  jfieldID f_gs = lc->GetFieldID(
      mcCls, "gameSettings",
      "Lnet/minecraft/client/settings/GameSettings;", "field_71474_y", "t", "Lavh;");
  if (!f_gs) { if (env->ExceptionCheck()) env->ExceptionClear();
    f_gs = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/settings/GameSettings;"); }
  if (!f_gs) { if (env->ExceptionCheck()) env->ExceptionClear();
    f_gs = lc->FindFieldBySignature(mcCls, "Lavh;"); }  // 1.8.9 notch type
  if (!f_gs) { env->DeleteLocalRef(mc); if (env->ExceptionCheck()) env->ExceptionClear(); TABLOG("[TabKey] gameSettings field NOT FOUND (name+sig+notch) -> fallback"); return g_tabVK; }
  jobject gs = env->GetObjectField(mc, f_gs);
  env->DeleteLocalRef(mc);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!gs) { TABLOG("[TabKey] gameSettings instance null -> fallback"); return g_tabVK; }

  jclass gsCls = env->GetObjectClass(gs);
  jobject kb = nullptr;
  jfieldID f_kb = lc->GetFieldID(
      gsCls, "keyBindPlayerList",
      "Lnet/minecraft/client/settings/KeyBinding;", "field_74321_H", "", "");
  if (f_kb) { kb = env->GetObjectField(gs, f_kb); if (env->ExceptionCheck()) env->ExceptionClear(); }
  if (!kb) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    jint fc = 0; jfieldID *pf = nullptr;
    if (lc->jvmti && lc->jvmti->GetClassFields(gsCls, &fc, &pf) == JVMTI_ERROR_NONE) {
      for (jint fi = 0; fi < fc && !kb; ++fi) {
        char *fn = nullptr, *fs = nullptr;
        if (lc->jvmti->GetFieldName(gsCls, pf[fi], &fn, &fs, nullptr) != JVMTI_ERROR_NONE)
          continue;
        std::string sig = fs ? fs : "";
        bool isKbArr = sig.find("KeyBinding;") != std::string::npos || sig == "[Lavb;";
        if (isKbArr) {
          jobjectArray arr = (jobjectArray)env->GetObjectField(gs, pf[fi]);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (arr) {
            jsize cnt = env->GetArrayLength(arr);
            TABLOG("[TabKey] KB[] sig=%s len=%d", sig.c_str(), (int)cnt);
            for (jsize i = 0; i < cnt && !kb; ++i) {
              jobject k = env->GetObjectArrayElement(arr, i);
              if (!k) continue;
              jclass kc = env->GetObjectClass(k);
              jfieldID f_desc = lc->FindFieldBySignature(kc, "Ljava/lang/String;");
              env->DeleteLocalRef(kc);
              bool match = false;
              if (f_desc) {
                jstring jd = (jstring)env->GetObjectField(k, f_desc);
                if (jd) {
                  const char *d = env->GetStringUTFChars(jd, nullptr);
                  match = d && std::string(d) == "key.playerlist";
                  if (d) env->ReleaseStringUTFChars(jd, d);
                  env->DeleteLocalRef(jd);
                }
              }
              if (match) kb = k;
              else env->DeleteLocalRef(k);
            }
            env->DeleteLocalRef(arr);
          }
        }
        if (fn) lc->jvmti->Deallocate((unsigned char *)fn);
        if (fs) lc->jvmti->Deallocate((unsigned char *)fs);
      }
      if (pf) lc->jvmti->Deallocate((unsigned char *)pf);
    }
  }
  env->DeleteLocalRef(gsCls);
  env->DeleteLocalRef(gs);
  if (!kb) { if (env->ExceptionCheck()) env->ExceptionClear(); TABLOG("[TabKey] keyBindPlayerList NOT FOUND (name+srg+array-desc) -> fallback"); return g_tabVK; }

  jclass kbCls = env->GetObjectClass(kb);
  jint lwjglCode = 0;
  bool gotCode = false;
  jfieldID foundKeyCodeFid = nullptr;
  {
    jint fc2 = 0; jfieldID *pf2 = nullptr;
    if (lc->jvmti && lc->jvmti->GetClassFields(kbCls, &fc2, &pf2) == JVMTI_ERROR_NONE) {
      int intSeen = 0;
      for (jint fi = 0; fi < fc2; ++fi) {
        char *fn = nullptr, *fs = nullptr;
        if (lc->jvmti->GetFieldName(kbCls, pf2[fi], &fn, &fs, nullptr) != JVMTI_ERROR_NONE)
          continue;
        if (fs && std::string(fs) == "I") {
          ++intSeen;
          jint v = env->GetIntField(kb, pf2[fi]);
          if (env->ExceptionCheck()) env->ExceptionClear();
          TABLOG("[TabKey]   KeyBinding int#%d (%s) = %d",
                       intSeen, fn ? fn : "?", (int)v);
          if (intSeen == 2) { lwjglCode = v; gotCode = true; foundKeyCodeFid = pf2[fi]; }
        }
        if (fn) lc->jvmti->Deallocate((unsigned char *)fn);
        if (fs) lc->jvmti->Deallocate((unsigned char *)fs);
      }
      if (pf2) lc->jvmti->Deallocate((unsigned char *)pf2);
    }
  }
  if (!gotCode) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    jmethodID m_code = lc->GetMethodID(kbCls, "getKeyCode", "()I", "func_151463_i", "", "");
    if (m_code) { lwjglCode = env->CallIntMethod(kb, m_code); if (env->ExceptionCheck()) env->ExceptionClear(); else gotCode = true; }
  }
  jfieldID f_pressed = lc->FindFieldBySignature(kbCls, "Z");
  if (f_pressed) {
    if (g_tabKbGlobal) env->DeleteGlobalRef(g_tabKbGlobal);
    g_tabKbGlobal = env->NewGlobalRef(kb);
    g_tabPressedFid = f_pressed;
    g_tabKeyCodeFid = foundKeyCodeFid;
  }
  env->DeleteLocalRef(kbCls);
  env->DeleteLocalRef(kb);
  if (!gotCode) { TABLOG("[TabKey] keyCode read FAILED -> fallback"); return g_tabVK; }

  if (lwjglCode > 0 && lwjglCode < 256) {
    UINT mapped = MapVirtualKeyA((UINT)lwjglCode, MAPVK_VSC_TO_VK);
    if (mapped != 0) {
      g_tabScan = lwjglCode;
      g_tabVK = (int)mapped;
    }
    TABLOG("[TabKey] READ OK lwjglCode=%d -> scan=0x%X vk=0x%X (mapped=%u)",
                 (int)lwjglCode, g_tabScan, g_tabVK, mapped);
  } else {
    TABLOG("[TabKey] keyCode=%d OUT OF RANGE (wrong field/mapping) -> fallback VK=0x%X",
                 (int)lwjglCode, g_tabVK);
  }
  
  s_logged = true;
  #undef TABLOG
  return g_tabVK;
}

class FlagReleaser {
public:
  explicit FlagReleaser(bool *flag) : m_flag(flag) {}
  ~FlagReleaser() { if (m_flag) *m_flag = false; }
  FlagReleaser(const FlagReleaser &) = delete;
  FlagReleaser &operator=(const FlagReleaser &) = delete;

private:
  bool *m_flag;
};

struct HookGuard {
  HookGuard()  { g_threadsInHook.fetch_add(1, std::memory_order_acq_rel); }
  ~HookGuard() { g_threadsInHook.fetch_sub(1, std::memory_order_acq_rel); }
  HookGuard(const HookGuard &) = delete;
  HookGuard &operator=(const HookGuard &) = delete;
};

typedef void(__stdcall *PFNGLUSEPROGRAMPROC_LOCAL)(unsigned int);
static PFNGLUSEPROGRAMPROC_LOCAL g_glUseProgram = nullptr;

static HMODULE g_MinHookModule = nullptr;

typedef enum {
  MH_OK = 0,
  MH_ERROR_ALREADY_INITIALIZED = 1,
  MH_ERROR_NOT_INITIALIZED = 2,
  MH_ERROR_ALREADY_CREATED = 3,
  MH_ERROR_NOT_CREATED = 4,
  MH_ERROR_ENABLED = 5,
  MH_ERROR_DISABLED = 6,
} MH_STATUS_LOCAL;

#define MH_ALL_HOOKS NULL

typedef MH_STATUS_LOCAL(WINAPI *MH_Initialize_t)(VOID);
typedef MH_STATUS_LOCAL(WINAPI *MH_Uninitialize_t)(VOID);
typedef MH_STATUS_LOCAL(WINAPI *MH_CreateHook_t)(LPVOID, LPVOID, LPVOID *);
typedef MH_STATUS_LOCAL(WINAPI *MH_EnableHook_t)(LPVOID);
typedef MH_STATUS_LOCAL(WINAPI *MH_DisableHook_t)(LPVOID);
typedef const char *(WINAPI *MH_StatusToString_t)(MH_STATUS_LOCAL);

static MH_Initialize_t pMH_Initialize = nullptr;
static MH_Uninitialize_t pMH_Uninitialize = nullptr;
static MH_CreateHook_t pMH_CreateHook = nullptr;
static MH_EnableHook_t pMH_EnableHook = nullptr;
static MH_DisableHook_t pMH_DisableHook = nullptr;
static MH_StatusToString_t pMH_StatusToString = nullptr;

typedef BOOL(WINAPI *wglSwapBuffers_t)(HDC hdc);
static wglSwapBuffers_t originalSwapBuffers = nullptr;

typedef BOOL(WINAPI* SetCursorPos_t)(int, int);
static SetCursorPos_t originalSetCursorPos = nullptr;

// --- Cross-mod safe teardown -------------------------------------------
// Minecraft is routinely running more than one injected DLL at a time, and
// hooks stack: whoever installs last sits in front of whoever installed
// first. That makes teardown order-dependent, so every step below has to
// answer one question before it undoes anything: "is the thing I am about
// to restore still mine?"
//
// If another module hooked wglSwapBuffers after us, its trampoline holds a
// copy of *our* jump, so it calls into our stub. MinHook's disable path
// restores the function's ORIGINAL prologue bytes, which silently deletes
// that module's jump, and MH_Uninitialize + FreeLibrary then frees the
// trampoline its chain still points at. Result: the other mod stops
// working, or the game dies in unmapped memory. The WndProc chain has the
// exact same shape.
//
// When we detect that we are no longer the outermost hook, we leave
// everything installed and leak instead. A resident module costs a few
// hundred KB until the game closes; a wrong unhook costs the other mod and
// usually the whole process.
struct HookedTarget {
  LPVOID address = nullptr;
  unsigned char prologue[16] = {};
  bool valid = false;
};

static HookedTarget g_swapTarget;
static HookedTarget g_cursorTarget;
static std::atomic<bool> g_mustStayLoaded{false};

// Record what the function's first bytes look like *after* MinHook has
// installed our jump, so we have something to compare against later.
static void snapshotPrologue(HookedTarget &target, LPVOID address) {
  if (!address) return;
  target.address = address;
  memcpy(target.prologue, address, sizeof(target.prologue));
  target.valid = true;
}

// True while the hooked function still starts with exactly the bytes
// MinHook left there for us. Anything else means a later injection
// overwrote the prologue with its own jump and is now chained in front of
// us. An unhooked/never-snapshotted target counts as ours: there is
// nothing of anyone else's to damage.
static bool prologueStillOurs(const HookedTarget &target) {
  if (!target.valid || !target.address) return true;
  return memcmp(target.address, target.prologue, sizeof(target.prologue)) == 0;
}

BOOL WINAPI hookedSetCursorPos(int X, int Y) {
  if (Config::isRawMouseFixEnabled()) {
    POINT pt;
    if (GetCursorPos(&pt) && pt.x == X && pt.y == Y) {
      return TRUE; // Bypass redundant SetCursorPos calls
    }
  }
  return originalSetCursorPos(X, Y);
}

static void writeDebugLog(const char *msg) {
  (void)msg;
  /* if (!g_debugLog.is_open()) {
     char path[MAX_PATH];
     GetModuleFileNameA(NULL, path, MAX_PATH);
     std::string exePath(path);
     size_t lastSlash = exePath.find_last_of("\\/");
     std::string dir = exePath.substr(0, lastSlash);
     std::string logPath = dir + "\\ovson_render_debug.txt";
     g_debugLog.open(logPath, std::ios::app);
   }
   if (g_debugLog.is_open()) {
     SYSTEMTIME st;
     GetLocalTime(&st);
     char timeStr[64];
     sprintf_s(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
     g_debugLog << timeStr << msg << std::endl;
     g_debugLog.flush();
   }*/
}

HMODULE GetCurrentModule() {
  HMODULE hModule = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCTSTR)GetCurrentModule, &hModule);
  return hModule;
}
// load minhook
bool LoadMinHook() {
  writeDebugLog("Loading MinHook from embedded resources...");

  HMODULE hModule = GetCurrentModule();
  if (!hModule) {
    writeDebugLog("ERROR: Failed to get current module handle");
    return false;
  }

  char buf[256];
  sprintf_s(buf, "Current module handle: 0x%p", hModule);
  writeDebugLog(buf);

  HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(101), "MINHOOK_DLL");
  if (!hRes) {
    DWORD err = GetLastError();
    sprintf_s(buf,
              "ERROR: MinHook resource not found (ID 101, Type MINHOOK_DLL). "
              "Error: %lu",
              err);
    writeDebugLog(buf);
    return false;
  }

  HGLOBAL hResData = LoadResource(hModule, hRes);
  if (!hResData) {
    writeDebugLog("ERROR: Failed to load MinHook resource");
    return false;
  }

  LPVOID pResData = LockResource(hResData);
  DWORD resSize = SizeofResource(hModule, hRes);

  if (!pResData || resSize == 0) {
    writeDebugLog("ERROR: Invalid resource data");
    return false;
  }

  sprintf_s(buf, "MinHook resource found, size: %lu bytes", resSize);
  writeDebugLog(buf);

  char tempPath[MAX_PATH];
  GetTempPathA(MAX_PATH, tempPath);
  std::string dllPath = std::string(tempPath) + "MinHook_" +
                        std::to_string(GetTickCount64()) + ".dll";

  HANDLE hFile = CreateFileA(dllPath.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    writeDebugLog("ERROR: Failed to create temp file");
    return false;
  }

  DWORD written;
  if (!WriteFile(hFile, pResData, resSize, &written, NULL) ||
      written != resSize) {
    CloseHandle(hFile);
    writeDebugLog("ERROR: Failed to write DLL to temp");
    return false;
  }
  CloseHandle(hFile);

  sprintf_s(buf, "MinHook extracted to: %s", dllPath.c_str());
  writeDebugLog(buf);

  g_MinHookModule = LoadLibraryA(dllPath.c_str());
  if (!g_MinHookModule) {
    DWORD err = GetLastError();
    sprintf_s(buf, "ERROR: Failed to load MinHook DLL. Error: %lu", err);
    writeDebugLog(buf);
    return false;
  }
  writeDebugLog("MinHook.x64.dll loaded successfully");

  pMH_Initialize =
      (MH_Initialize_t)GetProcAddress(g_MinHookModule, "MH_Initialize");
  pMH_Uninitialize =
      (MH_Uninitialize_t)GetProcAddress(g_MinHookModule, "MH_Uninitialize");
  pMH_CreateHook =
      (MH_CreateHook_t)GetProcAddress(g_MinHookModule, "MH_CreateHook");
  pMH_EnableHook =
      (MH_EnableHook_t)GetProcAddress(g_MinHookModule, "MH_EnableHook");
  pMH_DisableHook =
      (MH_DisableHook_t)GetProcAddress(g_MinHookModule, "MH_DisableHook");
  pMH_StatusToString =
      (MH_StatusToString_t)GetProcAddress(g_MinHookModule, "MH_StatusToString");

  if (!pMH_Initialize || !pMH_CreateHook || !pMH_EnableHook) {
    writeDebugLog("ERROR: Failed to load MinHook functions");
    FreeLibrary(g_MinHookModule);
    g_MinHookModule = nullptr;
    return false;
  }

  writeDebugLog("All MinHook functions loaded");
  return true;
}

static WNDPROC originalWndProc = nullptr;
static HWND g_gameHwnd = nullptr;

LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam) {
  HookGuard guard;
  if (g_unloading) {
    return CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
  }

  static thread_local bool s_sehInstalled = false;
  if (!s_sehInstalled) {
    SafeGuard::installSehTranslator();
    s_sehInstalled = true;
  }

  bool consume = false;
  SafeGuard::run("hookedWndProc/decision", [&]() {
    if (BetterTab::isResizeMode()) {
      if (uMsg == WM_LBUTTONDOWN) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        BetterTab::handleMouseClick(0, 1, pt.x, pt.y);
      } else if (uMsg == WM_LBUTTONUP) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        BetterTab::handleMouseClick(0, 0, pt.x, pt.y);
      } else if (uMsg == WM_MOUSEMOVE) {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        BetterTab::handleMouseMove(pt.x, pt.y);
      } else if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        BetterTab::setResizeMode(false);
      }
      
      switch (uMsg) {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
      case WM_MOUSEMOVE:
      case WM_MOUSEWHEEL:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_CHAR:
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
        consume = true;
        return;
      }
    }

    if (Render::ClickGUI::isOpen()) {
      Render::ClickGUI::handleMessage(uMsg, wParam, lParam);
      switch (uMsg) {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      case WM_RBUTTONDOWN:
      case WM_RBUTTONUP:
      case WM_MBUTTONDOWN:
      case WM_MBUTTONUP:
      case WM_MOUSEMOVE:
      case WM_MOUSEWHEEL:
      case WM_KEYDOWN:
      case WM_KEYUP:
      case WM_CHAR:
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
        consume = true;
        return;
      }
    }

    if (uMsg == WM_KEYDOWN && (int)wParam == g_tabVK) {
      // The replay case has to be here as well, and it was the one place it
      // was missing. Three gates decide BetterTab: this one swallows the Tab
      // key so Minecraft never sees it, the render gate below, and
      // BetterTab.cpp's own activeMatch check. The other two read
      // "isInHypixelGame() || isInReplay()"; this one read only the first.
      //
      // A replay is not an "in Hypixel game" session -- it is detected from a
      // scoreboard titled REPLAY, and g_inHypixelGame is driven by game
      // keywords that are absent there. So in a replay BetterTab drew itself
      // while the key still reached the game, and the vanilla player list drew
      // underneath it. Both lists on screen at once.
      //
      // suppressVanillaTab() clearing keyBindPlayerList.pressed does not save
      // it: that is a race against Minecraft's own input polling, which keeps
      // setting the field back to true from a key event that should never have
      // arrived. Not letting the key through is the actual fix.
      if (Config::isBetterTabModeEnabled() &&
          (OVson::isInHypixelGame() || OVson::isInReplay()) &&
          !OVson::isInPreGameLobby() && !OVson::isChatOpen()) {
        consume = true;
        return;
      }
    }

    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
      if ((lParam & 0x40000000) == 0) {
        if (OVson::handleEnterKeyPress()) {
          consume = true;
          return;
        }
      }
    }
  });

  if (consume) {
    return 0;
  }

  return CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
}

static void renderOverlayWorkBody(HDC hdc) {
  static int frameCount = 0;
  frameCount++;

  g_frameDelta = TimeUtil::getDelta();

  if (g_glUseProgram)
    g_glUseProgram(0);

  HWND currentHwnd = WindowFromDC(hdc);
  if (currentHwnd && IsWindow(currentHwnd) && currentHwnd != g_gameHwnd) {
    if (g_gameHwnd && IsWindow(g_gameHwnd) && originalWndProc) {
      // Only unhook the old window if we are still its current WndProc.
      // If another mod subclassed after us, the window's proc is theirs
      // and their saved "previous" pointer is our hookedWndProc --
      // writing originalWndProc back here would drop them out of the
      // chain and kill their input handling.
      if (GetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC) ==
          (LONG_PTR)hookedWndProc) {
        SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
      }
    }
    g_gameHwnd = currentHwnd;
    originalWndProc = (WNDPROC)SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC,
                                                (LONG_PTR)hookedWndProc);
    writeDebugLog("WndProc hooked (re-hook on HWND change)");
  } else if (!g_gameHwnd && currentHwnd && IsWindow(currentHwnd)) {
    g_gameHwnd = currentHwnd;
    originalWndProc = (WNDPROC)SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC,
                                                (LONG_PTR)hookedWndProc);
    writeDebugLog("WndProc hooked successfully!");
  }

  if (frameCount % 100 == 0) {
    char buf[128];
    sprintf_s(buf, "Rendered %d frames", frameCount);
    writeDebugLog(buf);
  }

  static Shader g_vectorBlurShader;
  static bool g_vectorBlurInit = false;
  if (!g_vectorBlurInit) {
    const char* const VECTOR_BLUR_FRAGMENT_SHADER = 
#include "../ClickGUI/Shaders/vector_blur_frag.glsl"
    ;
    const char* const VECTOR_BLUR_VERTEX_SHADER = 
#include "../ClickGUI/Shaders/vector_blur_vert.glsl"
    ;
    if (g_vectorBlurShader.compile(VECTOR_BLUR_VERTEX_SHADER, VECTOR_BLUR_FRAGMENT_SHADER)) {
      writeDebugLog("Vector blur shader compiled successfully");
    } else {
      writeDebugLog("Failed to compile vector blur shader");
    }
    g_vectorBlurInit = true;
  }


  if (Config::isMotionBlurEnabled() && !OVson::isChatOpen() && g_vectorBlurShader.getProgramId() != 0) {
    float amount = Config::getMotionBlurAmount(); // 0.0 to 1.0
    if (amount > 0.01f) {
      static float lastYaw = 0.0f;
      static float lastPitch = 0.0f;
      float currentYaw = 0.0f;
      float currentPitch = 0.0f;
      
      bool hasRot = false;
      SafeGuard::run("VectorBlurRot", [&]() {
        if (lc) {
          JNIEnv* env = lc->getEnv();
          if (env) {
            jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
            if (mcCls) {
              jobject mc = lc->GetStaticObjectField(mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
              if (mc) {
                jobject currentScreen = lc->GetObjectField(mc, "currentScreen", "Lnet/minecraft/client/gui/GuiScreen;", "field_71462_r", "m", "Laxu;");
                if (currentScreen) {
                  env->DeleteLocalRef(currentScreen);
                  env->DeleteLocalRef(mc);
                  return;
                }
                jobject player = lc->GetObjectField(mc, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h", "Lbew;");
                if (player) {
                  jclass entityCls = lc->GetClass("net.minecraft.entity.Entity");
                  if (entityCls) {
                    jfieldID yawFid = lc->GetFieldID(entityCls, "rotationYaw", "F", "field_70177_z", "y", "F");
                    if (yawFid) currentYaw = env->GetFloatField(player, yawFid);
                    
                    jfieldID pitchFid = lc->GetFieldID(entityCls, "rotationPitch", "F", "field_70125_A", "z", "F");
                    if (pitchFid) currentPitch = env->GetFloatField(player, pitchFid);
                    
                    hasRot = true;
                  }
                  env->DeleteLocalRef(player);
                }
                env->DeleteLocalRef(mc);
              }
            }
          }
        }
      });
      

      
      if (hasRot) {
        float deltaYaw = currentYaw - lastYaw;
        float deltaPitch = currentPitch - lastPitch;
        
        while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
        while (deltaYaw < -180.0f) deltaYaw += 360.0f;
        
        lastYaw = currentYaw;
        lastPitch = currentPitch;
        
        if (std::abs(deltaYaw) > 0.05f || std::abs(deltaPitch) > 0.05f) {
          GLint viewport[4];
          glGetIntegerv(GL_VIEWPORT, viewport);
          int sw = viewport[2];
          int sh = viewport[3];

          static GLuint g_blurTex = 0;
          static int    g_blurTexW = 0;
          static int    g_blurTexH = 0;

          if (sw > 0 && sh > 0) {
            if (!g_blurTex || g_blurTexW != sw || g_blurTexH != sh) {
              if (g_blurTex) glDeleteTextures(1, &g_blurTex);
              glGenTextures(1, &g_blurTex);
              glBindTexture(GL_TEXTURE_2D, g_blurTex);
              glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sw, sh, 0,
                           GL_RGB, GL_UNSIGNED_BYTE, nullptr);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
              glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
              g_blurTexW = sw;
              g_blurTexH = sh;
            }

            glBindTexture(GL_TEXTURE_2D, g_blurTex);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sw, sh);

            glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT |
                         GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT |
                         GL_TRANSFORM_BIT);
            glDisable(GL_LIGHTING);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glDisable(GL_FOG);
            glDisable(GL_BLEND);

            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, sw, 0, sh, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, g_blurTex);

            g_vectorBlurShader.use();
            g_vectorBlurShader.setUniform1i("u_texture", 0);
            
            float velX = -(deltaYaw / 90.0f) * amount * 2.5f;
            float velY = (deltaPitch / 90.0f) * amount * 2.5f;
            
            g_vectorBlurShader.setUniform2f("u_velocity", velX, velY);
            
            glColor4f(1, 1, 1, 1);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(0,  0);
            glTexCoord2f(1, 0); glVertex2f((float)sw, 0);
            glTexCoord2f(1, 1); glVertex2f((float)sw, (float)sh);
            glTexCoord2f(0, 1); glVertex2f(0, (float)sh);
            glEnd();
            
            g_vectorBlurShader.unuse();

            glBindTexture(GL_TEXTURE_2D, 0);
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glDepthMask(GL_TRUE);
            glPopAttrib();
          }
        }
      }
    }
  }

  runSubsystem("StatsOverlay::render", [hdc]() {
    StatsOverlay::render((void *)hdc);
  });

  runSubsystem("DefenseRenderer::render", [hdc]() {
    BedDefense::DefenseRenderer::getInstance()->render((void *)hdc, 0.0);
  });

  runSubsystem("NameTagRenderer::render", [hdc]() {
    OVson::NameTagRenderer::getInstance()->render((void *)hdc, 0.0);
  });

  if (Config::isNotificationsEnabled()) {
    runSubsystem("NotificationManager::render", [hdc]() {
      Render::NotificationManager::getInstance()->render(hdc);
    });
  }

  if (Config::isTechEnabled()) {
    runSubsystem("TechOverlay::render", [hdc]() {
      GLint vp[4];
      glGetIntegerv(GL_VIEWPORT, vp);
      Render::TechOverlay::render(hdc, vp[2], vp[3]);
    });
  }

  runSubsystem("OVson::poll", []() {
    OVson::poll();
  });

  bool wantBetterTab = false;
  bool physicalTab = false;
  runSubsystem("BetterTab/state", [&]() {
    wantBetterTab = Config::isBetterTabModeEnabled() &&
                    (OVson::isInHypixelGame() || OVson::isInReplay()) &&
                    !OVson::isInPreGameLobby() && !OVson::isChatOpen();
    HWND hwnd = WindowFromDC((HDC)hdc);
    int tabVK = getPlayerListVK();
    if (hwnd && GetForegroundWindow() != hwnd) {
      physicalTab = false;
    } else {
      physicalTab = (GetAsyncKeyState(tabVK) & 0x8000) != 0;
    }

    static bool wasWantBetterTab = false;
    if (wantBetterTab != wasWantBetterTab) {
      if (hwnd && IsWindow(hwnd)) {
        UINT scan = (UINT)g_tabScan;
        if (wantBetterTab) {
          PostMessage(hwnd, WM_KEYUP, tabVK, ((LPARAM)scan << 16) | 0xC0000001);
        } else {
          if (physicalTab) {
            PostMessage(hwnd, WM_KEYDOWN, tabVK, ((LPARAM)scan << 16) | 0x00000001);
          }
        }
      }
      wasWantBetterTab = wantBetterTab;
    }
  });

  g_suppressVanillaTab.store(wantBetterTab && physicalTab);
  if ((wantBetterTab && physicalTab) || BetterTab::isResizeMode()) {
    runSubsystem("BetterTab/suppressVanilla", []() { suppressVanillaTab(); });
    runSubsystem("BetterTab::render", [hdc]() {
      BetterTab::render((void *)hdc);
    });
  }

  runSubsystem("BedDefenseManager::tick", []() {
    BedDefense::BedDefenseManager::getInstance()->tick();
  });

  runSubsystem("Anticheat::tickFromRenderThread", []() {
    Anticheat::tickFromRenderThread();
  });

  runSubsystem("ReplaySpammer::tick", []() {
    Utils::ReplaySpammer::getInstance().tick();
  });

  {
    std::vector<std::function<void()>> drained;
    {
      std::lock_guard<std::mutex> lock(g_queueMutex);
      drained.reserve(g_taskQueue.size());
      while (!g_taskQueue.empty()) {
        drained.push_back(std::move(g_taskQueue.front()));
        g_taskQueue.pop();
      }
    }
    for (auto &task : drained) {
      runSubsystem("RenderHook::queuedTask", [&]() { task(); });
    }
  }

  runSubsystem("BedwarsRuntime::tick", []() {
    OVson::Bedwars::Runtime::instance().tick();
  });

  runSubsystem("NickRoll::tick", []() { OVson::NickRoll::tick(); });

  runSubsystem("MediaOverlay::render", [hdc]() {
    GLint vp[4]{};
    glGetIntegerv(GL_VIEWPORT, vp);
    Render::MediaOverlay::render(hdc, vp[2], vp[3]);
  });

  runSubsystem("BedwarsOverlay::render", [hdc]() {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    Render::BedwarsOverlay::render(hdc, vp[2], vp[3]);
  });

  runSubsystem("ClickGUI::render", [hdc]() {
    if (Render::ClickGUI::isOpen()) {
      FocusFix::setIngameFocus(false);
    }
    Render::ClickGUI::render(hdc);
  });
}

BOOL WINAPI hookedSwapBuffers(HDC hdc) {
  static thread_local bool s_inHook = false;
  if (s_inHook) {
    return originalSwapBuffers(hdc);
  }
  s_inHook = true;
  FlagReleaser _releaser(&s_inHook);

  HookGuard guard;
  // Check the unload flag before touching any subsystem. Once teardown has
  // begun this stub may stay installed indefinitely (see the cross-mod note
  // above), so from here on it must be a pure pass-through and nothing else.
  if (g_unloading) {
    return originalSwapBuffers(hdc);
  }
  Watchdog::tickFrame();

  SafeGuard::installSehTranslator();

  static bool s_glExtInitialized = false;
  if (!s_glExtInitialized) {
    initGLExtensions();
    g_glUseProgram = (PFNGLUSEPROGRAMPROC_LOCAL)wglGetProcAddress("glUseProgram");
    StatsOverlay::init();
    Render::ClickGUI::init();
    s_glExtInitialized = true;
  }

  JNIEnv *env = (lc ? lc->getEnv() : nullptr);
  bool framePushed = false;
  if (env && env->PushLocalFrame(128) == 0) {
    framePushed = true;
  }

  SafeGuard::run("hookedSwapBuffers", [hdc]() { renderOverlayWorkBody(hdc); });
  SafeGuard::run("PacketHook", []() { PacketHook::update(); });

  if (framePushed) {
    env->PopLocalFrame(nullptr);
  }

  return originalSwapBuffers(hdc);
}

bool RenderHook::install() {
  writeDebugLog("RenderHook::install() called (dynamic MinHook.dll loading)");
  Logger::info("RenderHook: Loading MinHook.dll dynamically");

  if (!LoadMinHook()) {
    Logger::error("RenderHook: Failed to load MinHook.dll, overlay disabled");
    return false;
  }

  MH_STATUS_LOCAL status = pMH_Initialize();
  if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
    char buf[256];
    sprintf_s(buf, "MH_Initialize failed: %d", (int)status);
    writeDebugLog(buf);
    Logger::error(buf);
    return false;
  }
  writeDebugLog("MinHook initialized successfully");

  HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
  if (!hOpenGL) {
    writeDebugLog("ERROR: opengl32.dll not found");
    return false;
  }
  writeDebugLog("opengl32.dll found");

  FARPROC pSwapBuffers = GetProcAddress(hOpenGL, "wglSwapBuffers");
  if (!pSwapBuffers) {
    writeDebugLog("ERROR: wglSwapBuffers not found");
    return false;
  }

  char buf[256];
  sprintf_s(buf, "wglSwapBuffers found at 0x%p", pSwapBuffers);
  writeDebugLog(buf);

  status = pMH_CreateHook(pSwapBuffers, &hookedSwapBuffers,
                          reinterpret_cast<LPVOID *>(&originalSwapBuffers));
  if (status != MH_OK) {
    sprintf_s(buf, "MH_CreateHook failed: %d", (int)status);
    writeDebugLog(buf);
    Logger::error(buf);
    return false;
  }
  writeDebugLog("Hook created successfully");

  status = pMH_EnableHook(pSwapBuffers);
  if (status != MH_OK) {
    sprintf_s(buf, "MH_EnableHook failed: %d", (int)status);
    writeDebugLog(buf);
    Logger::error(buf);
    return false;
  }
  writeDebugLog("wglSwapBuffers hooked successfully!");
  snapshotPrologue(g_swapTarget, (LPVOID)pSwapBuffers);

  HMODULE hUser32 = GetModuleHandleA("user32.dll");
  if (hUser32) {
    FARPROC pSetCursorPos = GetProcAddress(hUser32, "SetCursorPos");
    if (pSetCursorPos) {
      if (pMH_CreateHook(pSetCursorPos, &hookedSetCursorPos, reinterpret_cast<LPVOID*>(&originalSetCursorPos)) == MH_OK) {
        if (pMH_EnableHook(pSetCursorPos) == MH_OK) {
          snapshotPrologue(g_cursorTarget, (LPVOID)pSetCursorPos);
        }
        writeDebugLog("SetCursorPos hooked successfully!");
      }
    }
  }
  
  g_hookInstalled = true;

  Watchdog::start();
  writeDebugLog("Watchdog started");

  Render::NotificationManager::getInstance()->add(
      "System", "OVson Client loaded successfully!",
      Render::NotificationType::Success);

  Logger::info("RenderHook: wglSwapBuffers hook installed successfully!");
  return true;
}

float RenderHook::getDelta() { return g_frameDelta; }

void RenderHook::uninstall() {
  writeDebugLog("RenderHook::uninstall() called");

  Watchdog::stop();
  writeDebugLog("Watchdog stopped");

  if (g_hookInstalled) {
    g_unloading = true;

    if (g_gameHwnd && IsWindow(g_gameHwnd) && originalWndProc) {
      if (GetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC) ==
          (LONG_PTR)hookedWndProc) {
        SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
        writeDebugLog("WndProc restored");
      } else {
        // Somebody subclassed on top of us. Their chain runs through
        // hookedWndProc, which lives in this module, so we can neither
        // unhook nor unload without breaking them. Stay resident and let
        // hookedWndProc keep forwarding (g_unloading makes it a
        // pass-through).
        g_mustStayLoaded.store(true);
        Logger::info("RenderHook: another module subclassed the window after "
                     "us -- leaving the WndProc chain intact so it keeps "
                     "working.");
        writeDebugLog("WndProc NOT restored (foreign subclass on top)");
      }
    }

    for (int i = 0; i < 3; ++i) {
      MSG msg;
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      Sleep(10);
    }

    int retries = 0;
    while (g_threadsInHook.load(std::memory_order_acquire) > 0 &&
           retries < 3000) {
      Sleep(10);
      retries++;
    }

    bool drained = (g_threadsInHook.load() == 0);
    if (!drained) {
      writeDebugLog(
          "WARNING: hook threads did NOT drain in 30s — skipping MinHook "
          "free to avoid a freed-trampoline crash. The DLL will leak; "
          "process exit will clean it up.");
      g_mustStayLoaded.store(true);
    }

    // MinHook's disable path rewrites the ORIGINAL prologue bytes back over
    // the function. If a later injection put its own jump there, that write
    // erases their hook, and MH_Uninitialize then frees the trampoline they
    // are still chained to. Only unhook while we are demonstrably still the
    // outermost hook on every function we touched.
    const bool swapStillOurs = prologueStillOurs(g_swapTarget);
    const bool cursorStillOurs = prologueStillOurs(g_cursorTarget);
    const bool hooksStillOurs = swapStillOurs && cursorStillOurs;
    if (!hooksStillOurs) {
      g_mustStayLoaded.store(true);
      Logger::info("RenderHook: another module hooked %s after us -- leaving "
                   "MinHook installed so its hook and trampoline stay valid.",
                   swapStillOurs ? "SetCursorPos" : "wglSwapBuffers");
      writeDebugLog("MinHook NOT removed (foreign hook stacked on top)");
    }

    const bool safeToRemoveHooks = drained && hooksStillOurs;
    if (safeToRemoveHooks) {
      if (pMH_DisableHook) {
        pMH_DisableHook(MH_ALL_HOOKS);
        writeDebugLog("MinHook disabled");
      }
      Sleep(150);
      if (pMH_Uninitialize) {
        pMH_Uninitialize();
      }
    }

    g_hookInstalled = false;

    if (safeToRemoveHooks && g_MinHookModule) {
      FreeLibrary(g_MinHookModule);
      g_MinHookModule = nullptr;
      writeDebugLog("MinHook.x64.dll unloaded");
    }
  }

  StatsOverlay::shutdown();
  Render::TechOverlay::shutdown();
  Render::BedwarsOverlay::shutdown();
  Render::MediaOverlay::shutdown();
  OVson::Media::shutdown();
}

bool RenderHook::mustStayLoaded() { return g_mustStayLoaded.load(); }

void *RenderHook::gameWindowHandle() { return (void *)g_gameHwnd; }

void RenderHook::poll() {
  if (g_suppressVanillaTab.load()) suppressVanillaTab();
}

void RenderHook::enqueueTask(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(g_queueMutex);
  g_taskQueue.push(task);
}
