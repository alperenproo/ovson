#pragma once
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
using ClickGUIHelpers::drawThemePanel;
using ClickGUIHelpers::drawThemeSidebar;
using ClickGUIHelpers::drawThemeCard;
using ClickGUIHelpers::drawThemeButton;
using ClickGUIHelpers::drawTextInput;
using ClickGUIHelpers::drawThemeTabIndicator;
using ClickGUIHelpers::drawThemeBackground;
}
