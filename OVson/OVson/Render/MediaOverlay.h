#pragma once

namespace Render::MediaOverlay {

// Draws the now-playing card. Reads a snapshot the media worker published and
// never touches WinRT itself.
void render(void *hdc, int screenWidth, int screenHeight);
void shutdown();

} // namespace Render::MediaOverlay
