#pragma once
#include <jni.h>

namespace PacketAPIBridge {
    void registerFactoryNatives(JNIEnv* env, jclass cls);
    void registerHelperNatives(JNIEnv* env, jclass cls);
}
