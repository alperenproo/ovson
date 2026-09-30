#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "MediaOverlay.h"

#include "RenderUtils.h"
#include "../ClickGUI/ClickGUI.h"
#include "../ClickGUI/Helpers.h"
#include "../Config/Config.h"
#include "../Logic/MediaSession.h"
#include "../Utils/stb_image.h"
#include "../resource.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <gl/GL.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render::MediaOverlay {
namespace {

namespace Media = OVson::Media;

inline std::u32string utf8ToUtf32(const std::string &str) {
  std::u32string result;
  result.reserve(str.size());
  size_t i = 0;
  while (i < str.size()) {
    unsigned char c = static_cast<unsigned char>(str[i]);
    char32_t cp = 0;
    size_t extra = 0;
    if (c <= 0x7F) {
      cp = c;
      extra = 0;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      extra = 3;
    } else {
      i++;
      continue;
    }
    if (i + extra >= str.size()) break;
    bool valid = true;
    for (size_t j = 1; j <= extra; ++j) {
      unsigned char next = static_cast<unsigned char>(str[i + j]);
      if ((next & 0xC0) != 0x80) { valid = false; break; }
      cp = (cp << 6) | (next & 0x3F);
    }
    if (valid) result.push_back(cp);
    i += 1 + extra;
  }
  return result;
}

inline std::string utf32ToUtf8(const std::u32string &u32) {
  std::string result;
  result.reserve(u32.size() * 3);
  for (char32_t cp : u32) {
    if (cp <= 0x7F) {
      result.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      result.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
      result.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return result;
}

constexpr int kAtlasW = 2048;
constexpr int kAtlasH = 4096;
constexpr int kCell = 96;
constexpr int kPerRow = kAtlasW / kCell;
constexpr int kInset = 16;
constexpr int kEm = 48;

const char *const kFaceCandidates[] = {
    "Segoe UI", "Segoe UI Variable Display", "Montserrat", "Poppins",
    "Arial"};

bool faceExists(HDC dc, const char *face) {
  LOGFONTA request{};
  request.lfCharSet = DEFAULT_CHARSET;
  strncpy_s(request.lfFaceName, face, _TRUNCATE);
  bool found = false;
  EnumFontFamiliesExA(
      dc, &request,
      [](const LOGFONTA *, const TEXTMETRICA *, DWORD, LPARAM user) -> int {
        *reinterpret_cast<bool *>(user) = true;
        return 0;
      },
      reinterpret_cast<LPARAM>(&found), 0);
  return found;
}

struct GlyphMetrics {
  int col = 0;
  int row = 0;
  float width = 0.0f;
};

class CardFont {
public:
  bool init(HDC hdc, int weight) {
    if (m_initialized) return true;
    if (m_failed || !hdc) return false;

    HDC dc = CreateCompatibleDC(hdc);
    if (!dc) { m_failed = true; return false; }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kAtlasW;
    info.bmiHeader.biHeight = -kAtlasH; // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP bitmap =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
      DeleteDC(dc);
      m_failed = true;
      return false;
    }
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(dc, bitmap));

    const char *face = "Segoe UI";
    for (const char *candidate : kFaceCandidates) {
      if (faceExists(dc, candidate)) { face = candidate; break; }
    }

    HFONT font = CreateFontA(-kEm, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FF_SWISS | DEFAULT_PITCH,
                             face);
    if (!font) {
      SelectObject(dc, oldBitmap);
      DeleteObject(bitmap);
      DeleteDC(dc);
      m_failed = true;
      return false;
    }
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));

    std::memset(bits, 0xFF, static_cast<size_t>(kAtlasW) * kAtlasH * 4);
    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, RGB(255, 255, 255));
    SetTextColor(dc, RGB(0, 0, 0));
    SetTextAlign(dc, TA_TOP | TA_LEFT);

    std::vector<char32_t> codePoints;
    codePoints.reserve(1000);
    for (char32_t cp = 32; cp <= 126; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 160; cp <= 255; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 256; cp <= 383; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x0180; cp <= 0x021F; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x0400; cp <= 0x045F; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x2010; cp <= 0x203A; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x20A0; cp <= 0x20CF; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x2190; cp <= 0x2199; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x25A0; cp <= 0x25FF; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x2600; cp <= 0x266F; ++cp) codePoints.push_back(cp);
    for (char32_t cp = 0x2700; cp <= 0x2775; ++cp) codePoints.push_back(cp);

    m_glyphs.clear();
    int slot = 0;
    const int maxSlots = kPerRow * (kAtlasH / kCell);

    for (char32_t cp : codePoints) {
      if (slot >= maxSlots) break;
      wchar_t wstr[3]{};
      int wlen = 0;
      if (cp <= 0xFFFF) {
        wstr[0] = static_cast<wchar_t>(cp);
        wlen = 1;
      } else {
        wstr[0] = static_cast<wchar_t>(0xD800 + ((cp - 0x10000) >> 10));
        wstr[1] = static_cast<wchar_t>(0xDC00 + ((cp - 0x10000) & 0x3FF));
        wlen = 2;
      }

      SIZE size{};
      GetTextExtentPoint32W(dc, wstr, wlen, &size);
      const int column = slot % kPerRow;
      const int row = slot / kPerRow;
      TextOutW(dc, column * kCell + kInset, row * kCell + kInset, wstr, wlen);

      GlyphMetrics m;
      m.col = column;
      m.row = row;
      m.width = static_cast<float>(size.cx);
      m_glyphs[cp] = m;
      slot++;
    }
    GdiFlush();

    std::vector<unsigned char> pixels(
        static_cast<size_t>(kAtlasW) * kAtlasH * 4);
    const unsigned char *source = static_cast<const unsigned char *>(bits);
    for (size_t i = 0; i < static_cast<size_t>(kAtlasW) * kAtlasH; ++i) {
      const unsigned char b = source[i * 4 + 0];
      const unsigned char g = source[i * 4 + 1];
      const unsigned char r = source[i * 4 + 2];
      const int luminance = (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
      pixels[i * 4 + 0] = 255;
      pixels[i * 4 + 1] = 255;
      pixels[i * 4 + 2] = 255;
      pixels[i * 4 + 3] = static_cast<unsigned char>(255 - luminance);
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasW, kAtlasH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);

    m_initialized = true;
    return true;
  }

  void release() {
    if (m_texture) glDeleteTextures(1, &m_texture);
    m_texture = 0;
    m_glyphs.clear();
    m_initialized = false;
    m_failed = false;
  }

  bool ready() const { return m_initialized; }

  float width(const std::string &text, float em) const {
    if (text.empty()) return 0.0F;
    const float k = em / static_cast<float>(kEm);
    float total = 0.0F;
    const std::u32string u32 = utf8ToUtf32(text);
    for (char32_t cp : u32) {
      auto it = m_glyphs.find(cp);
      if (it != m_glyphs.end()) {
        total += (it->second.width + 1.0F) * k;
      } else {
        auto spaceIt = m_glyphs.find(U' ');
        float w = spaceIt != m_glyphs.end() ? spaceIt->second.width : 16.0F;
        total += (w + 1.0F) * k;
      }
    }
    return total;
  }

  void draw(float x, float top, const std::string &text, DWORD color,
            float em) const {
    if (!m_initialized || text.empty()) return;
    const float k = em / static_cast<float>(kEm);
    const float r = ((color >> 16) & 0xFF) / 255.0F;
    const float g = ((color >> 8) & 0xFF) / 255.0F;
    const float b = (color & 0xFF) / 255.0F;
    float a = ((color >> 24) & 0xFF) / 255.0F;
    if (a == 0.0F) a = 1.0F;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glColor4f(r, g, b, a);

    const float boxTop = top - kInset * k;
    float cursor = x - kInset * k;
    glBegin(GL_QUADS);
    const std::u32string u32 = utf8ToUtf32(text);
    for (char32_t cp : u32) {
      auto it = m_glyphs.find(cp);
      if (it == m_glyphs.end()) {
        auto spaceIt = m_glyphs.find(U' ');
        float w = spaceIt != m_glyphs.end() ? spaceIt->second.width : 16.0F;
        cursor += (w + 1.0F) * k;
        continue;
      }
      const auto &glyph = it->second;
      const float glyphW = glyph.width;
      const float cellW = (std::min)(static_cast<float>(kCell), glyphW + kInset * 2.0F);
      const float quadW = cellW * k;
      const float quadH = kCell * k;
      const float u0 = (glyph.col * kCell) / static_cast<float>(kAtlasW);
      const float u1 = (glyph.col * kCell + cellW) / static_cast<float>(kAtlasW);
      const float v0 = (glyph.row * kCell) / static_cast<float>(kAtlasH);
      const float v1 = (glyph.row * kCell + kCell) / static_cast<float>(kAtlasH);
      glTexCoord2f(u0, v0); glVertex2f(cursor, boxTop);
      glTexCoord2f(u1, v0); glVertex2f(cursor + quadW, boxTop);
      glTexCoord2f(u1, v1); glVertex2f(cursor + quadW, boxTop + quadH);
      glTexCoord2f(u0, v1); glVertex2f(cursor, boxTop + quadH);
      cursor += (glyphW + 1.0F) * k;
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
  }

private:
  bool m_initialized = false;
  bool m_failed = false;
  unsigned int m_texture = 0;
  std::unordered_map<char32_t, GlyphMetrics> m_glyphs;
};

CardFont g_bold;
CardFont g_regular;

unsigned int g_artTexture = 0;
unsigned long long g_artVersion = 0;
unsigned int g_shuffleTexture = 0;
unsigned int g_repeatTexture = 0;
bool g_controlIconsAttempted = false;
bool g_previousMouseDown = false;
bool g_dragging = false;
float g_dragOffsetX = 0.0F;
float g_dragOffsetY = 0.0F;
bool g_seeking = false;
std::uint64_t g_lastSeekSend = 0;

constexpr float kWideCardW = 292.0F;
constexpr float kWideCardH = 108.0F;
constexpr float kCompactCardW = 148.0F;
constexpr float kCompactCardH = 212.0F;
constexpr float kCompactNoArtH = 100.0F;

void quad(float x0, float y0, float x1, float y1) {
  glBegin(GL_QUADS);
  glVertex2f(x0, y0);
  glVertex2f(x1, y0);
  glVertex2f(x1, y1);
  glVertex2f(x0, y1);
  glEnd();
}

void tri(float ax, float ay, float bx, float by, float cx, float cy) {
  glBegin(GL_TRIANGLES);
  glVertex2f(ax, ay);
  glVertex2f(bx, by);
  glVertex2f(cx, cy);
  glEnd();
}

void stroke(float x0, float y0, float x1, float y1, float thickness) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.0001F) return;
  const float nx = -dy / length * thickness * 0.5F;
  const float ny = dx / length * thickness * 0.5F;
  glBegin(GL_QUADS);
  glVertex2f(x0 + nx, y0 + ny);
  glVertex2f(x1 + nx, y1 + ny);
  glVertex2f(x1 - nx, y1 - ny);
  glVertex2f(x0 - nx, y0 - ny);
  glEnd();
}

void arc(float cx, float cy, float radius, float thickness, float from,
         float to, int segments = 24) {
  const float half = thickness * 0.5F;
  glBegin(GL_QUADS);
  for (int i = 0; i < segments; ++i) {
    const float a0 = from + (to - from) * (static_cast<float>(i) / segments);
    const float a1 =
        from + (to - from) * (static_cast<float>(i + 1) / segments);
    const float c0 = std::cos(a0), s0 = std::sin(a0);
    const float c1 = std::cos(a1), s1 = std::sin(a1);
    glVertex2f(cx + c0 * (radius - half), cy + s0 * (radius - half));
    glVertex2f(cx + c0 * (radius + half), cy + s0 * (radius + half));
    glVertex2f(cx + c1 * (radius + half), cy + s1 * (radius + half));
    glVertex2f(cx + c1 * (radius - half), cy + s1 * (radius - half));
  }
  glEnd();
}

void iconPlay(float cx, float cy, float s) {
  tri(cx - s * 0.44F, cy - s * 0.72F, cx - s * 0.44F, cy + s * 0.72F,
      cx + s * 0.68F, cy);
}

void iconPause(float cx, float cy, float s) {
  const float bw = s * 0.42F;
  const float bh = s * 1.35F;
  RenderUtils::drawRoundedRect(cx - s * 0.55F - bw * 0.5F, cy - bh * 0.5F, bw, bh, bw * 0.5F, 0xFF000000 | Config::getMediaOverlayBgColor(), 1.0F);
  RenderUtils::drawRoundedRect(cx + s * 0.55F - bw * 0.5F, cy - bh * 0.5F, bw, bh, bw * 0.5F, 0xFF000000 | Config::getMediaOverlayBgColor(), 1.0F);
}

void iconSkip(float cx, float cy, float s, bool forward) {
  const float d = forward ? 1.0F : -1.0F;
  tri(cx - d * s * 0.82F, cy - s * 0.72F, cx - d * s * 0.82F, cy + s * 0.72F,
      cx + d * s * 0.28F, cy);
  RenderUtils::drawRoundedRect(cx + d * s * 0.42F - s * 0.12F, cy - s * 0.72F, s * 0.24F, s * 1.44F, s * 0.12F, 0xFFFFFFFF, 1.0F);
}

void iconShuffle(float cx, float cy, float s, float thickness) {
  const float y = s * 0.58F;
  const float left = cx - s;
  const float bend = cx + s * 0.15F;
  const float head = cx + s * 0.95F;
  stroke(left, cy - y, bend, cy + y, thickness);
  stroke(left, cy + y, bend, cy - y, thickness);
  stroke(bend, cy + y, head - s * 0.3F, cy + y, thickness);
  stroke(bend, cy - y, head - s * 0.3F, cy - y, thickness);
  tri(head - s * 0.34F, cy + y - s * 0.36F, head - s * 0.34F,
      cy + y + s * 0.36F, head, cy + y);
  tri(head - s * 0.34F, cy - y - s * 0.36F, head - s * 0.34F,
      cy - y + s * 0.36F, head, cy - y);
}

void iconRepeat(float cx, float cy, float s, float thickness) {
  const float w = s * 0.92F;
  const float h = s * 0.62F;
  const float r = s * 0.34F;
  const float pi = 3.14159265F;
  stroke(cx - w + r, cy - h, cx + w - r, cy - h, thickness);
  stroke(cx - w + r, cy + h, cx + w - r * 0.2F, cy + h, thickness);
  stroke(cx - w, cy - h + r, cx - w, cy + h - r, thickness);
  stroke(cx + w, cy - h + r, cx + w, cy + h - r, thickness);
  arc(cx - w + r, cy - h + r, r, thickness, pi, pi * 1.5F, 24);
  arc(cx + w - r, cy - h + r, r, thickness, pi * 1.5F, pi * 2.0F, 24);
  arc(cx + w - r, cy + h - r, r, thickness, 0.0F, pi * 0.5F, 24);
  arc(cx - w + r, cy + h - r, r, thickness, pi * 0.5F, pi, 24);
  tri(cx - w + r * 0.4F, cy + h - s * 0.4F, cx - w + r * 0.4F,
      cy + h + s * 0.4F, cx - w - r * 0.5F, cy + h);
}

std::string elide(const CardFont &font, const std::string &text,
                  float maximumWidth, float em) {
  if (text.empty()) return text;
  if (font.width(text, em) <= maximumWidth) return text;
  std::u32string u32 = utf8ToUtf32(text);
  while (u32.size() > 1) {
    u32.pop_back();
    std::string testStr = utf32ToUtf8(u32) + "...";
    if (font.width(testStr, em) <= maximumWidth) {
      return testStr;
    }
  }
  return utf32ToUtf8(u32) + "...";
}

std::string formatTime(std::int64_t milliseconds) {
  const std::int64_t totalSeconds = std::max<std::int64_t>(0, milliseconds) / 1000;
  const std::int64_t seconds = totalSeconds % 60;
  const std::int64_t totalMinutes = totalSeconds / 60;
  char text[24]{};
  if (totalMinutes >= 60) {
    std::snprintf(text, sizeof(text), "%lld:%02lld:%02lld",
                  static_cast<long long>(totalMinutes / 60),
                  static_cast<long long>(totalMinutes % 60),
                  static_cast<long long>(seconds));
  } else {
    std::snprintf(text, sizeof(text), "%lld:%02lld",
                  static_cast<long long>(totalMinutes),
                  static_cast<long long>(seconds));
  }
  return text;
}

unsigned int loadMaskedControlIcon(int resourceId) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&g_artTexture), &module))
    return 0;
  const HRSRC resource =
      FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
  if (!resource) return 0;
  const HGLOBAL loaded = LoadResource(module, resource);
  const void *encoded = loaded ? LockResource(loaded) : nullptr;
  const DWORD encodedSize = SizeofResource(module, resource);
  if (!encoded || encodedSize == 0) return 0;

  int width = 0;
  int height = 0;
  int channels = 0;
  unsigned char *source = stbi_load_from_memory(
      static_cast<const stbi_uc *>(encoded), static_cast<int>(encodedSize),
      &width, &height, &channels, 4);
  if (!source || width <= 0 || height <= 0) {
    if (source) stbi_image_free(source);
    return 0;
  }

  int left = width;
  int top = height;
  int right = -1;
  int bottom = -1;
  for (int py = 0; py < height; ++py) {
    for (int px = 0; px < width; ++px) {
      const unsigned char *pixel = source + (py * width + px) * 4;
      const int luminance =
          (static_cast<int>(pixel[0]) + pixel[1] + pixel[2]) / 3;
      if (luminance >= 128) continue;
      left = std::min(left, px);
      top = std::min(top, py);
      right = std::max(right, px);
      bottom = std::max(bottom, py);
    }
  }
  if (right < left || bottom < top) {
    stbi_image_free(source);
    return 0;
  }
  const int cropW = right - left + 1;
  const int cropH = bottom - top + 1;
  std::vector<unsigned char> mask(static_cast<std::size_t>(cropW) * cropH * 4);
  for (int py = 0; py < cropH; ++py) {
    for (int px = 0; px < cropW; ++px) {
      const unsigned char *pixel =
          source + ((top + py) * width + left + px) * 4;
      const int luminance =
          (static_cast<int>(pixel[0]) + pixel[1] + pixel[2]) / 3;
      const int alpha = luminance >= 190 ? 0 : (190 - luminance) * 255 / 190;
      unsigned char *out = mask.data() + (py * cropW + px) * 4;
      out[0] = 255;
      out[1] = 255;
      out[2] = 255;
      out[3] = static_cast<unsigned char>(std::clamp(alpha, 0, 255));
    }
  }
  stbi_image_free(source);

  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cropW, cropH, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, mask.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  return texture;
}

void ensureControlIcons() {
  if (g_controlIconsAttempted) return;
  g_controlIconsAttempted = true;
  g_shuffleTexture = loadMaskedControlIcon(IDR_MEDIA_SHUFFLE);
  g_repeatTexture = loadMaskedControlIcon(IDR_MEDIA_REPEAT);
}

bool drawControlIcon(unsigned int texture, float cx, float cy, float width,
                     float height) {
  if (texture == 0) return false;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0F, 0.0F); glVertex2f(cx - width * 0.5F, cy - height * 0.5F);
  glTexCoord2f(1.0F, 0.0F); glVertex2f(cx + width * 0.5F, cy - height * 0.5F);
  glTexCoord2f(1.0F, 1.0F); glVertex2f(cx + width * 0.5F, cy + height * 0.5F);
  glTexCoord2f(0.0F, 1.0F); glVertex2f(cx - width * 0.5F, cy + height * 0.5F);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  return true;
}

void adoptArtwork() {
  Media::Artwork art;
  if (!Media::takeArtwork(art)) return;
  if (art.rgba.empty() || art.width <= 0 || art.height <= 0) return;
  if (g_artTexture == 0) glGenTextures(1, &g_artTexture);
  glBindTexture(GL_TEXTURE_2D, g_artTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, art.width, art.height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, art.rgba.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  g_artVersion = art.version;
}

struct Button {
  float x = 0.0F;
  float half = 0.0F;
};

void colorFrom(DWORD rgb, float alpha) {
  glColor4f(((rgb >> 16) & 0xFF) / 255.0F, ((rgb >> 8) & 0xFF) / 255.0F,
            (rgb & 0xFF) / 255.0F, alpha);
}

} // namespace

void render(void *hdcValue, int screenWidth, int screenHeight) {
  if (!hdcValue || screenWidth <= 0 || screenHeight <= 0) return;
  static bool s_started = false;
  if (!Config::isMediaOverlayEnabled()) return;
  if (!s_started) {
    s_started = true;
    OVson::Media::start();
  }
  HDC hdc = static_cast<HDC>(hdcValue);

  const Media::Snapshot media = Media::snapshot();
  const bool guiOpen = Render::ClickGUI::isOpen();
  if (!media.valid && !guiOpen) return;

  g_bold.init(hdc, FW_BOLD);
  g_regular.init(hdc, FW_NORMAL);

  adoptArtwork();
  ensureControlIcons();

  const float scale = std::clamp(Config::getMediaOverlayScale(), 0.5F, 2.5F);
  const DWORD bgColor = Config::getMediaOverlayBgColor() & 0x00FFFFFF;
  const DWORD accentColor = Config::getMediaOverlayAccentColor() & 0x00FFFFFF;
  const DWORD textColor = Config::getMediaOverlayTextColor() & 0x00FFFFFF;
  const float opacity = std::clamp(Config::getMediaOverlayOpacity(), 0.0F, 1.0F);
  const bool showArt = Config::isMediaOverlayArtEnabled();
  const bool compact = Config::getMediaOverlayLayout() == 1;
  const float radius = std::clamp(Config::getMediaOverlayCorner(), 0.0F, 20.0F);

  const float baseCardW = compact ? kCompactCardW : kWideCardW;
  const float baseCardH = compact
                              ? (showArt ? kCompactCardH : kCompactNoArtH)
                              : kWideCardH;
  const float cardW = baseCardW * scale;
  const float cardH = baseCardH * scale;
  float x = Config::getMediaOverlayX() * screenWidth;
  float y = Config::getMediaOverlayY() * screenHeight;
  x = std::clamp(x, 2.0F, std::max(2.0F, screenWidth - cardW - 2.0F));
  y = std::clamp(y, 2.0F, std::max(2.0F, screenHeight - cardH - 2.0F));

  const float pad = (compact ? 10.0F : 10.0F) * scale;
  const float artSize = (compact ? 112.0F : 76.0F) * scale;
  const float artX = compact ? x + (cardW - artSize) * 0.5F : x + pad;
  const float artY = y + pad;
  const float textX = compact
                          ? x + 12.0F * scale
                          : (showArt ? artX + artSize + 12.0F * scale
                                     : x + 14.0F * scale);
  const float textW = compact ? cardW - 24.0F * scale
                              : cardW - (textX - x) - 12.0F * scale;
  const float titleTop = y + (compact ? (showArt ? 128.0F : 13.0F) : 15.0F) * scale;
  const float titleEm = (compact ? 13.0F : 15.0F) * scale;
  const float artistTop = y + (compact ? (showArt ? 145.0F : 31.0F) : 35.0F) * scale;
  const float artistEm = (compact ? 10.5F : 12.0F) * scale;
  const float trackY = y + (compact ? (showArt ? 164.0F : 52.0F) : 84.0F) * scale;
  const float trackH = 3.0F * scale;
  const float timeTop = trackY + 7.0F * scale;
  const float timeEm = 9.0F * scale;
  const float buttonY = y + (compact ? (showArt ? 198.0F : 86.0F) : 66.0F) * scale;

  const bool showShuffle = !compact;
  const bool showRepeat = !compact;
  const int buttonCount = 3 + (showShuffle ? 1 : 0) + (showRepeat ? 1 : 0);
  const float step = (compact ? 34.0F : 31.0F) * scale;
  const float halfHit = (compact ? 14.0F : 13.0F) * scale;
  float buttonCursor = textX + textW * 0.5F -
                       step * static_cast<float>(buttonCount - 1) * 0.5F;
  Button shuffleB{};
  Button repeatB{};
  if (showShuffle) {
    shuffleB = Button{buttonCursor, halfHit};
    buttonCursor += step;
  }
  Button prevB{buttonCursor, halfHit};
  buttonCursor += step;
  Button playB{buttonCursor, halfHit};
  buttonCursor += step;
  Button nextB{buttonCursor, halfHit};
  buttonCursor += step;
  if (showRepeat) repeatB = Button{buttonCursor, halfHit};

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, screenWidth, screenHeight, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT |
               GL_CURRENT_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);

  RenderUtils::drawRoundedRect(x, y, cardW, cardH, radius * scale,
                               0xFF000000 | bgColor, opacity);
  glDisable(GL_TEXTURE_2D);
  if (guiOpen) {
    colorFrom(accentColor, 0.85F);
    stroke(x, y, x + cardW, y, 1.0F);
    stroke(x + cardW, y, x + cardW, y + cardH, 1.0F);
    stroke(x + cardW, y + cardH, x, y + cardH, 1.0F);
    stroke(x, y + cardH, x, y, 1.0F);
  }

  if (showArt) {
    if (g_artTexture != 0 && media.valid) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, g_artTexture);
      glColor4f(1.0F, 1.0F, 1.0F, std::max(opacity, 0.55F));
      glBegin(GL_QUADS);
      glTexCoord2f(0, 0); glVertex2f(artX, artY);
      glTexCoord2f(1, 0); glVertex2f(artX + artSize, artY);
      glTexCoord2f(1, 1); glVertex2f(artX + artSize, artY + artSize);
      glTexCoord2f(0, 1); glVertex2f(artX, artY + artSize);
      glEnd();
      glBindTexture(GL_TEXTURE_2D, 0);
      glDisable(GL_TEXTURE_2D);
    } else {
      glColor4f(1.0F, 1.0F, 1.0F, opacity * 0.10F);
      quad(artX, artY, artX + artSize, artY + artSize);
      colorFrom(accentColor, opacity * 0.55F);
      const float placeholderInset = 6.0F * scale;
      stroke(artX + placeholderInset, artY + artSize * 0.62F,
             artX + artSize * 0.42F, artY + artSize * 0.40F,
             2.0F * scale);
      stroke(artX + artSize * 0.42F, artY + artSize * 0.40F,
             artX + artSize * 0.62F, artY + artSize * 0.58F,
             2.0F * scale);
      stroke(artX + artSize * 0.62F, artY + artSize * 0.58F,
             artX + artSize - placeholderInset, artY + artSize * 0.31F,
             2.0F * scale);
    }
  }

  float mx = -1.0F;
  float my = -1.0F;
  HWND window = WindowFromDC(hdc);
  const bool cursorActive =
      window != nullptr &&
      (guiOpen || !Render::ClickGUIHelpers::isIngame());
  if (cursorActive) {
    POINT cursor{};
    if (GetCursorPos(&cursor) && ScreenToClient(window, &cursor)) {
      mx = static_cast<float>(cursor.x);
      my = static_cast<float>(cursor.y);
    }
  }
  const float hitH = 12.0F * scale;
  auto hitButton = [&](const Button &button, bool enabled) {
    return enabled && cursorActive && my >= buttonY - hitH &&
           my <= buttonY + hitH && mx >= button.x - button.half &&
           mx <= button.x + button.half;
  };
  auto hoverPad = [&](const Button &button, bool enabled) {
    if (!hitButton(button, enabled)) return;
    RenderUtils::drawCircle(button.x, buttonY, 13.0F * scale, 0x22FFFFFF, 1.0F);
  };
  if (showShuffle) hoverPad(shuffleB, media.canShuffle);
  hoverPad(prevB, media.canPrevious);
  hoverPad(playB, media.canPlayPause);
  hoverPad(nextB, media.canNext);
  if (showRepeat) hoverPad(repeatB, media.canRepeat);

  const float trackX = compact ? x + 12.0F * scale : textX;
  const float trackW = compact ? cardW - 24.0F * scale : textW;
  const float progressTrackH = 4.0F * scale;
  const float timelineHitPad = 8.0F * scale;
  const bool timelineHovered =
      cursorActive && media.durationMs > 0 &&
      mx >= trackX - timelineHitPad &&
      mx <= trackX + trackW + timelineHitPad &&
      my >= trackY - timelineHitPad &&
      my <= trackY + progressTrackH + timelineHitPad;

  double fraction = 0.0;
  std::int64_t displayPositionMs = media.positionMs;
  if (media.durationMs > 0) {
    if (g_seeking && cursorActive && mx >= 0.0F) {
      fraction = std::clamp(static_cast<double>(mx - trackX) / static_cast<double>(trackW), 0.0, 1.0);
      displayPositionMs = static_cast<std::int64_t>(fraction * static_cast<double>(media.durationMs));
    } else {
      if (media.playing && media.sampledAtMs != 0) {
        const std::uint64_t now = GetTickCount64();
        if (now > media.sampledAtMs)
          displayPositionMs += static_cast<std::int64_t>(now - media.sampledAtMs);
      }
      displayPositionMs = std::clamp<std::int64_t>(displayPositionMs, 0,
                                                   media.durationMs);
      fraction = std::clamp(static_cast<double>(displayPositionMs) /
                                static_cast<double>(media.durationMs),
                            0.0, 1.0);
    }
  }

  RenderUtils::drawRoundedRect(trackX, trackY, trackW, progressTrackH, progressTrackH * 0.5F, 0x33FFFFFF, 1.0F);
  if (fraction > 0.001) {
    const float fillW = std::clamp(trackW * static_cast<float>(fraction), progressTrackH, trackW);
    RenderUtils::drawRoundedRect(trackX, trackY, fillW, progressTrackH, progressTrackH * 0.5F, 0xFF000000 | accentColor, 1.0F);
  }
  if ((timelineHovered || g_seeking) && media.canSeek) {
    const float thumbX = std::clamp(trackX + trackW * static_cast<float>(fraction), trackX, trackX + trackW);
    const float thumbR = (g_seeking ? 6.0F : 4.5F) * scale;
    RenderUtils::drawCircle(thumbX, trackY + progressTrackH * 0.5F, thumbR + 1.0F * scale, 0x44000000, 1.0F);
    RenderUtils::drawCircle(thumbX, trackY + progressTrackH * 0.5F, thumbR, 0xFFFFFFFF, 1.0F);
  }

  const std::string positionText = formatTime(displayPositionMs);
  const std::string durationText = formatTime(media.durationMs);
  g_regular.draw(trackX, timeTop, positionText, 0xFFB3B3B3, timeEm);
  g_regular.draw(trackX + trackW - g_regular.width(durationText, timeEm),
                 timeTop, durationText, 0xFFB3B3B3, timeEm);

  const float s = 6.0F * scale;
  const float thin = 1.8F * scale;
  auto tint = [&](bool enabled, bool active) {
    if (active) { colorFrom(accentColor, 1.0F); return; }
    glColor4f(((textColor >> 16) & 0xFF) / 255.0F,
              ((textColor >> 8) & 0xFF) / 255.0F, (textColor & 0xFF) / 255.0F,
              enabled ? 0.92F : 0.25F);
  };
  auto dot = [&](const Button &button) {
    RenderUtils::drawCircle(button.x, buttonY + s + 4.0F * scale, 1.6F * scale, 0xFF000000 | accentColor, 1.0F);
  };

  if (showShuffle) {
    tint(media.canShuffle, media.canShuffle && media.shuffle);
    if (!drawControlIcon(g_shuffleTexture, shuffleB.x, buttonY,
                         15.0F * scale, 10.0F * scale))
      iconShuffle(shuffleB.x, buttonY, s, thin);
    if (media.canShuffle && media.shuffle) dot(shuffleB);
  }

  tint(media.canPrevious, false);
  iconSkip(prevB.x, buttonY, s, false);

  const bool playHovered = cursorActive && my >= buttonY - hitH && my <= buttonY + hitH && mx >= playB.x - playB.half && mx <= playB.x + playB.half;
  if (playHovered && media.canPlayPause) {
    RenderUtils::drawCircle(playB.x, buttonY, 13.5F * scale, 0x33FFFFFF, 1.0F);
  }
  RenderUtils::drawCircle(playB.x, buttonY, 11.5F * scale, 0xFF000000 | accentColor, media.canPlayPause ? 1.0F : 0.35F);
  glDisable(GL_TEXTURE_2D);
  colorFrom(bgColor, media.canPlayPause ? 1.0F : 0.45F);
  if (media.playing) iconPause(playB.x, buttonY, s);
  else iconPlay(playB.x, buttonY, s);

  tint(media.canNext, false);
  iconSkip(nextB.x, buttonY, s, true);

  if (showRepeat) {
    tint(media.canRepeat, media.canRepeat && media.repeatMode != 0);
    if (!drawControlIcon(g_repeatTexture, repeatB.x, buttonY,
                         15.0F * scale, 9.0F * scale))
      iconRepeat(repeatB.x, buttonY, s, thin);
    if (media.canRepeat && media.repeatMode != 0) dot(repeatB);
    if (media.canRepeat && media.repeatMode == 1) {
      g_bold.draw(repeatB.x + s * 0.55F, buttonY - s * 1.25F, "1",
                  0xFF000000 | accentColor, 7.0F * scale);
    }
  }

  const std::string title =
      media.valid ? media.title : std::string("Nothing playing");
  const std::string artist =
      media.valid ? media.artist : std::string("Start a player to see it here");
  const std::string visibleTitle = elide(g_bold, title, textW, titleEm);
  const std::string visibleArtist = elide(g_regular, artist, textW, artistEm);
  const float titleX = compact
                           ? textX + (textW - g_bold.width(visibleTitle, titleEm)) * 0.5F
                           : textX;
  const float artistX = compact
                            ? textX + (textW - g_regular.width(visibleArtist, artistEm)) * 0.5F
                            : textX;
  g_bold.draw(titleX, titleTop, visibleTitle, 0xFF000000 | textColor, titleEm);
  g_regular.draw(artistX, artistTop, visibleArtist, 0xFFB3B3B3, artistEm);

  glPopAttrib();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  if (!cursorActive || mx < 0.0F) {
    g_previousMouseDown = false;
    g_dragging = false;
    g_seeking = false;
    return;
  }
  if (GetForegroundWindow() != window) {
    g_previousMouseDown = false;
    g_dragging = false;
    g_seeking = false;
    return;
  }

  const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  const bool pressed = down && !g_previousMouseDown;
  const bool inside = mx >= x && mx <= x + cardW && my >= y && my <= y + cardH;

  if (pressed && inside) {
    if (timelineHovered && media.canSeek && media.durationMs > 0) {
      g_seeking = true;
      const double seekFraction = std::clamp(
          static_cast<double>(mx - trackX) / static_cast<double>(trackW),
          0.0, 1.0);
      Media::seekTo(static_cast<std::int64_t>(
          std::llround(seekFraction * static_cast<double>(media.durationMs))));
      g_lastSeekSend = GetTickCount64();
    }
    else if (showShuffle && hitButton(shuffleB, media.canShuffle)) Media::toggleShuffle();
    else if (hitButton(prevB, media.canPrevious)) Media::previous();
    else if (hitButton(playB, media.canPlayPause)) Media::togglePlayPause();
    else if (hitButton(nextB, media.canNext)) Media::next();
    else if (showRepeat && hitButton(repeatB, media.canRepeat)) Media::cycleRepeat();
    else {
      g_dragging = true;
      g_dragOffsetX = mx - x;
      g_dragOffsetY = my - y;
    }
  }

  if (down && g_seeking && media.durationMs > 0) {
    const double seekFraction = std::clamp(
        static_cast<double>(mx - trackX) / static_cast<double>(trackW),
        0.0, 1.0);
    const std::int64_t targetPos = static_cast<std::int64_t>(
        std::llround(seekFraction * static_cast<double>(media.durationMs)));
    const std::uint64_t nowTicks = GetTickCount64();
    if (nowTicks - g_lastSeekSend >= 35) {
      g_lastSeekSend = nowTicks;
      Media::seekTo(targetPos);
    }
  }

  if (down && g_dragging) {
    float newX = (mx - g_dragOffsetX) / static_cast<float>(screenWidth);
    float newY = (my - g_dragOffsetY) / static_cast<float>(screenHeight);
    newX = std::clamp(newX, 0.0F, 1.0F - (cardW / static_cast<float>(screenWidth)));
    newY = std::clamp(newY, 0.0F, 1.0F - (cardH / static_cast<float>(screenHeight)));
    Config::setMediaOverlayX(newX);
    Config::setMediaOverlayY(newY);
  }

  if (!down) {
    if (g_seeking && media.durationMs > 0) {
      const double seekFraction = std::clamp(
          static_cast<double>(mx - trackX) / static_cast<double>(trackW),
          0.0, 1.0);
      Media::seekTo(static_cast<std::int64_t>(
          std::llround(seekFraction * static_cast<double>(media.durationMs))));
    }
    g_seeking = false;
    g_dragging = false;
  }
  g_previousMouseDown = down;
}

void shutdown() {
  if (g_artTexture != 0) {
    glDeleteTextures(1, &g_artTexture);
    g_artTexture = 0;
  }
  g_artVersion = 0;
  if (g_shuffleTexture != 0) glDeleteTextures(1, &g_shuffleTexture);
  if (g_repeatTexture != 0) glDeleteTextures(1, &g_repeatTexture);
  g_shuffleTexture = 0;
  g_repeatTexture = 0;
  g_controlIconsAttempted = false;
  g_bold.release();
  g_regular.release();
  g_previousMouseDown = false;
  g_dragging = false;
  g_seeking = false;
}

} // namespace Render::MediaOverlay
