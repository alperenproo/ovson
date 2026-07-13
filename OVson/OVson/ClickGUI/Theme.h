#pragma once
#include "../Config/Config.h"
#include <Windows.h>
#include <cstdint>
#include <string>

// ── OVSON ClickGUI theme tokens (design spec v3) ───────────────────
// Single SOLID theme (no glass/blur). Colours are ARGB DWORDs here; the
// renderer converts to glColor4f. Token values come straight from the
// spec (§1). LiquidGlass / Minimal are kept as legacy fallbacks so older
// configs still load, but Solid is the default.
namespace Render {
namespace ClickGUITheme {

enum class Style { Solid, LiquidGlass, Minimal };

inline Style style() {
  const std::string &s = Config::getClickGuiTheme();
  if (s == "LiquidGlass") return Style::LiquidGlass;
  if (s == "Minimal")     return Style::Minimal;
  return Style::Solid; // "Solid" + default
}

// Accent: user-selected; spec default navy #3D6EF5. Stored ARGB in
// config; force full alpha so derivatives are predictable.
inline DWORD accent() { return Config::getThemeColor() | 0xFF000000; }
#define THEME_NAVY (Render::ClickGUITheme::accent())

// ── Neutral / surface tokens (§1.1) ──
inline DWORD panelBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x2210131C;
  case Style::Minimal:     return 0xF40B0B0E;
  default:                 return 0xEB0E1119; // #0E1119 @ a=0.92
  }
}
inline DWORD sidebarBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22161929;
  case Style::Minimal:     return 0xFF111114;
  default:                 return 0xF20B0E15; // #0B0E15 @ a=0.95
  }
}
inline DWORD cardBg() {
  switch (style()) {
  case Style::LiquidGlass: return 0x301F2334;
  case Style::Minimal:     return 0xFF16161A;
  default:                 return 0x0AFFFFFF; // surface-1: white @ 4%
  }
}
inline DWORD cardHover() {
  switch (style()) {
  case Style::LiquidGlass: return 0x452A3148;
  case Style::Minimal:     return 0xFF1F1F24;
  default:                 return 0x12FFFFFF; // surface-2: white @ 7%
  }
}
inline DWORD border() {
  switch (style()) {
  case Style::LiquidGlass: return 0x22FFFFFF;
  case Style::Minimal:     return 0xFF202024;
  default:                 return 0x0CFFFFFF; // hairline: thin
  }
}
inline DWORD textPrimary()   { return 0xFFFFFFFF; }              // #FFFFFF
inline DWORD textSecondary() { return 0xFFA8A8B0; }              // dim
inline DWORD textMuted()     { return 0xFF707078; }              // faint

// Extra spec tokens (style-independent solid values).
inline DWORD surface1()       { return 0x0AFFFFFF; } // white @ 4%
inline DWORD surface2()       { return 0x12FFFFFF; } // white @ 7%
inline DWORD hairline()       { return 0x09FFFFFF; } // white @ ~3.5% (very thin)
inline DWORD hairlineStrong() { return 0x15FFFFFF; } // white @ ~8%
inline DWORD inset()          { return 0x47000000; } // black @ 28%
inline DWORD online()         { return 0xFF3DDC84; } // #3DDC84
inline DWORD danger()         { return 0xFFFF5A64; } // #FF5A64

// ── Radii (§1.3) ──
inline float panelRadius() {
  return style() == Style::LiquidGlass ? 18.0f : 12.0f;
}
inline float cardRadius() {
  return style() == Style::LiquidGlass ? 14.0f : 12.0f;
}
inline float buttonRadius() {           // a.k.a. control radius
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

// ── Accent derivatives (§1.2): same RGB, fixed alpha ──
inline DWORD accentSoft()   { return (accent() & 0x00FFFFFF) | 0x29000000; } // a=0.16
inline DWORD accentBorder() { return (accent() & 0x00FFFFFF) | 0x5C000000; } // a=0.36
inline DWORD accentGlow()   { return (accent() & 0x00FFFFFF) | 0x73000000; } // a=0.45

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
