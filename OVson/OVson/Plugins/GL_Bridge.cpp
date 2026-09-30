#include "GL_Bridge.h"
#include <windows.h>
#include <GL/GL.h>
#include <string>

static void JNICALL gl_push(JNIEnv* env, jclass cls) { glPushMatrix(); }
static void JNICALL gl_pop(JNIEnv* env, jclass cls) { glPopMatrix(); }
static void JNICALL gl_translate(JNIEnv* env, jclass cls, jdouble x, jdouble y, jdouble z) { glTranslated(x, y, z); }
static void JNICALL gl_rotate(JNIEnv* env, jclass cls, jfloat angle, jfloat x, jfloat y, jfloat z) { glRotatef(angle, x, y, z); }
static void JNICALL gl_scale(JNIEnv* env, jclass cls, jdouble x, jdouble y, jdouble z) { glScaled(x, y, z); }

static void setGlState(GLenum cap, jboolean enable) {
    if (enable) glEnable(cap);
    else glDisable(cap);
}

static void JNICALL gl_blend(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_BLEND, enable); }
static void JNICALL gl_alpha(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_ALPHA_TEST, enable); }
static void JNICALL gl_depth(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_DEPTH_TEST, enable); }
static void JNICALL gl_depthMask(JNIEnv* env, jclass cls, jboolean enable) { glDepthMask(enable); }
static void JNICALL gl_depthFunc(JNIEnv* env, jclass cls, jint func) { glDepthFunc(func); }
static void JNICALL gl_cull(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_CULL_FACE, enable); }
static void JNICALL gl_lighting(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_LIGHTING, enable); }
static void JNICALL gl_texture2d(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_TEXTURE_2D, enable); }
static void JNICALL gl_lineSmooth(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_LINE_SMOOTH, enable); }
static void JNICALL gl_lineWidth(JNIEnv* env, jclass cls, jfloat width) { glLineWidth(width); }
static void JNICALL gl_polygonSmooth(JNIEnv* env, jclass cls, jboolean enable) { setGlState(GL_POLYGON_SMOOTH, enable); }

static void JNICALL gl_begin(JNIEnv* env, jclass cls, jint mode) { glBegin(mode); }
static void JNICALL gl_end(JNIEnv* env, jclass cls) { glEnd(); }

static void JNICALL gl_vertex2(JNIEnv* env, jclass cls, jfloat x, jfloat y) { glVertex2f(x, y); }
static void JNICALL gl_vertex3(JNIEnv* env, jclass cls, jdouble x, jdouble y, jdouble z) { glVertex3d(x, y, z); }
static void JNICALL gl_color4f(JNIEnv* env, jclass cls, jfloat r, jfloat g, jfloat b, jfloat a) { glColor4f(r, g, b, a); }

static void JNICALL gl_color(JNIEnv* env, jclass cls, jint color) {
    float a = (float)(color >> 24 & 255) / 255.0f;
    float r = (float)(color >> 16 & 255) / 255.0f;
    float g = (float)(color >> 8 & 255) / 255.0f;
    float b = (float)(color & 255) / 255.0f;
    glColor4f(r, g, b, a);
}

static void JNICALL gl_resetColor(JNIEnv* env, jclass cls) { glColor4f(1.0f, 1.0f, 1.0f, 1.0f); }
static void JNICALL gl_colorMask(JNIEnv* env, jclass cls, jboolean r, jboolean g, jboolean b, jboolean a) { glColorMask(r, g, b, a); }
static void JNICALL gl_texCoord2(JNIEnv* env, jclass cls, jfloat u, jfloat v) { glTexCoord2f(u, v); }
static void JNICALL gl_normal(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat z) { glNormal3f(x, y, z); }
static void JNICALL gl_bindTexture(JNIEnv* env, jclass cls, jint id) { glBindTexture(GL_TEXTURE_2D, id); }

static jint JNICALL gl_getBoundTexture(JNIEnv* env, jclass cls) {
    jint id = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&id);
    return id;
}

static jint JNICALL gl_loadTexture(JNIEnv* env, jclass cls, jstring path) {
    // TODO: implement texture loading
    return 0;
}

static JNINativeMethod gMethods[] = {
    {(char*)"push", (char*)"()V", (void*)gl_push},
    {(char*)"pop", (char*)"()V", (void*)gl_pop},
    {(char*)"translate", (char*)"(DDD)V", (void*)gl_translate},
    {(char*)"rotate", (char*)"(FFFF)V", (void*)gl_rotate},
    {(char*)"scale", (char*)"(DDD)V", (void*)gl_scale},
    {(char*)"blend", (char*)"(Z)V", (void*)gl_blend},
    {(char*)"alpha", (char*)"(Z)V", (void*)gl_alpha},
    {(char*)"depth", (char*)"(Z)V", (void*)gl_depth},
    {(char*)"depthMask", (char*)"(Z)V", (void*)gl_depthMask},
    {(char*)"depthFunc", (char*)"(I)V", (void*)gl_depthFunc},
    {(char*)"cull", (char*)"(Z)V", (void*)gl_cull},
    {(char*)"lighting", (char*)"(Z)V", (void*)gl_lighting},
    {(char*)"texture2d", (char*)"(Z)V", (void*)gl_texture2d},
    {(char*)"lineSmooth", (char*)"(Z)V", (void*)gl_lineSmooth},
    {(char*)"lineWidth", (char*)"(F)V", (void*)gl_lineWidth},
    {(char*)"polygonSmooth", (char*)"(Z)V", (void*)gl_polygonSmooth},
    {(char*)"begin", (char*)"(I)V", (void*)gl_begin},
    {(char*)"end", (char*)"()V", (void*)gl_end},
    {(char*)"vertex2", (char*)"(FF)V", (void*)gl_vertex2},
    {(char*)"vertex3", (char*)"(DDD)V", (void*)gl_vertex3},
    {(char*)"color4f", (char*)"(FFFF)V", (void*)gl_color4f},
    {(char*)"color", (char*)"(I)V", (void*)gl_color},
    {(char*)"resetColor", (char*)"()V", (void*)gl_resetColor},
    {(char*)"colorMask", (char*)"(ZZZZ)V", (void*)gl_colorMask},
    {(char*)"texCoord2", (char*)"(FF)V", (void*)gl_texCoord2},
    {(char*)"normal", (char*)"(FFF)V", (void*)gl_normal},
    {(char*)"bindTexture", (char*)"(I)V", (void*)gl_bindTexture},
    {(char*)"getBoundTexture", (char*)"()I", (void*)gl_getBoundTexture},
    {(char*)"loadTexture", (char*)"(Ljava/lang/String;)I", (void*)gl_loadTexture}
};

namespace GLBridge {
    void registerNatives(JNIEnv* env, jclass cls) {
        if (!env || !cls) return;
        env->RegisterNatives(cls, gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
    }
}
