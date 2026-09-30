#include "RenderAPI_Bridge.h"
#include "../Render/RenderUtils.h"
#include "../Java.h"
#include <windows.h>
#include <GL/gl.h>

static void colorToGL(int argb, float& r, float& g, float& b, float& a) {
    a = ((argb >> 24) & 0xFF) / 255.0f;
    r = ((argb >> 16) & 0xFF) / 255.0f;
    g = ((argb >> 8) & 0xFF) / 255.0f;
    b = (argb & 0xFF) / 255.0f;
}

static bool getRenderPos(JNIEnv* env, double& rx, double& ry, double& rz) {
    jclass rmClass = lc->GetClass("net.minecraft.client.renderer.entity.RenderManager");
    if (!rmClass) return false;
    
    // TODO: Need proper mappings for RenderManager.renderPosX/Y/Z
    rx = 0.0; ry = 0.0; rz = 0.0;
    return true;
}

static double getVec3Field(JNIEnv* env, jobject vec3, const char* name) {
    jclass cls = env->GetObjectClass(vec3);
    jfieldID fid = env->GetFieldID(cls, name, "D");
    if (!fid) {
        env->ExceptionClear();
        return 0.0;
    }
    double val = env->GetDoubleField(vec3, fid);
    env->DeleteLocalRef(cls);
    return val;
}

namespace RenderAPIBridge {

    static void JNICALL drawRect(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jint color) {
        RenderUtils::drawRect(x, y, w, h, (DWORD)color);
    }

    static void JNICALL drawRoundedRect(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jfloat radius, jfloat alphaOverride, jint color) {
        RenderUtils::drawRoundedRect(x, y, w, h, radius, (DWORD)color, alphaOverride);
    }

    static void JNICALL drawOutline(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jfloat thickness, jint color, jint alphaOverride) {
        RenderUtils::drawOutline(x, y, w, h, thickness, (DWORD)color, (float)alphaOverride);
    }

    static void JNICALL drawRoundedOutline(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jfloat radius, jfloat thickness, jint color, jint alphaOverride) {
        RenderUtils::drawRoundedOutline(x, y, w, h, radius, thickness, (DWORD)color, (float)alphaOverride);
    }

    static void JNICALL drawGlow(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jfloat radius, jint color, jfloat intensity) {
        RenderUtils::drawGlow(x, y, w, h, radius, (DWORD)color, intensity);
    }

    static void JNICALL drawGradientRect(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat w, jfloat h, jint colorTop, jint colorBottom) {
        RenderUtils::drawGradientRect(x, y, w, h, (DWORD)colorTop, (DWORD)colorBottom);
    }

    static void JNICALL drawLine2D(JNIEnv* env, jclass cls, jfloat x1, jfloat y1, jfloat x2, jfloat y2, jfloat width, jint color) {
        float r, g, b, a;
        colorToGL(color, r, g, b, a);
        
        glPushMatrix();
        glEnable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(width);
        glColor4f(r, g, b, a);
        
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
        
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glPopMatrix();
    }

    static void JNICALL drawCircle(JNIEnv* env, jclass cls, jfloat x, jfloat y, jfloat radius, jint color) {
        RenderUtils::drawCircle(x, y, radius, (DWORD)color);
    }

    static void JNICALL drawString(JNIEnv* env, jclass cls, jfloat x, jfloat y, jstring text, jint color, jfloat scale) {
        // TODO: Implement with FontRenderer
    }

    static jfloat JNICALL getStringWidth(JNIEnv* env, jclass cls, jstring text) {
        // TODO: Implement with FontRenderer
        return 0.0f;
    }

    static jint JNICALL getDisplayWidth(JNIEnv* env, jclass cls) {
        jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
        if (!mcClass) return 0;
        jmethodID getMcMethod = lc->GetMethodID(mcClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
        if (!getMcMethod) getMcMethod = lc->GetMethodID(mcClass, "A", "()Lnet/minecraft/client/Minecraft;");
        if (!getMcMethod) return 0;
        
        jobject mc = env->CallStaticObjectMethod(mcClass, getMcMethod);
        if (!mc) return 0;
        
        jfieldID fid = lc->GetFieldID(mcClass, "displayWidth", "I");
        if (!fid) fid = lc->GetFieldID(mcClass, "field_71443_c", "I");
        if (!fid) fid = lc->GetFieldID(mcClass, "d", "I");
        
        jint width = fid ? env->GetIntField(mc, fid) : 0;
        env->DeleteLocalRef(mc);
        return width;
    }

    static jint JNICALL getDisplayHeight(JNIEnv* env, jclass cls) {
        jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
        if (!mcClass) return 0;
        jmethodID getMcMethod = lc->GetMethodID(mcClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
        if (!getMcMethod) getMcMethod = lc->GetMethodID(mcClass, "A", "()Lnet/minecraft/client/Minecraft;");
        if (!getMcMethod) return 0;
        
        jobject mc = env->CallStaticObjectMethod(mcClass, getMcMethod);
        if (!mc) return 0;
        
        jfieldID fid = lc->GetFieldID(mcClass, "displayHeight", "I");
        if (!fid) fid = lc->GetFieldID(mcClass, "field_71440_d", "I");
        if (!fid) fid = lc->GetFieldID(mcClass, "e", "I");
        
        jint height = fid ? env->GetIntField(mc, fid) : 0;
        env->DeleteLocalRef(mc);
        return height;
    }

    static void JNICALL drawLine3D(JNIEnv* env, jclass cls, jobject v1, jobject v2, jfloat width, jint color) {
        if (!v1 || !v2) return;
        double rx, ry, rz;
        getRenderPos(env, rx, ry, rz);
        
        double x1 = getVec3Field(env, v1, "x") - rx;
        double y1 = getVec3Field(env, v1, "y") - ry;
        double z1 = getVec3Field(env, v1, "z") - rz;
        
        double x2 = getVec3Field(env, v2, "x") - rx;
        double y2 = getVec3Field(env, v2, "y") - ry;
        double z2 = getVec3Field(env, v2, "z") - rz;
        
        float r, g, b, a;
        colorToGL(color, r, g, b, a);
        
        glPushMatrix();
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(width);
        glColor4f(r, g, b, a);
        
        glBegin(GL_LINES);
        glVertex3d(x1, y1, z1);
        glVertex3d(x2, y2, z2);
        glEnd();
        
        glPopAttrib();
        glPopMatrix();
    }

    static void JNICALL drawBlock(JNIEnv* env, jclass cls, jint x, jint y, jint z, jint color, jboolean outline, jboolean shade) {
        double rx, ry, rz;
        getRenderPos(env, rx, ry, rz);
        
        double dx = x - rx;
        double dy = y - ry;
        double dz = z - rz;
        
        float r, g, b, a;
        colorToGL(color, r, g, b, a);
        
        glPushMatrix();
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, a);
        glTranslated(dx, dy, dz);
        
        if (shade) {
            glBegin(GL_QUADS);
            // Front face
            glVertex3d(0, 0, 1); glVertex3d(1, 0, 1); glVertex3d(1, 1, 1); glVertex3d(0, 1, 1);
            // Back face
            glVertex3d(1, 0, 0); glVertex3d(0, 0, 0); glVertex3d(0, 1, 0); glVertex3d(1, 1, 0);
            // Top face
            glVertex3d(0, 1, 1); glVertex3d(1, 1, 1); glVertex3d(1, 1, 0); glVertex3d(0, 1, 0);
            // Bottom face
            glVertex3d(0, 0, 0); glVertex3d(1, 0, 0); glVertex3d(1, 0, 1); glVertex3d(0, 0, 1);
            // Right face
            glVertex3d(1, 0, 1); glVertex3d(1, 0, 0); glVertex3d(1, 1, 0); glVertex3d(1, 1, 1);
            // Left face
            glVertex3d(0, 0, 0); glVertex3d(0, 0, 1); glVertex3d(0, 1, 1); glVertex3d(0, 1, 0);
            glEnd();
        }
        
        if (outline) {
            glLineWidth(1.5f);
            glColor4f(r, g, b, a * 1.5f > 1.0f ? 1.0f : a * 1.5f);
            glBegin(GL_LINES);
            // TODO: Draw outline lines
            glEnd();
        }
        
        glPopAttrib();
        glPopMatrix();
    }

    static void JNICALL drawEntityBox(JNIEnv* env, jclass cls, jobject entity, jint color, jfloat pt, jboolean outline, jboolean shade) {
        // TODO: Implement entity box
    }

    static void JNICALL drawTracer(JNIEnv* env, jclass cls, jobject entity, jfloat width, jint color, jfloat pt) {
        // TODO: Implement tracer
    }

    static void JNICALL drawFilledBox(JNIEnv* env, jclass cls, jobject minVec, jobject maxVec, jint color) {
        // TODO: Implement filled box
    }

    static void JNICALL drawBeam(JNIEnv* env, jclass cls, jobject posVec, jfloat height, jint color) {
        // TODO: Implement beam
    }

    static void JNICALL drawString3D(JNIEnv* env, jclass cls, jstring str, jobject posVec, jfloat scale, jboolean shadow, jboolean depth, jint color) {
        // TODO: Implement 3D string
    }

    static jobject JNICALL worldToScreen(JNIEnv* env, jclass cls, jdouble x, jdouble y, jdouble z, jfloat pt1, jfloat pt2) {
        // TODO: Implement worldToScreen manual gluProject
        return nullptr;
    }

    static jobject JNICALL screenToWorld(JNIEnv* env, jclass cls, jint x, jint y, jfloat depth) {
        return nullptr;
    }

    static void JNICALL beginBlur(JNIEnv* env, jclass cls) {
        // TODO
    }

    static void JNICALL applyBlur(JNIEnv* env, jclass cls) {
        // TODO
    }

    static void JNICALL beginBloom(JNIEnv* env, jclass cls) {
        // TODO
    }

    static void JNICALL applyBloom(JNIEnv* env, jclass cls) {
        // TODO
    }

    static void JNICALL enableScissor(JNIEnv* env, jclass cls, jint x, jint y, jint w, jint h) {
        jint displayHeight = getDisplayHeight(env, cls);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, displayHeight - y - h, w, h);
    }

    static void JNICALL disableScissor(JNIEnv* env, jclass cls) {
        glDisable(GL_SCISSOR_TEST);
    }

    static void JNICALL drawImage(JNIEnv* env, jclass cls, jint id, jfloat x, jfloat y, jfloat w, jfloat h) {
        // TODO
    }

    static void JNICALL drawItem(JNIEnv* env, jclass cls, jobject item, jfloat x, jfloat y, jfloat scale) {
        // TODO
    }

    static void JNICALL drawPlane(JNIEnv* env, jclass cls, jobject posVec, jfloat size, jint color) {
        // TODO
    }

    static JNINativeMethod methods[] = {
        {(char*)"drawRect", (char*)"(FFFFI)V", (void*)drawRect},
        {(char*)"drawRoundedRect", (char*)"(FFFFFFI)V", (void*)drawRoundedRect},
        {(char*)"drawOutline", (char*)"(FFFFFII)V", (void*)drawOutline},
        {(char*)"drawRoundedOutline", (char*)"(FFFFFFII)V", (void*)drawRoundedOutline},
        {(char*)"drawGlow", (char*)"(FFFFFIF)V", (void*)drawGlow},
        {(char*)"drawGradientRect", (char*)"(FFFFII)V", (void*)drawGradientRect},
        {(char*)"drawLine2D", (char*)"(FFFFFI)V", (void*)drawLine2D},
        {(char*)"drawCircle", (char*)"(FFFI)V", (void*)drawCircle},
        
        {(char*)"drawString", (char*)"(FFLjava/lang/String;IF)V", (void*)drawString},
        {(char*)"getStringWidth", (char*)"(Ljava/lang/String;)F", (void*)getStringWidth},
        
        {(char*)"getDisplayWidth", (char*)"()I", (void*)getDisplayWidth},
        {(char*)"getDisplayHeight", (char*)"()I", (void*)getDisplayHeight},
        
        {(char*)"drawLine3D", (char*)"(Ljava/lang/Object;Ljava/lang/Object;FI)V", (void*)drawLine3D},
        {(char*)"drawBlock", (char*)"(IIIIZZ)V", (void*)drawBlock},
        {(char*)"drawEntityBox", (char*)"(Ljava/lang/Object;IFZZ)V", (void*)drawEntityBox},
        {(char*)"drawTracer", (char*)"(Ljava/lang/Object;FIF)V", (void*)drawTracer},
        {(char*)"drawFilledBox", (char*)"(Ljava/lang/Object;Ljava/lang/Object;I)V", (void*)drawFilledBox},
        {(char*)"drawBeam", (char*)"(Ljava/lang/Object;FI)V", (void*)drawBeam},
        {(char*)"drawString3D", (char*)"(Ljava/lang/String;Ljava/lang/Object;FZZI)V", (void*)drawString3D},
        
        {(char*)"worldToScreen", (char*)"(DDDFF)Ljava/lang/Object;", (void*)worldToScreen},
        {(char*)"screenToWorld", (char*)"(IIF)Ljava/lang/Object;", (void*)screenToWorld},
        
        {(char*)"beginBlur", (char*)"()V", (void*)beginBlur},
        {(char*)"applyBlur", (char*)"()V", (void*)applyBlur},
        {(char*)"beginBloom", (char*)"()V", (void*)beginBloom},
        {(char*)"applyBloom", (char*)"()V", (void*)applyBloom},
        
        {(char*)"enableScissor", (char*)"(IIII)V", (void*)enableScissor},
        {(char*)"disableScissor", (char*)"()V", (void*)disableScissor},
        
        {(char*)"drawImage", (char*)"(IFFFF)V", (void*)drawImage},
        {(char*)"drawItem", (char*)"(Ljava/lang/Object;FFF)V", (void*)drawItem},
        {(char*)"drawPlane", (char*)"(Ljava/lang/Object;FI)V", (void*)drawPlane}
    };

    void registerNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(JNINativeMethod));
    }
}
