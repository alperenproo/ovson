#include "KeybindAPI_Bridge.h"
#include "../Java.h"
#include <windows.h>

namespace KeybindAPIBridge {

    jboolean JNICALL isKeyDown(JNIEnv* env, jclass clazz, jint keyCode) {
        return (GetAsyncKeyState(keyCode) & 0x8000) != 0;
    }

    jboolean JNICALL isMouseDown(JNIEnv* env, jclass clazz, jint button) {
        return (GetAsyncKeyState(VK_LBUTTON + button) & 0x8000) != 0;
    }

    jboolean JNICALL isPressed(JNIEnv* env, jclass clazz, jstring keyName) {
        return JNI_FALSE;
    }

    void JNICALL setPressed(JNIEnv* env, jclass clazz, jstring keyName, jboolean pressed) {
    }

    jint JNICALL getKeyCode(JNIEnv* env, jclass clazz, jstring keyName) {
        return 0;
    }

    void JNICALL leftClick(JNIEnv* env, jclass clazz) {
    }

    void JNICALL rightClick(JNIEnv* env, jclass clazz) {
    }

    jint JNICALL getScroll(JNIEnv* env, jclass clazz) {
        return 0;
    }

    jintArray JNICALL getMousePosition(JNIEnv* env, jclass clazz) {
        jintArray result = env->NewIntArray(2);
        if (result == nullptr) return nullptr;
        jint pos[2] = {0, 0};
        env->SetIntArrayRegion(result, 0, 2, pos);
        return result;
    }

    JNINativeMethod methods[] = {
        {(char*)"isKeyDown", (char*)"(I)Z", (void*)isKeyDown},
        {(char*)"isMouseDown", (char*)"(I)Z", (void*)isMouseDown},
        {(char*)"isPressed", (char*)"(Ljava/lang/String;)Z", (void*)isPressed},
        {(char*)"setPressed", (char*)"(Ljava/lang/String;Z)V", (void*)setPressed},
        {(char*)"getKeyCode", (char*)"(Ljava/lang/String;)I", (void*)getKeyCode},
        {(char*)"leftClick", (char*)"()V", (void*)leftClick},
        {(char*)"rightClick", (char*)"()V", (void*)rightClick},
        {(char*)"getScroll", (char*)"()I", (void*)getScroll},
        {(char*)"getMousePosition", (char*)"()[I", (void*)getMousePosition}
    };

    void registerNatives(JNIEnv* env, jclass cls) { env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0])); }
}


