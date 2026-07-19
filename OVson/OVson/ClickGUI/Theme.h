#pragma once
#include "../Config/Config.h"
#include <Windows.h>
#include <cstdint>
#include <string>

namespace Render {
namespace ClickGUITheme {

enum class Style { Solid, LiquidGlass, Minimal };

inline Style style() {
  const std::string &s = Config::getClickGuiTheme();
  if (s == "LiquidGlass") return Style::LiquidGlass;
  if (s == "Minimal")     return Style::Minimal;
  return Style::Solid;
}

inline DWORD accent() { return Config::getThemeColor() | 0xFF000000; }
#define THEME_NAVY (Render::ClickGUITheme::accent())

inline DWORD panelBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x2210131C;
  case Style::Minimal:     return 0xF40B0B0E;
  default:                 return 0xEB0E1119;
  }
}
inline DWORD sidebarBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22161929;
  case Style::Minimal:     return 0xFF111114;
  default:                 return 0xF20B0E15;
  }
}
inline DWORD cardBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x301F2334;
  case Style::Minimal:     return 0xFF16161A;
  default:                 return 0x0AFFFFFF;
  }
}
inline DWORD cardHover() {
  switch (style()) {
  case Style::LiquidGlass: return 0x452A3148;
  case Style::Minimal:     return 0xFF1F1F24;
  default:                 return 0x12FFFFFF;
  }
}
inline DWORD border() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22FFFFFF;
  case Style::Minimal:     return 0xFF202024;
  default:                 return 0x0CFFFFFF;
  }
}
inline DWORD textPrimary()   { return 0xFFFFFFFF; }
inline DWORD textSecondary() { return 0xFFA8A8B0; }
inline DWORD textMuted()     { return 0xFF707078; }

inline DWORD surface1()       { return 0x0AFFFFFF; }
inline DWORD surface2()       { return 0x12FFFFFF; }
inline DWORD hairline()       { return 0x09FFFFFF; }
inline DWORD hairlineStrong() { return 0x15FFFFFF; }
inline DWORD inset()          { return 0x47000000; }
inline DWORD online()         { return 0xFF3DDC84; }
inline DWORD danger()         { return 0xFFFF5A64; }

inline float panelRadius() {
  return style() == Style::LiquidGlass ? 18.0f : 12.0f;
}
inline float cardRadius() {
  return style() == Style::LiquidGlass ? 14.0f : 12.0f;
}
inline float buttonRadius() {
  return style() == Style::LiquidGlass ? 10.0f : 9.0f;
}
inline float controlRadius() { return buttonRadius(); }
inline float windowRadius()  { return 7.0f; }
inline float pillRadius()    { return 10.0f; }
inline float chipRadius()    { return 6.0f; }

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

inline DWORD accentSoft()   { return (accent() & 0x00FFFFFF) | 0x29000000; }
inline DWORD accentBorder() { return (accent() & 0x00FFFFFF) | 0x5C000000; }
inline DWORD accentGlow()   { return (accent() & 0x00FFFFFF) | 0x73000000; }

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
