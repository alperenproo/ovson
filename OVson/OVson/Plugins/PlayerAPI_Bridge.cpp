#include "PlayerAPI_Bridge.h"
#include "../Java.h"
#include "PluginLoader.h"

static jobject getMinecraft(JNIEnv* env) {
    jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcClass) return nullptr;
    jmethodID getMinecraft = lc->GetStaticMethodID(mcClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (!getMinecraft) return nullptr;
    return env->CallStaticObjectMethod(mcClass, getMinecraft);
}

static jobject getPlayer(JNIEnv* env) {
    jobject mc = getMinecraft(env);
    if (!mc) return nullptr;
    jclass mcClass = env->GetObjectClass(mc);
    jfieldID playerField = lc->GetFieldID(mcClass, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h");
    env->DeleteLocalRef(mcClass);
    if (!playerField) { env->DeleteLocalRef(mc); return nullptr; }
    jobject player = env->GetObjectField(mc, playerField);
    env->DeleteLocalRef(mc);
    return player;
}

static jdouble JNICALL getX(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "posX", "D", "field_70165_t", "s");
    jdouble val = field ? env->GetDoubleField(player, field) : 0.0;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static jdouble JNICALL getY(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "posY", "D", "field_70163_u", "t");
    jdouble val = field ? env->GetDoubleField(player, field) : 0.0;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static jdouble JNICALL getZ(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "posZ", "D", "field_70161_v", "u");
    jdouble val = field ? env->GetDoubleField(player, field) : 0.0;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static jfloat JNICALL getHealth(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0f;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "getHealth", "()F", "func_110143_aJ", "bn");
    jfloat val = method ? env->CallFloatMethod(player, method) : 0.0f;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static void JNICALL setMotion(JNIEnv* env, jobject obj, jdouble x, jdouble y, jdouble z) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID mx = lc->GetFieldID(playerClass, "motionX", "D", "field_70159_w", "v");
    jfieldID my = lc->GetFieldID(playerClass, "motionY", "D", "field_70181_x", "w");
    jfieldID mz = lc->GetFieldID(playerClass, "motionZ", "D", "field_70179_y", "x");
    if (mx) env->SetDoubleField(player, mx, x);
    if (my) env->SetDoubleField(player, my, y);
    if (mz) env->SetDoubleField(player, mz, z);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static void JNICALL swingItem(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "swingItem", "()V", "func_71038_o", "bx");
    if (method) env->CallVoidMethod(player, method);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static jboolean JNICALL onGround(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return JNI_FALSE;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "onGround", "Z", "field_70122_E", "C");
    jboolean val = field ? env->GetBooleanField(player, field) : JNI_FALSE;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static jobject JNICALL getMotion(JNIEnv* env, jobject obj) {
    // TODO: implement with Vec3
    return nullptr;
}

static jfloat JNICALL getYaw(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0f;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "rotationYaw", "F", "field_70177_z", "y");
    jfloat val = field ? env->GetFloatField(player, field) : 0.0f;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static jfloat JNICALL getPitch(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return 0.0f;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "rotationPitch", "F", "field_70125_A", "z");
    jfloat val = field ? env->GetFloatField(player, field) : 0.0f;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static void JNICALL setYaw(JNIEnv* env, jobject obj, jfloat yaw) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "rotationYaw", "F", "field_70177_z", "y");
    if (field) env->SetFloatField(player, field, yaw);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static void JNICALL setPitch(JNIEnv* env, jobject obj, jfloat pitch) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jfieldID field = lc->GetFieldID(playerClass, "rotationPitch", "F", "field_70125_A", "z");
    if (field) env->SetFloatField(player, field, pitch);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static void JNICALL setRotations(JNIEnv* env, jobject obj, jfloat yaw, jfloat pitch) {
    setYaw(env, obj, yaw);
    setPitch(env, obj, pitch);
}

static jboolean JNICALL isSprinting(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return JNI_FALSE;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "isSprinting", "()Z", "func_70051_ag", "ai");
    jboolean val = method ? env->CallBooleanMethod(player, method) : JNI_FALSE;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static void JNICALL setSprinting(JNIEnv* env, jobject obj, jboolean sprinting) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "setSprinting", "(Z)V", "func_70031_b", "d");
    if (method) env->CallVoidMethod(player, method, sprinting);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static jboolean JNICALL isSneaking(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return JNI_FALSE;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "isSneaking", "()Z", "func_70093_af", "ah");
    jboolean val = method ? env->CallBooleanMethod(player, method) : JNI_FALSE;
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    return val;
}

static void JNICALL setSneaking(JNIEnv* env, jobject obj, jboolean sneaking) {
    // TODO: implement movementInput.sneak
}

static jboolean JNICALL isMoving(JNIEnv* env, jobject obj) {
    // TODO: implement movement checking
    return JNI_FALSE;
}

static void JNICALL jump(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return;
    jclass playerClass = env->GetObjectClass(player);
    jmethodID method = lc->GetMethodID(playerClass, "jump", "()V", "func_70664_aZ", "bj");
    if (method) env->CallVoidMethod(player, method);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
}

static jboolean JNICALL isCreative(JNIEnv* env, jobject obj) {
    // TODO: implement capabilities check
    return JNI_FALSE;
}

static jboolean JNICALL isFlying(JNIEnv* env, jobject obj) {
    // TODO: implement capabilities check
    return JNI_FALSE;
}

static void JNICALL setFlying(JNIEnv* env, jobject obj, jboolean flying) {
    // TODO: implement capabilities set
}

static jint JNICALL getFPS(JNIEnv* env, jobject obj) {
    // TODO: implement FPS retrieval
    return 0;
}

static jstring JNICALL getUsername(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return env->NewStringUTF("Unknown");
    jclass playerClass = env->GetObjectClass(player);
    jmethodID getProfile = lc->GetMethodID(playerClass, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;", "func_146103_bH", "eQ");
    if (!getProfile) {
        jmethodID getName = lc->GetMethodID(playerClass, "getName", "()Ljava/lang/String;", "func_70005_c_", "e_");
        if (getName) {
            jstring name = (jstring)env->CallObjectMethod(player, getName);
            env->DeleteLocalRef(playerClass);
            env->DeleteLocalRef(player);
            return name;
        }
        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(player);
        return env->NewStringUTF("Unknown");
    }
    jobject profile = env->CallObjectMethod(player, getProfile);
    env->DeleteLocalRef(playerClass);
    env->DeleteLocalRef(player);
    if (!profile) return env->NewStringUTF("Unknown");
    
    jclass profileClass = env->GetObjectClass(profile);
    jmethodID getNameM = env->GetMethodID(profileClass, "getName", "()Ljava/lang/String;");
    jstring name = getNameM ? (jstring)env->CallObjectMethod(profile, getNameM) : env->NewStringUTF("Unknown");
    env->DeleteLocalRef(profileClass);
    env->DeleteLocalRef(profile);
    return name;
}

static jobject JNICALL getLocalPlayer(JNIEnv* env, jobject obj) {
    jobject player = getPlayer(env);
    if (!player) return nullptr;
    
    jclass entityClass = lc->GetClass("net.minecraft.entity.Entity");
    if (!entityClass) { env->DeleteLocalRef(player); return nullptr; }
    
    jmethodID getEntityId = lc->GetMethodID(entityClass, "getEntityId", "()I", "func_145782_y", "F");
    if (!getEntityId) { env->DeleteLocalRef(player); return nullptr; }
    
    jint entityId = env->CallIntMethod(player, getEntityId);
    env->DeleteLocalRef(player);
    
    jclass apiEntityClass = PluginLoader::loadAPIClass(env, "net.ovson.api.model.Entity");
    if (!apiEntityClass) return nullptr;
    
    jmethodID ctor = env->GetMethodID(apiEntityClass, "<init>", "(I)V");
    if (!ctor) { env->DeleteLocalRef(apiEntityClass); return nullptr; }
    
    jobject entityObj = env->NewObject(apiEntityClass, ctor, entityId);
    env->DeleteLocalRef(apiEntityClass);
    return entityObj;
}

static jstring JNICALL getServerIP(JNIEnv* env, jobject obj) {
    // TODO: implement IP retrieval
    return env->NewStringUTF("Unknown");
}

static jint JNICALL getPing(JNIEnv* env, jobject obj) {
    // TODO: implement ping retrieval
    return 0;
}

static void JNICALL sendPacket(JNIEnv* env, jobject obj, jobject packet) {
    // TODO: implement sendPacket
}

static jintArray JNICALL getDisplaySize(JNIEnv* env, jobject obj) {
    jintArray arr = env->NewIntArray(2);
    jint dims[2] = {0, 0};
    env->SetIntArrayRegion(arr, 0, 2, dims);
    return arr;
}

namespace PlayerAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls) {
        static JNINativeMethod methods[] = {
            {(char*)"getX", (char*)"()D", (void*)getX},
            {(char*)"getY", (char*)"()D", (void*)getY},
            {(char*)"getZ", (char*)"()D", (void*)getZ},
            {(char*)"getHealth", (char*)"()F", (void*)getHealth},
            {(char*)"setMotion", (char*)"(DDD)V", (void*)setMotion},
            {(char*)"swingItem", (char*)"()V", (void*)swingItem},
            {(char*)"onGround", (char*)"()Z", (void*)onGround},
            {(char*)"getMotion", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getMotion},
            {(char*)"getYaw", (char*)"()F", (void*)getYaw},
            {(char*)"getPitch", (char*)"()F", (void*)getPitch},
            {(char*)"setYaw", (char*)"(F)V", (void*)setYaw},
            {(char*)"setPitch", (char*)"(F)V", (void*)setPitch},
            {(char*)"setRotations", (char*)"(FF)V", (void*)setRotations},
            {(char*)"isSprinting", (char*)"()Z", (void*)isSprinting},
            {(char*)"setSprinting", (char*)"(Z)V", (void*)setSprinting},
            {(char*)"isSneaking", (char*)"()Z", (void*)isSneaking},
            {(char*)"setSneaking", (char*)"(Z)V", (void*)setSneaking},
            {(char*)"isMoving", (char*)"()Z", (void*)isMoving},
            {(char*)"jump", (char*)"()V", (void*)jump},
            {(char*)"isCreative", (char*)"()Z", (void*)isCreative},
            {(char*)"isFlying", (char*)"()Z", (void*)isFlying},
            {(char*)"setFlying", (char*)"(Z)V", (void*)setFlying},
            {(char*)"getFPS", (char*)"()I", (void*)getFPS},
            {(char*)"getUsername", (char*)"()Ljava/lang/String;", (void*)getUsername},
            {(char*)"getServerIP", (char*)"()Ljava/lang/String;", (void*)getServerIP},
            {(char*)"getPing", (char*)"()I", (void*)getPing},
            {(char*)"sendPacket", (char*)"(Ljava/lang/Object;)V", (void*)sendPacket},
            {(char*)"getDisplaySize", (char*)"()[I", (void*)getDisplaySize},
            {(char*)"getLocalPlayer", (char*)"()Lnet/ovson/api/model/Entity;", (void*)getLocalPlayer}
        };
        env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    }
}
