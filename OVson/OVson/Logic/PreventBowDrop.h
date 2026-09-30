#pragma once

#include <jni.h>

namespace PreventBowDrop {
    bool shouldBlockDropKey(int vkCode);
    bool isHoldingBow(JNIEnv* env);
}
