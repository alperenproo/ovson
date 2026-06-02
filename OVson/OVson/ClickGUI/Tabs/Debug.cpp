#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Config/Config.h"
#include "../../Logic/StatsTracker.h"
#include <gl/GL.h>
#include <string>

namespace Render {
namespace Tabs {

void renderDebug(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Debug Console Settings",
                       applyAlpha(0xFFFFFFFF, alpha));
  cy += 40;

  bool dbgGlobal = Config::isGlobalDebugEnabled();
  bool hDbgCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);

  glDisable(GL_TEXTURE_2D);
  drawThemeCard(mainX + 190, cy - 10, g_w - 210, 60, hDbgCard, alpha);
  glEnable(GL_TEXTURE_2D);

  g_guiFont.drawString(cx, cy, "Master Debug Switch",
                       applyAlpha(0xFFFFFFFF, alpha));
  g_guiFont.drawString(cx, cy + 18, "Master toggle for all client debug logs",
                       applyAlpha(0xFFA0A0A5, alpha));

  float dbgSwX = mainX + g_w - 65;
  glDisable(GL_TEXTURE_2D);
  drawSwitch(5, dbgSwX, cy, dbgGlobal, hDbgCard, alpha);
  glEnable(GL_TEXTURE_2D);

  if (clickEvent && hDbgCard)
    Config::setGlobalDebugEnabled(!dbgGlobal);

  cy += 75;

  if (Config::isGlobalDebugEnabled()) {
    auto renderDebugToggle = [&](const char *title, int id,
                                 Config::DebugCategory cat) {
      bool enabled = Config::isDebugEnabled(cat);
      bool hov = isHovered(mx, my, cx, cy - 5, 240, 30);

      g_guiFont.drawString(cx, cy, title,
                           applyAlpha(hov ? 0xFFFFFFFF : 0xFF808085, alpha));
      float toggleX = cx + 180;

      glDisable(GL_TEXTURE_2D);
      drawSwitch(id, toggleX, cy - 5, enabled, hov, alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hov)
        Config::setDebugEnabled(cat, !enabled);
      cy += 35;
    };

    renderDebugToggle("Game Detection", 7,
                      Config::DebugCategory::GameDetection);
    renderDebugToggle("Bed Detection", 8,
                      Config::DebugCategory::BedDetection);
    renderDebugToggle("Urchin Service", 9, Config::DebugCategory::Urchin);
    renderDebugToggle("Bed Defense Sys", 10,
                      Config::DebugCategory::BedDefense);
    renderDebugToggle("GUI Internals", 11, Config::DebugCategory::GUI);
    renderDebugToggle("General / Other", 12, Config::DebugCategory::General);
    cy += 15;
  }

  g_guiFont.drawString(cx, cy, "Logs are sent to OutputDebugString",
                       applyAlpha(0xFF808085, alpha));
  cy += 20;
  g_guiFont.drawString(cx, cy, "Use DbgView to see live output.",
                       applyAlpha(0xFF808085, alpha));
  cy += 40;

  bool hTest = isHovered(mx, my, cx, cy, 200, 35);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, 200, 35, hTest, false, alpha);
  glEnable(GL_TEXTURE_2D);

  std::string testText = "SEND TEST TOAST";
  float testTextX = cx + (200.0f - g_guiFont.getStringWidth(testText)) / 2.0f;
  g_guiFont.drawString(testTextX, cy + 4, testText,
                       applyAlpha(0xFFFFFFFF, alpha));
  if (clickEvent && hTest) {
    NotificationManager::getInstance()->add(
        "System", "Toast notifications are working!",
        NotificationType::Success);
  }
  cy += 50;

  bool hClear = isHovered(mx, my, cx, cy, 200, 35);
  glDisable(GL_TEXTURE_2D);
  drawThemeButton(cx, cy, 200, 35, hClear, false, alpha);
  glEnable(GL_TEXTURE_2D);

  std::string clearText = "CLEAR CACHE";
  float clearTextX =
      cx + (200.0f - g_guiFont.getStringWidth(clearText)) / 2.0f;
  g_guiFont.drawString(clearTextX, cy + 4, clearText,
                       applyAlpha(0xFFFFFFFF, alpha));
  if (clickEvent && hClear) {
    OVson::clearAllCaches();
    NotificationManager::getInstance()->add(
        "System", "All caches cleared and reset!", NotificationType::Success);
  }
  cy += 50;
}

} // namespace Tabs
} // namespace Render
