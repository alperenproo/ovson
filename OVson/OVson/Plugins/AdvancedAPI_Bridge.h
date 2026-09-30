#pragma once
#include <jni.h>

namespace AdvancedAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls);
    void registerBedDefenseNatives(JNIEnv* env, jclass cls);
    void registerAudioNatives(JNIEnv* env, jclass cls);
    void registerSpoofNatives(JNIEnv* env, jclass cls);
}
