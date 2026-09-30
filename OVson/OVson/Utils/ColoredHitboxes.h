#pragma once
#include <jni.h>
#include <string>
#include <cstdint>

namespace OVson {
namespace Utils {
    uint32_t getTeamColorArgb(const std::string& team);
    uint32_t resolvePlayerTeamColor(JNIEnv* env, jobject entity);
    uint32_t resolveBoxColor(JNIEnv* env, jobject bb, uint32_t originalColor);
    void clearHitboxColorCache();
    void resetBoxColorState(JNIEnv* env);
}
}
