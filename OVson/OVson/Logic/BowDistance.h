#pragma once

#include <string>
#include <jni.h>

namespace BowDistance {
    void tick(JNIEnv* env);
    void recordBowRelease(double x, double y, double z);
    bool processChat(const std::string& unformatted, const std::string& rawJson, std::string& modifiedJson);
    std::string appendDistanceToJson(const std::string& rawJson, double dist);
}
