#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BedwarsOverlay.h"

#include "FontRenderer.h"
#include "../ClickGUI/ClickGUI.h"
#include "../Logic/Bedwars/BedwarsConfig.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/GL.h>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Render::BedwarsOverlay {
namespace {

using OVson::Bedwars::HudId;
using OVson::Bedwars::HudLayout;
using OVson::Bedwars::kHudCount;

FontRenderer g_font;
bool g_layoutMode = false;
std::optional<HudId> g_draggedHud;
// The layout being dragged, kept ACROSS frames.
//
// render() rebuilds every panel from the saved configuration at the top of
// each frame, so the position a drag had built up lived only until the next
// frame. On the frame the button came up, `down` was already false, the
// mid-drag update was skipped, and the panel that got written back to the
// configuration was the one just re-read from it -- the old position. That is
// why a dragged panel jumped straight back to where it started.
HudLayout g_dragLayout;
float g_dragOffsetX = 0.0F;
float g_dragOffsetY = 0.0F;
bool g_previousMouseDown = false;

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
    return {"Iron 48", "Gold 12", "Diamond 4", "Emerald 2"};
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
  const float scale = std::clamp(panel.layout.scale, 0.5F, 2.5F);
  float textWidth = 0.0F;
  for (const auto &line : panel.lines)
    textWidth = std::max(textWidth, g_font.getStringWidth(line) * scale);
  panel.width = std::max(44.0F, textWidth + 10.0F);
  panel.height = std::max(
      18.0F, 13.0F * scale * static_cast<float>(panel.lines.size()) + 6.0F);
  panel.layout = OVson::Bedwars::sanitizeHudLayout(
      panel.layout, panel.width / static_cast<float>(screenWidth),
      panel.height / static_cast<float>(screenHeight));
  panel.x = panel.layout.x * screenWidth;
  panel.y = panel.layout.y * screenHeight;
}

void drawPanel(const HudPanel &panel, bool preview) {
  if (preview) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.04F, 0.06F, 0.09F, 0.78F);
    glBegin(GL_QUADS);
    glVertex2f(panel.x - 3.0F, panel.y - 3.0F);
    glVertex2f(panel.x + panel.width, panel.y - 3.0F);
    glVertex2f(panel.x + panel.width, panel.y + panel.height);
    glVertex2f(panel.x - 3.0F, panel.y + panel.height);
    glEnd();
    glColor4f(0.35F, 0.72F, 1.0F, 0.9F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(panel.x - 3.0F, panel.y - 3.0F);
    glVertex2f(panel.x + panel.width, panel.y - 3.0F);
    glVertex2f(panel.x + panel.width, panel.y + panel.height);
    glVertex2f(panel.x - 3.0F, panel.y + panel.height);
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }

  float cursor = panel.y;
  const float scale = std::clamp(panel.layout.scale, 0.5F, 2.5F);
  for (const auto &line : panel.lines) {
    g_font.drawString(panel.x, cursor, line, panel.color, 0.5F * scale);
    cursor += 13.0F * scale;
  }
}

bool contains(const HudPanel &panel, float x, float y) {
  return x >= panel.x - 3.0F && x <= panel.x + panel.width &&
         y >= panel.y - 3.0F && y <= panel.y + panel.height;
}

void updateDragging(std::array<HudPanel, kHudCount> &panels, HDC hdc,
                    int screenWidth, int screenHeight) {
  if (!g_layoutMode || !Render::ClickGUI::isOpen()) {
    g_draggedHud.reset();
    g_previousMouseDown = false;
    return;
  }
  POINT mouse{};
  HWND window = WindowFromDC(hdc);
  if (!window || !GetCursorPos(&mouse) || !ScreenToClient(window, &mouse))
    return;
  const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  if (down && !g_previousMouseDown) {
    for (auto it = panels.rbegin(); it != panels.rend(); ++it) {
      if (contains(*it, static_cast<float>(mouse.x),
                   static_cast<float>(mouse.y))) {
        g_draggedHud = it->id;
        g_dragLayout = it->layout;
        g_dragOffsetX = static_cast<float>(mouse.x) - it->x;
        g_dragOffsetY = static_cast<float>(mouse.y) - it->y;
        break;
      }
    }
  }
  if (down && g_draggedHud) {
    auto &panel = panels[static_cast<std::size_t>(*g_draggedHud)];
    panel.layout.x = (static_cast<float>(mouse.x) - g_dragOffsetX) /
                     static_cast<float>(screenWidth);
    panel.layout.y = (static_cast<float>(mouse.y) - g_dragOffsetY) /
                     static_cast<float>(screenHeight);
    measureAndClamp(panel, screenWidth, screenHeight);
    // Remember it outside the frame-local array, which is thrown away and
    // rebuilt from the configuration before the next frame gets here.
    g_dragLayout = panel.layout;
  }
  if (!down && g_previousMouseDown && g_draggedHud) {
    // Commit the remembered layout, not whatever the rebuild just put in the
    // array. On this frame the array holds the pre-drag position.
    OVson::Bedwars::Configuration::setHudLayout(*g_draggedHud, g_dragLayout);
    g_draggedHud.reset();
  }
  g_previousMouseDown = down;
}

} // namespace

void setLayoutMode(bool enabled) {
  g_layoutMode = enabled;
  if (!enabled) {
    // Leaving layout mode mid-drag keeps what was dragged so far rather than
    // silently discarding it.
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
  const bool preview = g_layoutMode && Render::ClickGUI::isOpen();
  const auto snapshot = OVson::Bedwars::Runtime::instance().snapshot();
  const auto settings = OVson::Bedwars::Configuration::get();
  if ((!snapshot.active || !settings.masterEnabled) && !preview)
    return;
  if (!g_font.isInitialized() && !g_font.init(hdc))
    return;

  std::array<HudPanel, kHudCount> panels{};
  for (std::size_t i = 0; i < panels.size(); ++i) {
    auto &panel = panels[i];
    panel.id = static_cast<HudId>(i);
    // A drag in progress is the live truth for that one panel; everything else
    // comes from the saved configuration.
    panel.layout = (g_draggedHud && *g_draggedHud == panel.id) ? g_dragLayout
                                                               : settings.hud[i];
    fillPanelLines(panel, snapshot, preview);
    switch (panel.id) {
    case HudId::EventTimer:
      panel.color = snapshot.timerUrgency >= 2     ? 0xFFFF6B6B
                    : snapshot.timerUrgency == 1   ? 0xFFFFD166
                                                   : 0xFFFFFFFF;
      break;
    case HudId::Height:
      panel.color = snapshot.heightUrgency >= 2    ? 0xFFFF6B6B
                    : snapshot.heightUrgency == 1  ? 0xFFFFD166
                                                   : 0xFFE7F3FF;
      break;
    case HudId::Resource: panel.color = 0xFFFFE9B5; break;
    case HudId::TeamState: panel.color = 0xFFCDE8FF; break;
    default: break;
    }
    measureAndClamp(panel, screenWidth, screenHeight);
  }

  updateDragging(panels, hdc, screenWidth, screenHeight);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, screenWidth, screenHeight, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT |
               GL_CURRENT_BIT | GL_LINE_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  for (const auto &panel : panels) {
    const auto index = static_cast<std::size_t>(panel.id);
    if ((preview || settings.hud[index].visible) && !panel.lines.empty())
      drawPanel(panel, preview);
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
}

} // namespace Render::BedwarsOverlay
