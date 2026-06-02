#pragma once
#include <Windows.h>
#include <gl/GL.h>

namespace GlGuard {

class GlAttribGuard {
public:
  explicit GlAttribGuard(GLbitfield mask = GL_ALL_ATTRIB_BITS) {
    glPushAttrib(mask);
  }
  ~GlAttribGuard() { glPopAttrib(); }
  GlAttribGuard(const GlAttribGuard &) = delete;
  GlAttribGuard &operator=(const GlAttribGuard &) = delete;
};

class GlMatrixGuard {
public:
  explicit GlMatrixGuard(GLenum mode = GL_MODELVIEW) : m_mode(mode) {
    glGetIntegerv(GL_MATRIX_MODE, &m_prevMode);
    glMatrixMode(m_mode);
    glPushMatrix();
  }
  ~GlMatrixGuard() {
    glMatrixMode(m_mode);
    glPopMatrix();
    glMatrixMode((GLenum)m_prevMode);
  }
  GlMatrixGuard(const GlMatrixGuard &) = delete;
  GlMatrixGuard &operator=(const GlMatrixGuard &) = delete;

private:
  GLenum m_mode;
  GLint  m_prevMode = GL_MODELVIEW;
};

class GlTextureGuard {
public:
  GlTextureGuard() {
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_prev);
  }
  ~GlTextureGuard() { glBindTexture(GL_TEXTURE_2D, (GLuint)m_prev); }
  GlTextureGuard(const GlTextureGuard &) = delete;
  GlTextureGuard &operator=(const GlTextureGuard &) = delete;

private:
  GLint m_prev = 0;
};

class GlFullGuard {
public:
  GlFullGuard()
      : m_attrib(GL_ALL_ATTRIB_BITS),
        m_mv(GL_MODELVIEW),
        m_pr(GL_PROJECTION) {}

private:
  GlAttribGuard m_attrib;
  GlMatrixGuard m_mv;
  GlMatrixGuard m_pr;
  GlTextureGuard m_tex;
};

} // namespace GlGuard
