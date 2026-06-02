#pragma once
#include "../Config/Config.h"
#include <Windows.h>
#include <cstdint>
#include <string>

namespace Render {
namespace ClickGUITheme {

enum class Style { LiquidGlass, Minimal };

inline Style style() {
  const std::string &s = Config::getClickGuiTheme();
  if (s == "Minimal") return Style::Minimal;
  return Style::LiquidGlass; // default + unknown fallback
}

inline DWORD accent() { return Config::getThemeColor(); }
#define THEME_NAVY (Render::ClickGUITheme::accent())

inline DWORD panelBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x2210131C; // very translucent panel
  case Style::Minimal:     return 0xF40B0B0E;
  }
  return 0xFF0D0D0F;
}
inline DWORD sidebarBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22161929; // translucent sidebar
  case Style::Minimal:     return 0xFF111114;
  }
  return 0xFF121214;
}
inline DWORD cardBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x301F2334; // translucent card background (18% opacity)
  case Style::Minimal:     return 0xFF16161A;
  }
  return 0xFF18181B;
}
inline DWORD cardHover() {
  switch (style()) {
  case Style::LiquidGlass: return 0x452A3148; // translucent hover card background (~27% opacity)
  case Style::Minimal:     return 0xFF1F1F24;
  }
  return 0xFF222226;
}
inline DWORD border() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22FFFFFF; // subtle white border
  case Style::Minimal:     return 0xFF202024;
  }
  return 0xFF252528;
}
inline DWORD textPrimary()   { return 0xFFFFFFFF; }
inline DWORD textSecondary() { return 0xFFA8A8B0; }
inline DWORD textMuted()     { return 0xFF707078; }

inline float panelRadius() {
  return style() == Style::LiquidGlass ? 18.0f : 10.0f;
}
inline float cardRadius() {
  return style() == Style::LiquidGlass ? 14.0f : 8.0f;
}
inline float buttonRadius() {
  return style() == Style::LiquidGlass ? 10.0f : 6.0f;
}
inline float shadowSpread() {
  return style() == Style::LiquidGlass ? 20.0f : 6.0f;
}
inline bool hasGlassDecorations() { return style() == Style::LiquidGlass; }
inline bool hasAnimatedBackground() { return style() == Style::Minimal; }

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline uint32_t applyAlpha(uint32_t color, float alpha) {
  uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * alpha);
  return (uint32_t)((a << 24) | (color & 0x00FFFFFF));
}

inline bool isHovered(float mx, float my, float x, float y, float w, float h) {
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

inline float ease(float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return t * t * (3.0f - 2.0f * t);
}

inline float easeOut(float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  float u = 1.0f - t;
  return 1.0f - u * u * u;
}

} // namespace ClickGUITheme
} // namespace Render

#define THEME_BG         (Render::ClickGUITheme::panelBg())
#define THEME_SIDEBAR    (Render::ClickGUITheme::sidebarBg())
#define THEME_CARD       (Render::ClickGUITheme::cardBg())
#define THEME_CARD_HOVER (Render::ClickGUITheme::cardHover())
#define THEME_BORDER     (Render::ClickGUITheme::border())

namespace Render {
using ClickGUITheme::lerp;
using ClickGUITheme::applyAlpha;
using ClickGUITheme::isHovered;
}
