#pragma once
#include "GL.h"

class Framebuffer {
private:
    unsigned int fbo;
    unsigned int texture;
    int width;
    int height;
    bool initialized;

    void cleanup();

public:
    Framebuffer();
    ~Framebuffer();

    bool init(int w, int h);
    void bind();
    void unbind();
    void resize(int w, int h);

    unsigned int getFbo() const { return fbo; }
    unsigned int getTexture() const { return texture; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isInitialized() const { return initialized; }
};
