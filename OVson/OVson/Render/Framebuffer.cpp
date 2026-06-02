#include "Framebuffer.h"
#include "../Utils/Logger.h"

Framebuffer::Framebuffer() : fbo(0), texture(0), width(0), height(0), initialized(false) {}

Framebuffer::~Framebuffer() {
    cleanup();
}

void Framebuffer::cleanup() {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (fbo != 0 && glDeleteFramebuffers != nullptr) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    initialized = false;
}

bool Framebuffer::init(int w, int h) {
    if (!glGenFramebuffers || !glBindFramebuffer || !glFramebufferTexture2D || !glCheckFramebufferStatus) {
        Logger::error("OpenGL Framebuffer functions are not loaded!");
        return false;
    }

    cleanup();
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Logger::error(("Framebuffer is incomplete! Status: " + std::to_string(status)).c_str());
        cleanup();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    initialized = true;
    return true;
}

void Framebuffer::bind() {
    if (initialized && glBindFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }
}

void Framebuffer::unbind() {
    if (glBindFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void Framebuffer::resize(int w, int h) {
    if (w != width || h != height || !initialized) {
        init(w, h);
    }
}
