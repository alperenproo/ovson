#include "GL.h"

PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;

template<typename T>
T getGLProc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        HMODULE module = GetModuleHandleA("opengl32.dll");
        if (!module) module = LoadLibraryA("opengl32.dll");
        if (module) p = (void*)GetProcAddress(module, name);
    }
    return reinterpret_cast<T>(p);
}

void initGLExtensions() {
    static bool initialized = false;
    if (initialized) return;

    glCreateShader = getGLProc<PFNGLCREATESHADERPROC>("glCreateShader");
    if (!glCreateShader) {
        return;
    }
    
    initialized = true;

    glShaderSource = getGLProc<PFNGLSHADERSOURCEPROC>("glShaderSource");
    glCompileShader = getGLProc<PFNGLCOMPILESHADERPROC>("glCompileShader");
    glGetShaderiv = getGLProc<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    glGetShaderInfoLog = getGLProc<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
    glCreateProgram = getGLProc<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    glAttachShader = getGLProc<PFNGLATTACHSHADERPROC>("glAttachShader");
    glLinkProgram = getGLProc<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    glGetProgramiv = getGLProc<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    glGetProgramInfoLog = getGLProc<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
    glUseProgram = getGLProc<PFNGLUSEPROGRAMPROC>("glUseProgram");
    glDeleteShader = getGLProc<PFNGLDELETESHADERPROC>("glDeleteShader");
    glDeleteProgram = getGLProc<PFNGLDELETEPROGRAMPROC>("glDeleteProgram");
    glGetUniformLocation = getGLProc<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    glUniform1f = getGLProc<PFNGLUNIFORM1FPROC>("glUniform1f");
    glUniform2f = getGLProc<PFNGLUNIFORM2FPROC>("glUniform2f");
    glUniform3f = getGLProc<PFNGLUNIFORM3FPROC>("glUniform3f");
    glUniform4f = getGLProc<PFNGLUNIFORM4FPROC>("glUniform4f");
    glUniform1i = getGLProc<PFNGLUNIFORM1IPROC>("glUniform1i");
    glActiveTexture = getGLProc<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    glGenFramebuffers = getGLProc<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    glBindFramebuffer = getGLProc<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    glFramebufferTexture2D = getGLProc<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    glCheckFramebufferStatus = getGLProc<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    glDeleteFramebuffers = getGLProc<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
}
