#include "LiquidGlass.h"
#include "../Utils/Logger.h"
#include <gl/GL.h>
#include <cmath>
#include <fstream>
#include <string>
#include "../Config/Config.h"
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace Render {

Shader LiquidGlass::blurShader;
Shader LiquidGlass::glassShader;
Framebuffer LiquidGlass::fboA;
Framebuffer LiquidGlass::fboB;
unsigned int LiquidGlass::screenTexture = 0;
int LiquidGlass::screenWidth = 0;
int LiquidGlass::screenHeight = 0;
bool LiquidGlass::initialized = false;
float LiquidGlass::time = 0.0f;


const char* const BLUR_VERTEX_SHADER = 
#include "Shaders/blur_vert.glsl"
;

const char* const BLUR_FRAGMENT_SHADER = 
#include "Shaders/blur_frag.glsl"
;

const char* const LIQUID_GLASS_VERTEX_SHADER = 
#include "Shaders/glass_vert.glsl"
;

const char* const LIQUID_GLASS_FRAGMENT_SHADER = 
#include "Shaders/glass_frag.glsl"
;




static void LG_Log(const std::string& msg) {
    std::ofstream ofs("liquidglass_debug.log", std::ios::app);
    if (ofs.is_open()) {
        ofs << msg << "\n";
    }
}

static void CheckGLError(const std::string& prefix) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LG_Log(prefix + " - GL Error: " + std::to_string(err));
    }
}


void LiquidGlass::init() {
    if (initialized) return;
    
    LG_Log("Initializing LiquidGlass...");
    initGLExtensions();
    
    if (!glCreateShader) {
        LG_Log("ERROR: OpenGL functions not loaded!");
        Logger::error("LiquidGlass: OpenGL functions not loaded yet.");
        return;
    }

    if (!blurShader.compile(BLUR_VERTEX_SHADER, BLUR_FRAGMENT_SHADER)) {
        LG_Log("ERROR: Failed to compile blur shader");
        Logger::error("LiquidGlass: Failed to compile blur shader");
        return;
    }
    LG_Log("Blur shader compiled successfully.");

    if (!glassShader.compile(LIQUID_GLASS_VERTEX_SHADER, LIQUID_GLASS_FRAGMENT_SHADER)) {
        LG_Log("ERROR: Failed to compile liquid glass shader");
        Logger::error("LiquidGlass: Failed to compile liquid glass shader");
        return;
    }
    LG_Log("Glass shader compiled successfully.");

    initialized = true;
    LG_Log("LiquidGlass initialized successfully.");
    Logger::info("LiquidGlass: Shaders compiled successfully.");
}

void LiquidGlass::shutdown() {
    if (screenTexture != 0) {
        glDeleteTextures(1, &screenTexture);
        screenTexture = 0;
    }
    initialized = false;
}

void LiquidGlass::setupScreenTexture(int w, int h) {
    if (screenTexture != 0 && (w != screenWidth || h != screenHeight)) {
        glDeleteTextures(1, &screenTexture);
        screenTexture = 0;
    }

    if (screenTexture == 0) {
        glGenTextures(1, &screenTexture);
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        screenWidth = w;
        screenHeight = h;
    }
}

void LiquidGlass::drawFullScreenQuad() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void LiquidGlass::beginFrame(int sw, int sh) {
    if (!initialized) {
        init();
        if (!initialized) return;
    }
    if (!isReady()) return;

    GLint lastFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFbo);
    GLint lastProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);

    setupScreenTexture(sw, sh);
    int dw = sw / 2;
    int dh = sh / 2;
    fboA.resize(dw, dh);
    fboB.resize(dw, dh);

    if (!fboA.isInitialized() || !fboB.isInitialized()) return;

    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sw, sh);
    glBindTexture(GL_TEXTURE_2D, 0);

    glPushAttrib(GL_VIEWPORT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    fboA.bind();
    glViewport(0, 0, dw, dh);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    if (glUseProgram) {
        glUseProgram(0);
    }
    drawFullScreenQuad();

    fboB.bind();
    blurShader.use();
    blurShader.setUniform1i("u_texture", 0);
    blurShader.setUniform2f("u_direction", 1.5f / (float)dw, 0.0f);
    glBindTexture(GL_TEXTURE_2D, fboA.getTexture());
    drawFullScreenQuad();

    fboA.bind();
    blurShader.setUniform2f("u_direction", 0.0f, 1.5f / (float)dh);
    glBindTexture(GL_TEXTURE_2D, fboB.getTexture());
    drawFullScreenQuad();

    // restore bindings
    if (glUseProgram) {
        glUseProgram(lastProgram);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    if (glBindFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, lastFbo);
    }
    glPopAttrib();
}

void LiquidGlass::drawRect(float x, float y, float w, float h, float radius, float alpha, unsigned int accentColor, bool isMainPanel) {
    if (!isReady() || !fboA.isInitialized()) {
        return;
    }

    GLint lastFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFbo);
    GLint lastProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);

    POINT mousePt;
    GetCursorPos(&mousePt);
    HWND hwnd = GetActiveWindow();
    if (hwnd) {
        ScreenToClient(hwnd, &mousePt);
    }
    float mx = (float)mousePt.x;
    float my = (float)mousePt.y;

    float centerX = x + w / 2.0f;
    float centerY = y + h / 2.0f;
    float dx = mx - centerX;
    float dy = my - centerY;
    float dist = sqrt(dx * dx + dy * dy);

    float shiftX = 0.0f;
    float shiftY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if (w < 400.0f && h < 400.0f && dist < 150.0f) {
        float factor = 1.0f - (dist / 150.0f); // 1 at center 0 at boundary
        float elasticity = 0.12f;

        shiftX = dx * elasticity * 0.15f * factor;
        shiftY = dy * elasticity * 0.15f * factor;

        if (dist > 0.1f) {
            float nx = dx / dist;
            float ny = dy / dist;
            float intensity = (dist / 150.0f) * elasticity * factor;

            scaleX = 1.0f + abs(nx) * intensity * 0.25f - abs(ny) * intensity * 0.12f;
            scaleY = 1.0f + abs(ny) * intensity * 0.25f - abs(nx) * intensity * 0.12f;
        }
    }

    glPushMatrix();
    glTranslatef(centerX + shiftX, centerY + shiftY, 0.0f);
    glScalef(scaleX, scaleY, 1.0f);
    glTranslatef(-centerX, -centerY, 0.0f);

    float mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);

    float x0 = x * mv[0] + mv[12];
    float y0 = y * mv[5] + mv[13];
    float x1 = (x + w) * mv[0] + mv[12];
    float y1 = (y + h) * mv[5] + mv[13];

    float newX = x0;
    float newY = y0;
    float newW = x1 - x0;
    float newH = y1 - y0;

    float currentScale = (abs(mv[0]) + abs(mv[5])) * 0.5f;
    float newRadius = radius * currentScale;

    glassShader.use();

    float ar = ((accentColor >> 16) & 0xFF) / 255.0f;
    float ag = ((accentColor >> 8) & 0xFF) / 255.0f;
    float ab = (accentColor & 0xFF) / 255.0f;
    float aa = ((accentColor >> 24) & 0xFF) / 255.0f;

    if (glActiveTexture) glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glassShader.setUniform1i("u_screenTexture", 0);

    if (glActiveTexture) glActiveTexture(GL_TEXTURE0 + 1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fboA.getTexture());
    glassShader.setUniform1i("u_blurredTexture", 1);

    glassShader.setUniform2f("u_mouse", mx, my);
    glassShader.setUniform2f("u_resolution", (float)screenWidth, (float)screenHeight);
    glassShader.setUniform2f("u_rectPos", newX, newY);
    glassShader.setUniform2f("u_rectSize", newW, newH);
    glassShader.setUniform1f("u_radius", newRadius);
    glassShader.setUniform1f("u_time", time);
    glassShader.setUniform1f("u_alpha", alpha);
    glassShader.setUniform4f("u_accentColor", ar, ag, ab, aa);
    glassShader.setUniform1i("u_wiggleEnabled", Config::isLiquidGlassWiggleEnabled() ? 1 : 0);
    glassShader.setUniform1i("u_glowEnabled", Config::isLiquidGlassGlowEnabled() ? 1 : 0);
    glassShader.setUniform1f("u_refractStrength", Config::getLiquidGlassRefractStrength());
    glassShader.setUniform1f("u_edgeWidth", isMainPanel ? Config::getLiquidGlassEdgeWidth() : Config::getLiquidGlassCardEdgeWidth());
    glassShader.setUniform1f("u_darkness", Config::getLiquidGlassDarkness());

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();

    glPopMatrix();

    if (glUseProgram) {
        glUseProgram(lastProgram);
    }
    if (glActiveTexture) {
        glActiveTexture(GL_TEXTURE0 + 1);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if (glBindFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, lastFbo);
    }
}

} // namespace Render
