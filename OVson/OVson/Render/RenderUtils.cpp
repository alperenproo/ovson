#include "RenderUtils.h"

namespace RenderUtils {
float g_animAlpha = 1.0f;

void drawRect(float x, float y, float w, float h, DWORD color,
              float alphaOverride) {
  glDisable(GL_TEXTURE_2D);
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float finalAlpha =
      (alphaOverride >= 0.0f) ? alphaOverride : (a * g_animAlpha);

  glColor4f(r, g, b, finalAlpha);
  glBegin(GL_QUADS);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

void drawGradientRect(float x, float y, float w, float h, DWORD col1,
                      DWORD col2, float alphaShift) {
  glDisable(GL_TEXTURE_2D);
  float r1 = ((col1 >> 16) & 0xFF) / 255.0f;
  float g1 = ((col1 >> 8) & 0xFF) / 255.0f;
  float b1 = (col1 & 0xFF) / 255.0f;
  float a1 = ((col1 >> 24) & 0xFF) / 255.0f;

  float r2 = ((col2 >> 16) & 0xFF) / 255.0f;
  float g2 = ((col2 >> 8) & 0xFF) / 255.0f;
  float b2 = (col2 & 0xFF) / 255.0f;
  float a2 = ((col2 >> 24) & 0xFF) / 255.0f;

  glBegin(GL_QUADS);
  glColor4f(r1, g1, b1, a1 * g_animAlpha * alphaShift);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glColor4f(r2, g2, b2, a2 * g_animAlpha * alphaShift);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

void drawCircleSector(float x, float y, float radius, float startAngle,
                      float endAngle, int segments) {
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(x, y);
  for (int i = 0; i <= segments; i++) {
    float angle =
        startAngle + (endAngle - startAngle) * (float)i / (float)segments;
    glVertex2f(x + cosf(angle) * radius, y + sinf(angle) * radius);
  }
  glEnd();
}

void drawCircle(float x, float y, float radius, DWORD color, float alphaShift) {
  glDisable(GL_TEXTURE_2D);
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = ((color >> 24) & 0xFF) / 255.0f;
  glColor4f(r, g, b, a * g_animAlpha * alphaShift);
  drawCircleSector(x, y, radius, 0, 3.14159265f * 2.0f, 20);
  glEnable(GL_TEXTURE_2D);
}

void drawGlow(float x, float y, float w, float h, float radius, DWORD color,
              float intensity) {
  if (intensity <= 0.0f)
    return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive: colours add up -> glow
  for (int i = 5; i >= 1; --i) {
    float sp = (float)i * 3.0f;
    float la = intensity * (0.5f / (float)i);
    drawRoundedRect(x - sp, y - sp, w + 2.0f * sp, h + 2.0f * sp,
                    radius + sp, color, la);
  }
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // back to normal
}

void drawRadialGlow(float cx, float cy, float radius, DWORD color,
                    float centerAlpha) {
  if (centerAlpha <= 0.0f || radius <= 0.0f)
    return;
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive
  glBegin(GL_TRIANGLE_FAN);
  glColor4f(r, g, b, centerAlpha);
  glVertex2f(cx, cy);
  glColor4f(r, g, b, 0.0f);  // transparent rim
  for (int i = 0; i <= 48; ++i) {
    float an = (float)i * 6.2831853f / 48.0f;
    glVertex2f(cx + cosf(an) * radius, cy + sinf(an) * radius);
  }
  glEnd();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restore
  glEnable(GL_TEXTURE_2D);
}

void drawRoundedRect(float x, float y, float w, float h, float radius,
                     DWORD color, float alphaOverride) {
  glDisable(GL_TEXTURE_2D);
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float finalAlpha =
      (alphaOverride >= 0.0f) ? alphaOverride : (a * g_animAlpha);

  glColor4f(r, g, b, finalAlpha);

  glBegin(GL_QUADS);
  glVertex2f(x + radius, y);
  glVertex2f(x + w - radius, y);
  glVertex2f(x + w - radius, y + h);
  glVertex2f(x + radius, y + h);
  glVertex2f(x, y + radius);
  glVertex2f(x + radius, y + radius);
  glVertex2f(x + radius, y + h - radius);
  glVertex2f(x, y + h - radius);
  glVertex2f(x + w - radius, y + radius);
  glVertex2f(x + w, y + radius);
  glVertex2f(x + w, y + h - radius);
  glVertex2f(x + w - radius, y + h - radius);
  glEnd();

  int segs = 10;
  const float PI = 3.1415926535f;
  drawCircleSector(x + radius, y + radius, radius, PI, PI * 1.5f, segs);
  drawCircleSector(x + w - radius, y + radius, radius, PI * 1.5f, PI * 2.0f,
                   segs);
  drawCircleSector(x + w - radius, y + h - radius, radius, 0.0f, PI * 0.5f,
                   segs);
  drawCircleSector(x + radius, y + h - radius, radius, PI * 0.5f, PI, segs);
  glEnable(GL_TEXTURE_2D);
}

void drawOutline(float x, float y, float w, float h, float thickness,
                 DWORD color, float alphaOverride) {
  glDisable(GL_TEXTURE_2D);
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float finalAlpha =
      (alphaOverride >= 0.0f) ? alphaOverride : (a * g_animAlpha);

  glColor4f(r, g, b, finalAlpha);
  glLineWidth(thickness);
  glBegin(GL_LINE_LOOP);
  glVertex2f(x, y);
  glVertex2f(x + w, y);
  glVertex2f(x + w, y + h);
  glVertex2f(x, y + h);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

void drawRoundedOutline(float x, float y, float w, float h, float radius,
                        float thickness, DWORD color, float alphaOverride) {
  glDisable(GL_TEXTURE_2D);
  float r = ((color >> 16) & 0xFF) / 255.0f;
  float g = ((color >> 8) & 0xFF) / 255.0f;
  float b = (color & 0xFF) / 255.0f;
  float a = ((color >> 24) & 0xFF) / 255.0f;
  float finalAlpha =
      (alphaOverride >= 0.0f) ? alphaOverride : (a * g_animAlpha);

  glColor4f(r, g, b, finalAlpha);
  glLineWidth(thickness);

  int segs = 10;
  const float PI = 3.1415926535f;

  glBegin(GL_LINE_STRIP);
  for (int i = 0; i <= segs; ++i) {
    float ang = PI + (PI * 0.5f) * (float)i / segs;
    glVertex2f(x + radius + cosf(ang) * radius,
               y + radius + sinf(ang) * radius);
  }
  for (int i = 0; i <= segs; ++i) {
    float ang = PI * 1.5f + (PI * 0.5f) * (float)i / segs;
    glVertex2f(x + w - radius + cosf(ang) * radius,
               y + radius + sinf(ang) * radius);
  }
  for (int i = 0; i <= segs; ++i) {
    float ang = 0.0f + (PI * 0.5f) * (float)i / segs;
    glVertex2f(x + w - radius + cosf(ang) * radius,
               y + h - radius + sinf(ang) * radius);
  }
  for (int i = 0; i <= segs; ++i) {
    float ang = PI * 0.5f + (PI * 0.5f) * (float)i / segs;
    glVertex2f(x + radius + cosf(ang) * radius,
               y + h - radius + sinf(ang) * radius);
  }
  glVertex2f(x + radius + cosf(PI) * radius, y + radius + sinf(PI) * radius);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

DWORD lerpColor(DWORD c1, DWORD c2, float t) {
  uint8_t a1 = (c1 >> 24) & 0xFF, a2 = (c2 >> 24) & 0xFF;
  uint8_t r1 = (c1 >> 16) & 0xFF, r2 = (c2 >> 16) & 0xFF;
  uint8_t g1 = (c1 >> 8) & 0xFF, g2 = (c2 >> 8) & 0xFF;
  uint8_t b1 = c1 & 0xFF, b2 = c2 & 0xFF;

  return (DWORD)(((uint8_t)(a1 + (a2 - a1) * t) << 24) |
                 ((uint8_t)(r1 + (r2 - r1) * t) << 16) |
                 ((uint8_t)(g1 + (g2 - g1) * t) << 8) |
                 (uint8_t)(b1 + (b2 - b1) * t));
}
} // namespace RenderUtils
