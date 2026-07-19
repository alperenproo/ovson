#pragma once
#include <Windows.h>

namespace Render {

struct TabCtx {
  HWND  hwnd;
  float mainX;   // panel top-left X
  float mainY;   // panel top-left Y
  float cx;      // content cursor X (anchor for left-aligned widgets)
  float startCy; // first content cursor Y of this frame (for scroll calc)
  float &cy;     // mutable cursor Y; tabs advance it as they draw rows
  float mx;      // mouse X
  float my;      // mouse Y
  bool  lClick;  // left mouse currently held
  bool  clickEvent; // edge-detected click (true for one frame)
  float alpha;   // effective alpha for this content (animAlpha*contentAlpha)
};

namespace Tabs {

void renderVisuals (TabCtx &ctx);  // tab 0
void renderPlayers (TabCtx &ctx);  // tab 1
void renderTags    (TabCtx &ctx);  // tab 2
void renderSettings(TabCtx &ctx);  // tab 3
void renderColors  (TabCtx &ctx);  // tab 4
void renderDebug   (TabCtx &ctx);  // tab 5
void renderUtils   (TabCtx &ctx);  // tab 6
void renderPlugins (TabCtx &ctx);  // tab 7

} // namespace Tabs
} // namespace Render
