#pragma once
#include <Windows.h>
#include <string>

namespace BetterTab {
void init();
void render(void *hdcPtr);
void drawStringWithShadow(const std::string &text, float x, float y,
                          uint32_t color);
int getStringWidth(const std::string &text);

bool isResizeMode();
void setResizeMode(bool resize);
void handleMouseClick(int btn, int state, int x, int y);
void handleMouseMove(int x, int y);
} // namespace BetterTab
