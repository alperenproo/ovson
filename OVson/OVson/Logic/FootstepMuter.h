#pragma once

#include <jni.h>

namespace FootstepMuter {
    void initialize(JNIEnv* env);
    void tick(JNIEnv* env);
    void cleanup(JNIEnv* env);
}
