#include "TechOverlay.h"
#include "../Chat/ChatInterceptor.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Render/FontRenderer.h"
#include "../Render/RenderUtils.h"
#include "../Utils/Logger.h"
#include "../Utils/ThreadTracker.h"
#include <iomanip>
#include <psapi.h>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "psapi.lib")

namespace Render {
namespace TechOverlay {
static bool s_dragging = false;
static float s_dragOffsetX = 0, s_dragOffsetY = 0;
static FontRenderer s_font;

void render(void *hdc, int screenWidth, int screenHeight) {
  if (!Config::isTechEnabled())
    return;

  if (!s_font.isInitialized()) {
    s_font.init((HDC)hdc);
  }

  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  float latency = ChatInterceptor::g_jniLatency;
  int activeThreads = ThreadTracker::g_activeThreads.load();

  size_t cachedPlayers = 0;
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    cachedPlayers = ChatInterceptor::g_playerStatsMap.size();
  }

  static int s_cachedFps = 0;
  static ULONGLONG s_lastFpsUpdate = 0;
  ULONGLONG now = GetTickCount64();

  if (now - s_lastFpsUpdate > 500) {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (mcCls) {
      jmethodID m_getFps = env->GetStaticMethodID(mcCls, "getDebugFPS", "()I");
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getFps = env->GetStaticMethodID(mcCls, "func_175610_ah", "()I");
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        m_getFps = env->GetStaticMethodID(mcCls, "ai", "()I");
      }
      if (m_getFps)
        s_cachedFps = env->CallStaticIntMethod(mcCls, m_getFps);
    }
    s_lastFpsUpdate = now;
  }
  int fps = s_cachedFps;

  size_t cacheBytes = 0;
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    for (const auto &pair : ChatInterceptor::g_playerStatsMap) {
      cacheBytes += sizeof(pair.first) + pair.first.capacity();
      const auto &s = pair.second;
      cacheBytes += sizeof(s) + s.uuid.capacity() + s.displayName.capacity() +
                    s.teamColor.capacity() + s.tagsDisplay.capacity();
      for (const auto &t : s.rawTags)
        cacheBytes += sizeof(t) + t.capacity();
    }
  }
  float cacheMB = (float)cacheBytes / (1024.0f * 1024.0f);

  float x = Config::getTechX() * screenWidth;
  float y = Config::getTechY() * screenHeight;

  glPushMatrix();

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, screenWidth, screenHeight, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  std::vector<std::string> lines;
  lines.push_back("\xC2\xA7"
                  "5[OVson Tech]");

  std::stringstream ss_latency;
  ss_latency << std::fixed << std::setprecision(2) << latency;
  lines.push_back("\xC2\xA7"
                  "7JNI Latency: " +
                  std::string(latency > 5.0f ? "\xC2\xA7"
                                               "c"
                                             : "\xC2\xA7"
                                               "a") +
                  ss_latency.str() + "ms");

  std::stringstream ss_cache;
  ss_cache << std::fixed << std::setprecision(3) << cacheMB;

  int processed = ChatInterceptor::getProcessedCount();
  int active = ChatInterceptor::getActiveFetchCount();
  int pending = ChatInterceptor::getPendingStatsCount();
  float apiLat = ChatInterceptor::getApiLatency();
  float scanSpeed = ChatInterceptor::getScanSpeed();

  lines.push_back("\xC2\xA7"
                  "7Threads: \xC2\xA7"
                  "b" +
                  std::to_string(activeThreads));
  lines.push_back("\xC2\xA7"
                  "7Session Scans: \xC2\xA7"
                  "a" +
                  std::to_string(processed));

  std::stringstream ss_q;
  ss_q << "\xC2\xA7" << "7Queue: \xC2\xA7" << "e" << active << "\xC2\xA7"
       << "7/\xC2\xA7" << "6" << pending;
  lines.push_back(ss_q.str());

  std::stringstream ss_apilat;
  ss_apilat << std::fixed << std::setprecision(1) << apiLat;
  lines.push_back("\xC2\xA7"
                  "7API Latency: \xC2\xA7"
                  "d" +
                  ss_apilat.str() + "ms");

  std::stringstream ss_sspeed;
  ss_sspeed << std::fixed << std::setprecision(1) << scanSpeed;
  lines.push_back("\xC2\xA7"
                  "7Scan Speed: \xC2\xA7"
                  "b" +
                  ss_sspeed.str() + "ms/p");

  lines.push_back("\xC2\xA7"
                  "7Client Cache: \xC2\xA7"
                  "6" +
                  ss_cache.str() + "MB");
  lines.push_back("\xC2\xA7"
                  "7FPS: \xC2\xA7"
                  "f" +
                  std::to_string(fps));

  float width = 140;
  float height = (float)lines.size() * 12 + 8;

  RenderUtils::drawRect(x, y, width, height, 0xAA0F172A);
  RenderUtils::drawRect(x, y, width, 1.5f, 0xFF6366F1);

  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  for (size_t i = 0; i < lines.size(); ++i) {
    s_font.drawString(x + 5, y + 5 + (float)i * 12, lines[i], 0xFFFFFFFF);
  }
  glDisable(GL_TEXTURE_2D);

  glPopAttrib();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  if (Config::isClickGuiOn()) {
    HWND gameHwnd = WindowFromDC((HDC)hdc);
    if (gameHwnd) {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(gameHwnd, &pt);
      float mx = (float)pt.x;
      float my = (float)pt.y;

      bool isMouseOver =
          mx >= x && mx <= x + width && my >= y && my <= y + height;

      if (isMouseOver || s_dragging) {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
          if (!s_dragging) {
            s_dragging = true;
            s_dragOffsetX = mx - x;
            s_dragOffsetY = my - y;
          }
          Config::setTechX((mx - s_dragOffsetX) / (float)screenWidth);
          Config::setTechY((my - s_dragOffsetY) / (float)screenHeight);
        } else {
          s_dragging = false;
        }
      }
    }
  } else {
    s_dragging = false;
  }
}
void shutdown() {
  //
}
} // namespace TechOverlay
} // namespace Render
