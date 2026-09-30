#include "FootstepMuter.h"
#include "FootstepDebug.h"
#include "../Config/Config.h"
#include "../Java.h"

namespace FootstepMuter {

static jfieldID s_f_nextStepDistance = nullptr;
static jfieldID s_f_distanceWalked = nullptr;
static bool s_fieldsInited = false;
static bool s_wasMuted = false;

static void initFields(JNIEnv* env) {
    if (s_fieldsInited || !env) return;
    jclass entCls = lc->GetClass("net.minecraft.entity.Entity");
    if (!entCls) return;

    s_f_nextStepDistance = lc->GetFieldID(entCls, "nextStepDistance", "I", "field_70150_b", "h");
    s_f_distanceWalked = lc->GetFieldID(entCls, "distanceWalkedOnStepModified", "F", "field_82151_R", "N");
    if (!s_f_distanceWalked) {
        s_f_distanceWalked = lc->GetFieldID(entCls, "distanceWalkedOnStepModified", "F", "field_82151_R", "M");
    }
    s_fieldsInited = true;
    FootstepDebug::log("Footstep fields initialized: nextStepDistance=%p, distanceWalkedOnStepModified=%p",
        s_f_nextStepDistance, s_f_distanceWalked);
}

static jobject getLocalPlayer(JNIEnv* env) {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return nullptr;

    jmethodID getMcM = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (!getMcM) return nullptr;

    jobject mc = env->CallStaticObjectMethod(mcCls, getMcM);
    if (!mc) return nullptr;

    jclass mcObjCls = env->GetObjectClass(mc);
    jfieldID fPlayer = lc->GetFieldID(mcObjCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h");
    env->DeleteLocalRef(mcObjCls);
    if (!fPlayer) {
        env->DeleteLocalRef(mc);
        return nullptr;
    }

    jobject player = env->GetObjectField(mc, fPlayer);
    env->DeleteLocalRef(mc);
    return player;
}

void initialize(JNIEnv* env) {
    if (!env) return;
    initFields(env);
    if (!s_f_nextStepDistance || !s_f_distanceWalked) return;

    jobject player = getLocalPlayer(env);
    if (player) {
        jint currentNext = env->GetIntField(player, s_f_nextStepDistance);
        jfloat dist = env->GetFloatField(player, s_f_distanceWalked);
        if (currentNext > (static_cast<jint>(dist) + 50)) {
            jint healed = static_cast<jint>(dist) + 1;
            env->SetIntField(player, s_f_nextStepDistance, healed);
            FootstepDebug::log("Healed corrupted nextStepDistance (%d -> %d) at dist %.2f", currentNext, healed, dist);
        }
        env->DeleteLocalRef(player);
    }
}

void cleanup(JNIEnv* env) {
    if (!env) return;
    initFields(env);
    if (!s_f_nextStepDistance || !s_f_distanceWalked) return;

    jobject player = getLocalPlayer(env);
    if (player) {
        jfloat dist = env->GetFloatField(player, s_f_distanceWalked);
        jint normalNext = static_cast<jint>(dist) + 1;
        env->SetIntField(player, s_f_nextStepDistance, normalNext);
        FootstepDebug::log("Cleanup: restored nextStepDistance to %d (dist: %.2f)", normalNext, dist);
        env->DeleteLocalRef(player);
    }
    s_wasMuted = false;
}

void tick(JNIEnv* env) {
    if (!env) return;
    try {
        if (env->ExceptionCheck()) env->ExceptionClear();
        const bool enabled = Config::isMuteOwnStepsEnabled();
        if (!enabled && !s_wasMuted) return;

        initFields(env);
        if (!s_f_nextStepDistance || !s_f_distanceWalked) return;

        if (enabled != s_wasMuted) {
            FootstepDebug::logStateChange(enabled);
        }

        jobject player = getLocalPlayer(env);
        if (!player) return;

        jfloat dist = env->GetFloatField(player, s_f_distanceWalked);
        jint currentNext = env->GetIntField(player, s_f_nextStepDistance);

        if (enabled) {
            jint targetNext = static_cast<jint>(dist) + 20;
            env->SetIntField(player, s_f_nextStepDistance, targetNext);
            s_wasMuted = true;
        } else if (s_wasMuted || currentNext > (static_cast<jint>(dist) + 30)) {
            jint restoredNext = static_cast<jint>(dist) + 1;
            env->SetIntField(player, s_f_nextStepDistance, restoredNext);
            FootstepDebug::log("Unmuted: restored nextStepDistance from %d to %d (dist: %.2f)", currentNext, restoredNext, dist);
            s_wasMuted = false;
        }

        env->DeleteLocalRef(player);
        if (env->ExceptionCheck()) env->ExceptionClear();
    } catch (...) {}
}

} // namespace FootstepMuter
