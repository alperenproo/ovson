#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <cstdint>
namespace Render {
namespace ClickGUIHelpers {

void drawSectionLabel(float x, float y, const std::string &text, float alpha);

void drawChevron(float cx, float cy, float s, bool open, uint32_t col,
                 float alpha);

void setMouseGrabbed(bool grabbed);

bool isIngame();

void drawSwitch(int id, float x, float y, bool enabled, bool hovered,
                float alpha);

bool drawSlider(int id, float x, float y, float w, float h, float &val, float minVal, float maxVal, float mx, float my, bool lClick, float alpha);

// Shared click-to-type readout for slider-backed values. The caller supplies
// exactly the same bounds used by its slider, so typed and dragged values are
// clamped identically.
bool drawNumericInput(int id, float x, float y, float w, float h, float &value,
                      float minValue, float maxValue, int decimals,
                      const char *suffix, float mx, float my, bool clickEvent,
                      float alpha);
bool handleEditorMessage(UINT msg, WPARAM wParam, LPARAM lParam);
void cancelInlineEditors();

float colorPickerHeight(bool includeRainbow = false);
bool drawColorPicker(int id, float x, float y, float width,
                     std::uint32_t &color, float mx, float my, bool lClick,
                     bool clickEvent, float alpha, bool *rainbow = nullptr);

void drawThemePanel(float x, float y, float w, float h, float alpha);

void drawThemeSidebar(float x, float y, float w, float h, float alpha);

void drawThemeCard(float x, float y, float w, float h, bool hovered,
                   float alpha, bool active = false);

void drawThemeButton(float x, float y, float w, float h, bool hovered,
                     bool pressed, float alpha);

void drawTextInput(float x, float y, float w, float h, bool focused,
                   bool hovered, float alpha);

void drawThemeTabIndicator(float x, float y, float w, float h, float alpha);

void drawThemeBackground(float screenW, float screenH, float alpha);

} // namespace ClickGUIHelpers
} // namespace Render

namespace Render {
using ClickGUIHelpers::setMouseGrabbed;
using ClickGUIHelpers::isIngame;
using ClickGUIHelpers::drawSectionLabel;
using ClickGUIHelpers::drawChevron;
using ClickGUIHelpers::drawSwitch;
using ClickGUIHelpers::drawSlider;
using ClickGUIHelpers::drawNumericInput;
using ClickGUIHelpers::drawColorPicker;
using ClickGUIHelpers::colorPickerHeight;
using ClickGUIHelpers::drawThemePanel;
using ClickGUIHelpers::drawThemeSidebar;
using ClickGUIHelpers::drawThemeCard;
using ClickGUIHelpers::drawThemeButton;
using ClickGUIHelpers::drawTextInput;
using ClickGUIHelpers::drawThemeTabIndicator;
using ClickGUIHelpers::drawThemeBackground;
}
