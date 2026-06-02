#pragma once
#include "../Render/GL.h"
#include "../Render/Shader.h"
#include "../Render/Framebuffer.h"

namespace Render {

class LiquidGlass {
private:
    static Shader blurShader;
    static Shader glassShader;
    static Framebuffer fboA;
    static Framebuffer fboB;
    static unsigned int screenTexture;
    static int screenWidth;
    static int screenHeight;
    static bool initialized;
    static float time;

    static void setupScreenTexture(int w, int h);
    static void drawFullScreenQuad();

public:
    static void init();
    static void shutdown();
    static void beginFrame(int sw, int sh);
    static void drawRect(float x, float y, float w, float h, float radius, float alpha, unsigned int accentColor = 0x00000000, bool isMainPanel = false);
    static bool isReady() { return initialized && blurShader.isCompiled() && glassShader.isCompiled(); }
    static void updateTime(float dt) { time += dt; }
};

} // namespace Render
