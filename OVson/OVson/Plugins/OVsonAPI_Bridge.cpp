#include "OVsonAPI_Bridge.h"
#include <iostream>

static jboolean JNICALL isModuleEnabled(JNIEnv* env, jobject obj, jstring name) {
    // TODO: implement actual check
    return JNI_FALSE; 
}

static void JNICALL setModuleEnabled(JNIEnv* env, jobject obj, jstring name, jboolean enabled) {
    // TODO: implement module toggling
}

static jobject JNICALL getPlayerStats(JNIEnv* env, jobject obj, jstring name) {
    // TODO: implement stats fetching
    return nullptr;
}

namespace OVsonAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls) {
        static JNINativeMethod methods[] = {
            {(char*)"isModuleEnabled", (char*)"(Ljava/lang/String;)Z", (void*)isModuleEnabled},
            {(char*)"setModuleEnabled", (char*)"(Ljava/lang/String;Z)V", (void*)setModuleEnabled},
            {(char*)"getPlayerStats", (char*)"(Ljava/lang/String;)Ljava/lang/Object;", (void*)getPlayerStats}
        };
        env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    }
}
