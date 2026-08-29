#include "NotificationManager.h"
#include "../Config/Config.h"
#include "../Utils/GlGuard.h"
#include "../Utils/Timer.h"
#include "FontRenderer.h"
#include "RenderHook.h"
#include "RenderUtils.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
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
                              float duration, std::size_t maximumVisible) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const std::size_t limit = std::clamp<std::size_t>(maximumVisible, 1, 20);
  while (m_notifications.size() >= limit) {
    m_notifications.erase(m_notifications.begin());
  }

  Notification n;
  n.title = title;
  n.message = message;
  n.type = type;
  n.duration = std::clamp(duration, 0.5f, 30.0f);
  n.timer = 0.0f;
  n.slideAnim = 0.0f;
  m_notifications.push_back(n);

}

void NotificationManager::addRich(
    const std::string &title,
    const std::vector<NotificationSegment> &segments, NotificationType type,
    float duration, std::size_t maximumVisible) {
  std::string plain;
  std::vector<NotificationSegment> bounded;
  bounded.reserve(std::min<std::size_t>(segments.size(), 16));
  for (std::size_t i = 0; i < segments.size() && i < 16; ++i) {
    NotificationSegment segment = segments[i];
    segment.text = segment.text.substr(0, 128);
    plain += segment.text;
    bounded.push_back(std::move(segment));
  }
  std::lock_guard<std::mutex> lock(m_mutex);
  const std::size_t limit = std::clamp<std::size_t>(maximumVisible, 1, 20);
  while (m_notifications.size() >= limit)
    m_notifications.erase(m_notifications.begin());
  Notification notification;
  notification.title = title.substr(0, 128);
  notification.message = plain.substr(0, 512);
  notification.type = type;
  notification.duration = std::clamp(duration, 0.5f, 30.0f);
  notification.timer = 0.0f;
  notification.slideAnim = 0.0f;
  notification.segments = std::move(bounded);
  m_notifications.push_back(std::move(notification));
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


namespace {
// A notification used to draw its body on one line and let it run off the
// card. The scorer's rejection reasons ("first name and surname: amy dunn")
// are routinely wider than 460px, so the end of the explanation simply was not
// there. Wrap instead: pack segments onto lines, splitting a segment on spaces
// when it does not fit, and give the card the height it needs.
constexpr float kBodyMaxWidth = 420.0f;
constexpr std::size_t kMaxBodyLines = 3;
constexpr float kLineHeight = 14.0f;

using Line = std::vector<Render::NotificationSegment>;

void pushPiece(std::vector<Line> &lines, Line &current, float &width,
               const std::string &text, DWORD color, float pieceWidth) {
  if (text.empty()) return;
  // A single run with no space in it that is wider than the card on its own --
  // it cannot be wrapped, so it is cut. Nothing the scorer produces is this
  // long, but a page could hand us anything and running off the card is the
  // bug being fixed here.
  if (pieceWidth > kBodyMaxWidth) {
    const float perChar = pieceWidth / static_cast<float>(text.size());
    const std::size_t fits =
        perChar > 0.0f ? static_cast<std::size_t>(kBodyMaxWidth / perChar) : text.size();
    if (fits > 0 && fits < text.size()) {
      pushPiece(lines, current, width, text.substr(0, fits), color, fits * perChar);
      pushPiece(lines, current, width, text.substr(fits), color,
                pieceWidth - fits * perChar);
      return;
    }
  }
  if (!current.empty() && width + pieceWidth > kBodyMaxWidth) {
    lines.push_back(current);
    current.clear();
    width = 0.0f;
  }
  // Merge with the previous piece when it is the same colour, so a wrapped
  // sentence does not turn into a hundred one-word draw calls.
  if (!current.empty() && current.back().color == color)
    current.back().text += text;
  else
    current.push_back(Render::NotificationSegment{text, color});
  width += pieceWidth;
}

std::vector<Line> wrapSegments(const std::vector<Render::NotificationSegment> &segments) {
  std::vector<Line> lines;
  Line current;
  float width = 0.0f;

  for (const auto &segment : segments) {
    const float whole = g_notifyFont.getStringWidth(segment.text);
    if (width + whole <= kBodyMaxWidth || segment.text.find(' ') == std::string::npos) {
      pushPiece(lines, current, width, segment.text, segment.color, whole);
      continue;
    }
    // Too wide and it has spaces to break on.
    std::size_t start = 0;
    while (start < segment.text.size()) {
      std::size_t space = segment.text.find(' ', start);
      const std::size_t end = space == std::string::npos ? segment.text.size() : space + 1;
      const std::string word = segment.text.substr(start, end - start);
      pushPiece(lines, current, width, word, segment.color,
                g_notifyFont.getStringWidth(word));
      start = end;
    }
  }
  if (!current.empty()) lines.push_back(current);
  if (lines.empty()) lines.push_back(Line{});

  if (lines.size() > kMaxBodyLines) {
    lines.resize(kMaxBodyLines);
    if (!lines.back().empty()) lines.back().back().text += "...";
  }
  return lines;
}
} // namespace

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
  const float notifBaseH = 62.0f;
  const float radius    = 12.0f;
  const float textPadL  = 18.0f;   // text starts this far in from left
  const float gap       = 10.0f;
  const float revealDur = 0.35f;
  const float hideDur   = 0.30f;

  float yPos = sh - padding;

  for (auto it = m_notifications.begin(); it != m_notifications.end();) {
    float titleW = g_notifyFont.getStringWidth(it->title);
    const std::vector<Render::NotificationSegment> body =
        it->segments.empty()
            ? std::vector<Render::NotificationSegment>{{it->message, 0xFFC8C8D0}}
            : it->segments;
    const std::vector<Line> bodyLines = wrapSegments(body);
    float msgW = 0.0f;
    for (const auto &line : bodyLines) {
      float lineW = 0.0f;
      for (const auto &segment : line)
        lineW += g_notifyFont.getStringWidth(segment.text);
      if (lineW > msgW) msgW = lineW;
    }
    const float notifH =
        notifBaseH + static_cast<float>(bodyLines.size() - 1) * kLineHeight;
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
    yPos -= notifH;
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
    float lineY = drawY + 33.0f;
    for (const auto &line : bodyLines) {
      float segmentX = textX;
      for (const auto &segment : line) {
        g_notifyFont.drawString(segmentX, lineY, segment.text,
                                applyAlpha(segment.color, alpha));
        segmentX += g_notifyFont.getStringWidth(segment.text);
      }
      lineY += kLineHeight;
    }

    glDisable(GL_TEXTURE_2D);

    yPos -= gap;
    ++it;
  }

}
} // namespace Render
