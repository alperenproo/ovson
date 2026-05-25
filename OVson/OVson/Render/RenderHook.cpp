#include "RenderHook.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Logic/StatsTracker.h"
#include "../Utils/Anticheat/AcInternal.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/Logger.h"
#include "../Utils/ReplaySpammer.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/SensitivityFix.h"
#include "BetterTab.h"
#include "ClickGUI.h"
#include "DefenseRenderer.h"
#include "NameTagRenderer.h"
#include "NotificationManager.h"
#include "StatsOverlay.h"
#include "TechOverlay.h"
#include <fstream>
#include <functional>
#include <gl/GL.h>
#include <mutex>
#include <windows.h>

#include "../Utils/Timer.h"
#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>

static std::ofstream g_debugLog;
static bool g_hookInstalled = false;

static std::queue<std::function<void()>> g_taskQueue;
static std::mutex g_queueMutex;
static float g_frameDelta = 0.0f;
static std::atomic<bool> g_unloading{false};
static std::atomic<int> g_threadsInHook{0};
static std::atomic<bool> g_tabDown{false};

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

static void writeDebugLog(const char *msg) {
  if (!g_debugLog.is_open()) {
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
  }
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
  g_threadsInHook++;
  if (g_unloading) {
    LRESULT res = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
    g_threadsInHook--;
    return res;
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
      g_threadsInHook--;
      return 0;
    }
  }

  if (uMsg == WM_KEYDOWN && wParam == VK_TAB) {
    if (Config::isBetterTabModeEnabled() && OVson::isInHypixelGame() &&
        !OVson::isInPreGameLobby() && !OVson::isChatOpen()) {
      return 0;
    }
  }

  if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
    if (OVson::handleEnterKeyPress()) {
      g_threadsInHook--;
      return 0;
    }
  }

  LRESULT res = CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
  g_threadsInHook--;
  return res;
}

static void renderOverlayWorkBody(HDC hdc) {
  static int frameCount = 0;
  frameCount++;

  g_frameDelta = TimeUtil::getDelta();

  if (g_glUseProgram)
    g_glUseProgram(0);

  HWND currentHwnd = WindowFromDC(hdc);
  if (currentHwnd && currentHwnd != g_gameHwnd) {
    if (g_gameHwnd && originalWndProc) {
      SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
    }
    g_gameHwnd = currentHwnd;
    originalWndProc = (WNDPROC)SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC,
                                                (LONG_PTR)hookedWndProc);
    writeDebugLog("WndProc hooked (re-hook on HWND change)");
  } else if (!g_gameHwnd && currentHwnd) {
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

  if (Config::isMotionBlurEnabled()) {
    float amount = Config::getMotionBlurAmount();
    if (amount > 0.01f) {
      GLint viewport[4];
      glGetIntegerv(GL_VIEWPORT, viewport);
      int sw = viewport[2];
      int sh = viewport[3];

      static GLuint g_blurTex = 0;
      static int    g_blurTexW = 0;
      static int    g_blurTexH = 0;
      static bool   g_blurHavePrev = false;

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
          g_blurHavePrev = false;
        }

        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT |
                     GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT |
                     GL_TRANSFORM_BIT);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_FOG);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, sw, 0, sh, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        if (g_blurHavePrev) {
          glEnable(GL_TEXTURE_2D);
          glBindTexture(GL_TEXTURE_2D, g_blurTex);
          float alpha = amount * 0.85f;
          if (alpha > 0.92f) alpha = 0.92f;
          glColor4f(1, 1, 1, alpha);
          glBegin(GL_QUADS);
          glTexCoord2f(0, 0); glVertex2f(0,  0);
          glTexCoord2f(1, 0); glVertex2f((float)sw, 0);
          glTexCoord2f(1, 1); glVertex2f((float)sw, (float)sh);
          glTexCoord2f(0, 1); glVertex2f(0, (float)sh);
          glEnd();
          glBindTexture(GL_TEXTURE_2D, 0);
          glDisable(GL_TEXTURE_2D);
        }

        glBindTexture(GL_TEXTURE_2D, g_blurTex);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sw, sh);
        glBindTexture(GL_TEXTURE_2D, 0);
        g_blurHavePrev = true;

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glPopAttrib();
      }
    }
  }

  StatsOverlay::render((void *)hdc);

  BedDefense::DefenseRenderer::getInstance()->render((void *)hdc, 0.0);

  SafeGuard::run("NameTagRenderer::render", [hdc]() {
    OVson::NameTagRenderer::getInstance()->render((void *)hdc, 0.0);
  });

  if (Config::isNotificationsEnabled()) {
    Render::NotificationManager::getInstance()->render(hdc);
  }

  if (Config::isTechEnabled()) {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    Render::TechOverlay::render(hdc, vp[2], vp[3]);
  }

  OVson::poll();

  bool wantBetterTab = Config::isBetterTabModeEnabled() &&
                       OVson::isInHypixelGame() && !OVson::isInPreGameLobby() &&
                       !OVson::isChatOpen();
  bool physicalTab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

  static bool wasWantBetterTab = false;
  if (wantBetterTab != wasWantBetterTab) {
    HWND hwnd = WindowFromDC((HDC)hdc);
    if (hwnd) {
      if (wantBetterTab) {
        PostMessage(hwnd, WM_KEYUP, VK_TAB, 0xC00F0001);
      } else {
        if (physicalTab) {
          PostMessage(hwnd, WM_KEYDOWN, VK_TAB, 0x000F0001);
        }
      }
    }
    wasWantBetterTab = wantBetterTab;
  }

  if (wantBetterTab && physicalTab) {
    BetterTab::render((void *)hdc);
  }

  BedDefense::BedDefenseManager::getInstance()->tick();

  Anticheat::tickFromRenderThread();

  Utils::ReplaySpammer::getInstance().tick();

  {
    std::lock_guard<std::mutex> lock(g_queueMutex);
    while (!g_taskQueue.empty()) {
      auto task = std::move(g_taskQueue.front());
      g_taskQueue.pop();
      try {
        task();
      } catch (const std::exception &e) {
        Logger::error("[SafeGuard] enqueueTask exception: %s", e.what());
      } catch (...) {
        Logger::error("[SafeGuard] enqueueTask unknown exception");
      }
    }
  }

  if (Render::ClickGUI::isOpen()) {
    FocusFix::setIngameFocus(false);
  }
  Render::ClickGUI::render(hdc);
}

BOOL WINAPI hookedSwapBuffers(HDC hdc) {
  g_threadsInHook++;
  if (g_unloading) {
    BOOL res = originalSwapBuffers(hdc);
    g_threadsInHook--;
    return res;
  }

  SafeGuard::installSehTranslator();

  JNIEnv *env = (lc ? lc->getEnv() : nullptr);
  bool framePushed = false;
  if (env && env->PushLocalFrame(128) == 0) {
    framePushed = true;
  }

  SafeGuard::run("hookedSwapBuffers", [hdc]() { renderOverlayWorkBody(hdc); });

  if (framePushed) {
    env->PopLocalFrame(nullptr);
  }

  g_threadsInHook--;
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
  g_hookInstalled = true;

  StatsOverlay::init();
  writeDebugLog("StatsOverlay initialized");

  Render::ClickGUI::init();

  g_glUseProgram = (PFNGLUSEPROGRAMPROC_LOCAL)wglGetProcAddress("glUseProgram");
  if (g_glUseProgram)
    writeDebugLog("glUseProgram loaded successfully");
  else
    writeDebugLog("glUseProgram not supported/found");

  Render::NotificationManager::getInstance()->add(
      "System", "OVson Client loaded successfully!",
      Render::NotificationType::Success);

  Logger::info("RenderHook: wglSwapBuffers hook installed successfully!");
  return true;
}

float RenderHook::getDelta() { return g_frameDelta; }

void RenderHook::uninstall() {
  writeDebugLog("RenderHook::uninstall() called");

  if (g_hookInstalled) {
    g_unloading = true;

    int retries = 0;
    while (g_threadsInHook.load() > 0 && retries < 500) {
      Sleep(10);
      retries++;
    }

    if (pMH_DisableHook) {
      pMH_DisableHook(MH_ALL_HOOKS);
      writeDebugLog("MinHook disabled");
    }

    if (g_gameHwnd && originalWndProc) {
      SetWindowLongPtr(g_gameHwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
      writeDebugLog("WndProc restored");
    }

    Sleep(100);

    if (pMH_Uninitialize) {
      pMH_Uninitialize();
    }

    g_hookInstalled = false;
  }

  if (g_MinHookModule) {
    FreeLibrary(g_MinHookModule);
    g_MinHookModule = nullptr;
    writeDebugLog("MinHook.x64.dll unloaded");
  }

  StatsOverlay::shutdown();
  Render::TechOverlay::shutdown();
}

void RenderHook::poll() {
  // not used when hook is active
}

void RenderHook::enqueueTask(std::function<void()> task) {
  std::lock_guard<std::mutex> lock(g_queueMutex);
  g_taskQueue.push(task);
}
