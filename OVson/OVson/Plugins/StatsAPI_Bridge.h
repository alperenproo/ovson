#pragma once
#include <jni.h>

namespace StatsAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls);
    void registerAnticheatNatives(JNIEnv* env, jclass cls);
}
