#include "NotificationManager.h"
#include "../Config/Config.h"
#include "../Utils/GlGuard.h"
#include "../Utils/Timer.h"
#include "FontRenderer.h"
#include "RenderHook.h"
#include "RenderUtils.h"
#include <algorithm>
#include <cmath>
#include <Windows.h>
#include <gl/GL.h>


namespace Render {

static FontRenderer g_notifyFont;

static uint32_t applyAlpha(uint32_t color, float alpha) {
  uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * alpha);
  return (uint32_t)((a << 24) | (color & 0x00FFFFFF));
}

NotificationManager *NotificationManager::getInstance() {
  static NotificationManager s_instance;
  return &s_instance;
}

DWORD Notification::getTitleColor() const {
  switch (type) {
  case NotificationType::Success:
    return 0xFF00FF55; // Vibrant Green
  case NotificationType::Error:
    return 0xFFFF3333; // Vibrant Red
  case NotificationType::Warning:
    return 0xFFFFCC00; // Bright Yellow
  case NotificationType::Info:
    return Config::getThemeColor();
  default:
    return 0xFFFFFFFF;
  }
}

DWORD Notification::getBodyColor() const {
  return 0xFFE0E0E0; // Light White
}

void NotificationManager::add(const std::string &title,
                              const std::string &message, NotificationType type,
                              float duration) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const size_t MAX_NOTIFICATIONS = 20;
  while (m_notifications.size() >= MAX_NOTIFICATIONS) {
    m_notifications.erase(m_notifications.begin());
  }

  Notification n;
  n.title = title;
  n.message = message;
  n.type = type;
  n.duration = duration;
  n.timer = 0.0f;
  n.slideAnim = 0.0f;
  m_notifications.push_back(n);

  OutputDebugStringA(
      ("[OVson] Notification Added: " + title + " - " + message + "\n")
          .c_str());
}

static void drawRect(float x, float y, float w, float h, DWORD color,
                     float alphaMult = 1.0f) {
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = (((color >> 24) & 0xFF) / 255.0f) * alphaMult;
  glColor4f(r, g, b, a);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
}

static float easeOutCubic(float x) {
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  float u = 1.0f - x;
  return 1.0f - u * u * u;
}

static float easeInCubic(float x) {
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  return x * x * x;
}

void NotificationManager::render(HDC hdc) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_notifications.empty())
    return;

  if (!m_fontInit || !g_notifyFont.isInitialized()) {
    if (g_notifyFont.init(hdc)) {
      m_fontInit = true;
      OutputDebugStringA("[OVson] Notification Font Initialized\n");
    } else {
      return;
    }
  }

  float dt = RenderHook::getDelta();

  GlGuard::GlAttribGuard  _gAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT |
                                    GL_DEPTH_BUFFER_BIT | GL_SCISSOR_BIT);
  GlGuard::GlMatrixGuard  _gMv(GL_MODELVIEW);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  GlGuard::GlMatrixGuard  _gPr(GL_PROJECTION);
  glLoadIdentity();

  HWND hwnd = WindowFromDC(hdc);
  if (!hwnd)
    hwnd = GetActiveWindow();

  RECT rect = {0};
  if (hwnd)
    GetClientRect(hwnd, &rect);
  float sw = (float)(rect.right - rect.left);
  float sh = (float)(rect.bottom - rect.top);

  if (sw <= 0 || sh <= 0) {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    sw = (float)vp[2];
    sh = (float)vp[3];
  }

  if (sw <= 0 || sh <= 0) {
    return;
  }

  glOrtho(0, sw, sh, 0, -1, 1);

  GlGuard::GlMatrixGuard _gMvInner(GL_MODELVIEW);
  glLoadIdentity();

  const float padding   = 22.0f;
  const float notifH    = 62.0f;
  const float radius    = 12.0f;
  const float textPadL  = 18.0f;   // text starts this far in from left
  const float gap       = 10.0f;
  const float revealDur = 0.35f;
  const float hideDur   = 0.30f;

  float yPos = sh - notifH - padding;

  for (auto it = m_notifications.begin(); it != m_notifications.end();) {
    float titleW = g_notifyFont.getStringWidth(it->title);
    float msgW   = g_notifyFont.getStringWidth(it->message);
    float maxContentW = (titleW > msgW) ? titleW : msgW;
    float notifW = textPadL + maxContentW + 22.0f;
    if (notifW < 280.0f) notifW = 280.0f;
    if (notifW > 460.0f) notifW = 460.0f;

    it->timer += dt;

    float life = it->timer / it->duration;
    float alpha = 1.0f;
    float slide = 1.0f;   // 0 = off-screen right, 1 = at rest
    float scale = 1.0f;   // 0.96..1.0 — subtle pop on enter

    if (it->timer < revealDur) {
      float t = it->timer / revealDur;
      slide = easeOutCubic(t);
      alpha = easeOutCubic(t);
      scale = 0.96f + 0.04f * easeOutCubic(t);
    } else if (it->timer > it->duration - hideDur) {
      float t = (it->duration - it->timer) / hideDur;
      slide = 1.0f - easeInCubic(1.0f - t);
      alpha = t;
      scale = 0.96f + 0.04f * t;
    }

    if (it->timer >= it->duration) {
      it = m_notifications.erase(it);
      continue;
    }

    const float restX = sw - notifW - padding;
    float x = restX + (1.0f - slide) * 40.0f;
    float y = yPos;

    float drawW = notifW * scale;
    float drawH = notifH * scale;
    float drawX = x + (notifW - drawW);
    float drawY = y + (notifH - drawH) / 2.0f;

    DWORD accent = it->getTitleColor();

    glDisable(GL_TEXTURE_2D);

    {
      const int   kSteps    = 16;
      const float kStepSpread = 1.0f;
      const float kBaseAlpha  = 0.045f;
      const float kFalloff    = 0.85f;   // exponential decay per step
      float a = kBaseAlpha;
      for (int s = 0; s < kSteps; ++s) {
        float spread = (s + 1) * kStepSpread;
        RenderUtils::drawRoundedRect(
            drawX - spread, drawY - spread + 3.0f,
            drawW + 2 * spread, drawH + 2 * spread,
            radius + spread,
            0x000000, a * alpha);
        a *= kFalloff;
      }
    }

    RenderUtils::drawRoundedRect(drawX, drawY, drawW, drawH, radius,
                                  0xFF10131C, 0.95f * alpha);

    RenderUtils::drawRoundedRect(drawX, drawY, drawW, drawH * 0.48f, radius,
                                  0xFFFFFFFF, 0.05f * alpha);

    float progress = 1.0f - life;
    if (progress > 0) {
      float pillW = (drawW - 28.0f) * progress;
      RenderUtils::drawRoundedRect(drawX + 14, drawY + drawH - 6, pillW, 2.0f,
                                    1.0f, accent, 0.85f * alpha);
    }

    glEnable(GL_TEXTURE_2D);

    float textX = drawX + textPadL;
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawCircle(textX, drawY + 19.0f, 2.5f, accent, alpha);
    glEnable(GL_TEXTURE_2D);
    g_notifyFont.drawString(textX + 8.0f, drawY + 13.0f, it->title,
                            applyAlpha(accent, alpha));
    g_notifyFont.drawString(textX, drawY + 33.0f, it->message,
                            applyAlpha(0xFFC8C8D0, alpha));

    glDisable(GL_TEXTURE_2D);

    yPos -= (notifH + gap);
    ++it;
  }

}
} // namespace Render
