#pragma once
namespace Render {
namespace ClickGUIHelpers {

void setMouseGrabbed(bool grabbed);

bool isIngame();

void drawSwitch(int id, float x, float y, bool enabled, bool hovered,
                float alpha);

bool drawSlider(int id, float x, float y, float w, float h, float &val, float minVal, float maxVal, float mx, float my, bool lClick, float alpha);

void drawThemePanel(float x, float y, float w, float h, float alpha);

void drawThemeSidebar(float x, float y, float w, float h, float alpha);

void drawThemeCard(float x, float y, float w, float h, bool hovered,
                   float alpha);

void drawThemeButton(float x, float y, float w, float h, bool hovered,
                     bool pressed, float alpha);

void drawThemeTabIndicator(float x, float y, float w, float h, float alpha);

void drawThemeBackground(float screenW, float screenH, float alpha);

} // namespace ClickGUIHelpers
} // namespace Render

namespace Render {
using ClickGUIHelpers::setMouseGrabbed;
using ClickGUIHelpers::isIngame;
using ClickGUIHelpers::drawSwitch;
using ClickGUIHelpers::drawSlider;
using ClickGUIHelpers::drawThemePanel;
using ClickGUIHelpers::drawThemeSidebar;
using ClickGUIHelpers::drawThemeCard;
using ClickGUIHelpers::drawThemeButton;
using ClickGUIHelpers::drawThemeTabIndicator;
using ClickGUIHelpers::drawThemeBackground;
}
