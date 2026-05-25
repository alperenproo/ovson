#pragma once
#include <Windows.h>
#include <string>

namespace BetterTab {
void init();
void render(void *hdcPtr);
void drawStringWithShadow(const std::string &text, float x, float y,
                          uint32_t color);
int getStringWidth(const std::string &text);
} // namespace BetterTab
