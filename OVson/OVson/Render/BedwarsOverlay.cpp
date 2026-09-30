#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BedwarsOverlay.h"

#include "FontRenderer.h"
#include "McFont.h"
#include "RenderUtils.h"
#include "../ClickGUI/ClickGUI.h"
#include "../ClickGUI/State.h"
#include "../Java.h"
#include "../Logic/Bedwars/BedwarsConfig.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"
#include "../Logic/StatsTracker.internal.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/GL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace Render {
McFont g_mcFont;
}

namespace Render::BedwarsOverlay {
namespace {

using OVson::Bedwars::HudId;
using OVson::Bedwars::HudLayout;
using OVson::Bedwars::kHudCount;

constexpr float kBaseScale = 1.5F;

FontRenderer g_font;
bool g_layoutMode = false;
std::optional<HudId> g_draggedHud;
HudLayout g_dragLayout;
float g_dragOffsetX = 0.0F;
float g_dragOffsetY = 0.0F;
bool g_previousMouseDown = false;
using Render::g_mcFont;

struct HudPanel {
  HudId id = HudId::EventTimer;
  HudLayout layout;
  std::vector<std::string> lines;
  std::uint32_t color = 0xFFFFFFFF;
  float x = 0.0F;
  float y = 0.0F;
  float width = 0.0F;
  float height = 0.0F;
};

std::vector<std::string> previewLines(HudId id) {
  switch (id) {
  case HudId::EventTimer: return {"Diamond II  4:32", "Emerald II  7:32"};
  case HudId::Height: return {"Y 72  Map Preview  Build 96  Remaining 24"};
  case HudId::Resource:
    return {"§fIron 48", "§6Gold 12", "§bDiamond 4", "§aEmerald 2"};
  case HudId::TeamState:
    return {"Sharpness 1", "Protection 2", "Trap queued"};
  default: return {"HUD preview"};
  }
}

void fillPanelLines(HudPanel &panel,
                    const OVson::Bedwars::RenderSnapshot &snapshot,
                    bool preview) {
  switch (panel.id) {
  case HudId::EventTimer: panel.lines = snapshot.timerLines; break;
  case HudId::Height:
    if (!snapshot.heightLine.empty()) panel.lines = {snapshot.heightLine};
    break;
  case HudId::Resource: panel.lines = snapshot.resourceLines; break;
  case HudId::TeamState: panel.lines = snapshot.upgradeLines; break;
  default: break;
  }
  if (panel.lines.empty() && preview)
    panel.lines = previewLines(panel.id);
}

void measureAndClamp(HudPanel &panel, int screenWidth, int screenHeight) {
  const float scale = std::clamp(panel.layout.scale, 0.5F, 2.5F) * kBaseScale;
  float textWidth = 0.0F;
  const bool useMcFont = g_mcFont.ready;

  for (const auto &line : panel.lines) {
    float w = 0.0F;
    if (useMcFont) {
      w = g_mcFont.getStringWidth(line) * scale;
    }
    if (w <= 0.0F) {
      w = g_font.getStringWidth(line) * (0.5F * scale);
    }
    textWidth = (std::max)(textWidth, w);
  }

  panel.width = (std::max)(36.0F, textWidth + 4.0F);
  const float lineH = (useMcFont ? 10.0F : 13.0F) * scale;
  panel.height = (std::max)(12.0F, lineH * static_cast<float>(panel.lines.size()) + 2.0F);
  panel.layout = OVson::Bedwars::sanitizeHudLayout(
      panel.layout, panel.width / static_cast<float>(screenWidth),
      panel.height / static_cast<float>(screenHeight));
  panel.x = panel.layout.x * screenWidth;
  panel.y = panel.layout.y * screenHeight;
}

bool contains(const HudPanel &panel, float x, float y) {
  return x >= panel.x - 4.0F && x <= panel.x + panel.width + 4.0F &&
         y >= panel.y - 4.0F && y <= panel.y + panel.height + 4.0F;
}

static bool isCursorActive() {
  if (Render::ClickGUI::isOpen())
    return true;

  if (lc) {
    JNIEnv *env = lc->getEnv();
    if (env) {
      jclass mouseCls = lc->GetClass("org.lwjgl.input.Mouse");
      if (mouseCls) {
        static jmethodID m_isGrabbed = nullptr;
        if (!m_isGrabbed) {
          m_isGrabbed = env->GetStaticMethodID(mouseCls, "isGrabbed", "()Z");
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (m_isGrabbed) {
          jboolean grabbed = env->CallStaticBooleanMethod(mouseCls, m_isGrabbed);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (!grabbed) return true;
        }
      }

      jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
      if (mcCls) {
        static jfieldID f_theMc = nullptr;
        if (!f_theMc) {
          f_theMc = lc->GetStaticFieldID(mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S");
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        static jfieldID f_screen = nullptr;
        if (!f_screen) {
          f_screen = lc->GetFieldID(mcCls, "currentScreen", "Lnet/minecraft/client/gui/GuiScreen;", "field_71462_r", "m");
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (f_theMc && f_screen) {
          jobject mc = env->GetStaticObjectField(mcCls, f_theMc);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (mc) {
            jobject screen = env->GetObjectField(mc, f_screen);
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(mc);
            if (screen) {
              env->DeleteLocalRef(screen);
              return true;
            }
          }
        }
      }
    }
  }

  CURSORINFO ci{ sizeof(CURSORINFO) };
  if (GetCursorInfo(&ci)) {
    if (ci.flags & CURSOR_SHOWING)
      return true;
  }

  return false;
}

void drawPanel(const HudPanel &panel, bool preview, bool cursorActive, float mx, float my) {
  const float drawX = std::floor(panel.x);
  const float drawY = std::floor(panel.y);

  const bool isDragged = (g_draggedHud && *g_draggedHud == panel.id);
  const bool isHovered = cursorActive && contains(panel, mx, my);

  if (preview || isDragged || isHovered) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.0F, 0.0F, 0.0F, isDragged ? 0.65F : (isHovered ? 0.50F : 0.40F));
    glBegin(GL_QUADS);
    glVertex2f(drawX - 2.0F, drawY - 2.0F);
    glVertex2f(drawX + panel.width + 2.0F, drawY - 2.0F);
    glVertex2f(drawX + panel.width + 2.0F, drawY + panel.height + 2.0F);
    glVertex2f(drawX - 2.0F, drawY + panel.height + 2.0F);
    glEnd();
    if (isDragged) {
      glColor4f(0.2F, 0.85F, 1.0F, 1.0F);
    } else if (isHovered) {
      glColor4f(0.35F, 0.80F, 1.0F, 0.85F);
    } else {
      glColor4f(0.35F, 0.72F, 1.0F, 0.65F);
    }
    glBegin(GL_LINE_LOOP);
    glVertex2f(drawX - 2.0F, drawY - 2.0F);
    glVertex2f(drawX + panel.width + 2.0F, drawY - 2.0F);
    glVertex2f(drawX + panel.width + 2.0F, drawY + panel.height + 2.0F);
    glVertex2f(drawX - 2.0F, drawY + panel.height + 2.0F);
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }

  const float scale = std::clamp(panel.layout.scale, 0.5F, 2.5F) * kBaseScale;
  const bool useMcFont = g_mcFont.ready;
  const float lineStep = (useMcFont ? 10.0F : 13.0F) * scale;
  float cursor = drawY;

  for (const auto &line : panel.lines) {
    uint32_t lineColor = panel.color;
    if (panel.id == HudId::Resource) {
      if (line.find("Iron") != std::string::npos || line.find("iron") != std::string::npos) {
        lineColor = 0xFFFFFFFF;
      } else if (line.find("Gold") != std::string::npos || line.find("gold") != std::string::npos) {
        lineColor = 0xFFFFAA00;
      } else if (line.find("Diamond") != std::string::npos || line.find("diamond") != std::string::npos) {
        lineColor = 0xFF55FFFF;
      } else if (line.find("Emerald") != std::string::npos || line.find("emerald") != std::string::npos) {
        lineColor = 0xFF55FF55;
      }
    }

    if (useMcFont) {
      glPushMatrix();
      glTranslatef(drawX, std::floor(cursor), 0.0F);
      if (std::abs(scale - 1.0F) > 0.01F) {
        glScalef(scale, scale, 1.0F);
      }
      g_mcFont.drawStringWithShadow(line, 0.0F, 0.0F, lineColor);
      glPopMatrix();
    } else {
      g_font.drawString(drawX, std::floor(cursor), line, lineColor, 0.5F * scale);
    }
    cursor += lineStep;
  }
}

void updateDragging(std::array<HudPanel, kHudCount> &panels, HDC hdc,
                    int screenWidth, int screenHeight, float mx, float my, bool cursorActive) {
  if (!cursorActive) {
    g_draggedHud.reset();
    g_previousMouseDown = false;
    return;
  }
  HWND window = WindowFromDC(hdc);
  if (!window || GetForegroundWindow() != window)
    return;
  if (mx < 0.0f || my < 0.0f || mx > static_cast<float>(screenWidth) || my > static_cast<float>(screenHeight))
    return;

  const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

  if (down && !g_previousMouseDown) {
    if (Render::ClickGUI::isOpen() &&
        mx >= Render::ClickGUIState::g_x &&
        mx <= Render::ClickGUIState::g_x + Render::ClickGUIState::g_w &&
        my >= Render::ClickGUIState::g_y &&
        my <= Render::ClickGUIState::g_y + Render::ClickGUIState::g_h) {
      g_previousMouseDown = down;
      return;
    }

    for (auto it = panels.rbegin(); it != panels.rend(); ++it) {
      if (it->layout.visible && contains(*it, mx, my)) {
        g_draggedHud = it->id;
        g_dragLayout = it->layout;
        g_dragOffsetX = mx - it->x;
        g_dragOffsetY = my - it->y;
        break;
      }
    }
  }
  if (down && g_draggedHud) {
    auto &panel = panels[static_cast<std::size_t>(*g_draggedHud)];
    panel.layout.x = (mx - g_dragOffsetX) / static_cast<float>(screenWidth);
    panel.layout.y = (my - g_dragOffsetY) / static_cast<float>(screenHeight);
    measureAndClamp(panel, screenWidth, screenHeight);
    g_dragLayout = panel.layout;
  }
  if (!down && g_previousMouseDown && g_draggedHud) {
    OVson::Bedwars::Configuration::setHudLayout(*g_draggedHud, g_dragLayout);
    g_draggedHud.reset();
  }
  g_previousMouseDown = down;
}

} // namespace

void setLayoutMode(bool enabled) {
  g_layoutMode = enabled;
  if (!enabled) {
    if (g_draggedHud)
      OVson::Bedwars::Configuration::setHudLayout(*g_draggedHud, g_dragLayout);
    g_draggedHud.reset();
    g_previousMouseDown = false;
  }
}

bool isLayoutMode() { return g_layoutMode; }

void render(void *hdcValue, int screenWidth, int screenHeight) {
  if (!hdcValue || screenWidth <= 0 || screenHeight <= 0)
    return;
  HDC hdc = static_cast<HDC>(hdcValue);
  const bool cursorActive = isCursorActive();
  const auto snapshot = OVson::Bedwars::Runtime::instance().snapshot();
  const auto settings = OVson::Bedwars::Configuration::get();

  if (!settings.masterEnabled || !snapshot.active || OVson::g_inPreGameLobby || !OVson::g_inHypixelGame)
    return;

  const bool preview = false;
  if (!g_font.isInitialized() && !g_font.init(hdc))
    return;

  if (!g_mcFont.ready) {
    JNIEnv *env = lc ? lc->getEnv() : nullptr;
    if (env) {
      g_mcFont.init(env);
    }
  }

  POINT mouse{};
  HWND window = WindowFromDC(hdc);
  bool hasMouse = false;
  if (window && GetCursorPos(&mouse) && ScreenToClient(window, &mouse)) {
    hasMouse = true;
  }
  const float mx = hasMouse ? static_cast<float>(mouse.x) : -1000.0f;
  const float my = hasMouse ? static_cast<float>(mouse.y) : -1000.0f;

  std::array<HudPanel, kHudCount> panels{};
  for (std::size_t i = 0; i < panels.size(); ++i) {
    auto &panel = panels[i];
    panel.id = static_cast<HudId>(i);
    panel.layout = (g_draggedHud && *g_draggedHud == panel.id) ? g_dragLayout
                                                               : settings.hud[i];
    fillPanelLines(panel, snapshot, preview);
    switch (panel.id) {
    case HudId::EventTimer:
      panel.color = snapshot.timerUrgency >= 2     ? 0xFFFF5555
                    : snapshot.timerUrgency == 1   ? 0xFFFFAA00
                                                   : 0xFFFFFFFF;
      break;
    case HudId::Height:
      panel.color = snapshot.heightUrgency >= 2    ? 0xFFFF5555
                    : snapshot.heightUrgency == 1  ? 0xFFFFAA00
                                                   : 0xFFFFFFFF;
      break;
    case HudId::Resource: panel.color = 0xFFFFFFFF; break;
    case HudId::TeamState: panel.color = 0xFFFFFFFF; break;
    default: break;
    }
    measureAndClamp(panel, screenWidth, screenHeight);
  }

  updateDragging(panels, hdc, screenWidth, screenHeight, mx, my, cursorActive);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, screenWidth, screenHeight, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  for (const auto &panel : panels) {
    const auto index = static_cast<std::size_t>(panel.id);
    if (settings.hud[index].visible && !panel.lines.empty())
      drawPanel(panel, preview, cursorActive, mx, my);
  }

  glPopAttrib();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void shutdown() {
  g_layoutMode = false;
  g_draggedHud.reset();
  g_previousMouseDown = false;
  g_mcFont.shutdown();
}

} // namespace Render::BedwarsOverlay
