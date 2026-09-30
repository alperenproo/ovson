#pragma once
#include <jni.h>

namespace MojangCape {
    void tick(JNIEnv* env);
    void reset();
    const char* getStyleName(int style = 0);
    int getStyleCount();
    bool hasAccountCape();
}
