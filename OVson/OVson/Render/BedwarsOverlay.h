#pragma once

namespace Render::BedwarsOverlay {

void render(void *hdc, int screenWidth, int screenHeight);
void setLayoutMode(bool enabled);
bool isLayoutMode();
void shutdown();

} // namespace Render::BedwarsOverlay
