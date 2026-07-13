#pragma once
#include "../Java.h"
#include <jni.h>

namespace Mc {

inline jclass minecraftClass() {
    return lc ? lc->GetClass("net.minecraft.client.Minecraft") : nullptr;
}

inline jobject theMinecraft(JNIEnv *env) {
    if (!env || !lc) return nullptr;
    jclass cls = minecraftClass();
    if (!cls) return nullptr;
    static jfieldID fid = nullptr;
    if (!fid) {
        fid = lc->GetStaticFieldID(
            cls, "theMinecraft", "Lnet/minecraft/client/Minecraft;",
            "field_71432_P", "S", "Lave;");
        if (!fid && env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!fid) return nullptr;
    jobject o = env->GetStaticObjectField(cls, fid);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return o;
}

inline jobject gameSettings(JNIEnv *env) {
    if (!env || !lc) return nullptr;
    jobject mc = theMinecraft(env);
    if (!mc) return nullptr;
    jclass cls = minecraftClass();
    static jfieldID fid = nullptr;
    if (!fid) {
        fid = lc->GetFieldID(
            cls, "gameSettings",
            "Lnet/minecraft/client/settings/GameSettings;",
            "field_71474_y", "t", "Lavh;");
        if (!fid && env->ExceptionCheck()) env->ExceptionClear();
        if (!fid)
            fid = lc->FindFieldBySignature(
                cls, "Lnet/minecraft/client/settings/GameSettings;");
        if (!fid) fid = lc->FindFieldBySignature(cls, "Lavh;");
    }
    if (!fid) { env->DeleteLocalRef(mc); return nullptr; }
    jobject gs = env->GetObjectField(mc, fid);
    env->DeleteLocalRef(mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return gs;
}

inline jobject netHandler(JNIEnv *env) {
    if (!env || !lc) return nullptr;
    jobject mc = theMinecraft(env);
    if (!mc) return nullptr;
    jclass cls = minecraftClass();
    static jmethodID mid = nullptr;
    if (!mid) {
        mid = lc->GetMethodID(
            cls, "getNetHandler",
            "()Lnet/minecraft/client/network/NetHandlerPlayClient;",
            "func_147114_u", "ay", "()Lbcy;");
        if (!mid && env->ExceptionCheck()) env->ExceptionClear();
        if (!mid)
            mid = lc->FindMethodBySignature(
                cls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
        if (!mid) mid = lc->FindMethodBySignature(cls, "()Lbcy;");
    }
    if (!mid) { env->DeleteLocalRef(mc); return nullptr; }
    jobject nh = env->CallObjectMethod(mc, mid);
    env->DeleteLocalRef(mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return nh;
}

inline jobject thePlayer(JNIEnv *env) {
    if (!env || !lc) return nullptr;
    jobject mc = theMinecraft(env);
    if (!mc) return nullptr;
    jclass cls = minecraftClass();
    static jfieldID fid = nullptr;
    if (!fid) {
        fid = lc->GetFieldID(
            cls, "thePlayer",
            "Lnet/minecraft/client/entity/EntityPlayerSP;",
            "field_71439_g", "h", "Lbew;");
        if (!fid && env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!fid) { env->DeleteLocalRef(mc); return nullptr; }
    jobject p = env->GetObjectField(mc, fid);
    env->DeleteLocalRef(mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return p;
}

inline jobject theWorld(JNIEnv *env) {
    if (!env || !lc) return nullptr;
    jobject mc = theMinecraft(env);
    if (!mc) return nullptr;
    jclass cls = minecraftClass();
    static jfieldID fid = nullptr;
    if (!fid) {
        fid = lc->GetFieldID(
            cls, "theWorld",
            "Lnet/minecraft/client/multiplayer/WorldClient;",
            "field_71441_e", "f", "Lavk;");
        if (!fid && env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!fid) { env->DeleteLocalRef(mc); return nullptr; }
    jobject w = env->GetObjectField(mc, fid);
    env->DeleteLocalRef(mc);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return w;
}

} // namespace Mc
