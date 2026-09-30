#include "EntityAPI_Bridge.h"
#include "PluginLoader.h"
#include "../Java.h"
#include <cmath>

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

static jobject getWorld(JNIEnv* env) {
    jobject mc = getMinecraft(env);
    if (!mc) return nullptr;
    jclass mcClass = env->GetObjectClass(mc);
    jfieldID worldField = lc->GetFieldID(mcClass, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;", "field_71441_e", "f");
    env->DeleteLocalRef(mcClass);
    if (!worldField) { env->DeleteLocalRef(mc); return nullptr; }
    jobject world = env->GetObjectField(mc, worldField);
    env->DeleteLocalRef(mc);
    return world;
}

static jint getEntityIdFromJava(JNIEnv* env, jobject self) {
    jclass cls = env->GetObjectClass(self);
    jfieldID field = env->GetFieldID(cls, "entityId", "I");
    jint id = field ? env->GetIntField(self, field) : 0;
    env->DeleteLocalRef(cls);
    return id;
}

static jobject getMCEntity(JNIEnv* env, jobject self) {
    jint id = getEntityIdFromJava(env, self);
    jobject player = getPlayer(env);
    if (player) {
        jclass playerCls = env->GetObjectClass(player);
        jmethodID getId = lc->GetMethodID(playerCls, "getEntityId", "()I", "func_145782_y", "F");
        if (getId && env->CallIntMethod(player, getId) == id) {
            env->DeleteLocalRef(playerCls);
            return player;
        }
        env->DeleteLocalRef(playerCls);
        env->DeleteLocalRef(player);
    }
    jobject world = getWorld(env);
    if (!world) return nullptr;
    jclass worldCls = env->GetObjectClass(world);
    jmethodID getEnt = lc->GetMethodID(worldCls, "getEntityByID", "(I)Lnet/minecraft/entity/Entity;", "func_73045_a", "a");
    jobject ent = getEnt ? env->CallObjectMethod(world, getEnt, id) : nullptr;
    env->DeleteLocalRef(worldCls);
    env->DeleteLocalRef(world);
    return ent;
}

static jobject createVec3(JNIEnv* env, double x, double y, double z) {
    jclass cls = PluginLoader::loadAPIClass(env, "net.ovson.api.model.Vec3");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(DDD)V");
    if (!ctor) { env->DeleteLocalRef(cls); return nullptr; }
    jobject res = env->NewObject(cls, ctor, x, y, z);
    env->DeleteLocalRef(cls);
    return res;
}

static jstring JNICALL getName(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return env->NewStringUTF("Unknown");
    jclass entCls = env->GetObjectClass(mcEnt);
    
    jmethodID getProfile = lc->GetMethodID(entCls, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;", "func_146103_bH", "eQ");
    if (getProfile) {
        jobject profile = env->CallObjectMethod(mcEnt, getProfile);
        if (profile) {
            jclass profCls = env->GetObjectClass(profile);
            jmethodID getPName = env->GetMethodID(profCls, "getName", "()Ljava/lang/String;");
            jstring name = getPName ? (jstring)env->CallObjectMethod(profile, getPName) : nullptr;
            env->DeleteLocalRef(profCls);
            env->DeleteLocalRef(profile);
            if (name) {
                env->DeleteLocalRef(entCls);
                env->DeleteLocalRef(mcEnt);
                return name;
            }
        }
    }
    
    jmethodID getNameM = lc->GetMethodID(entCls, "getName", "()Ljava/lang/String;", "func_70005_c_", "e_");
    jstring name = getNameM ? (jstring)env->CallObjectMethod(mcEnt, getNameM) : env->NewStringUTF("Unknown");
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return name;
}

static jstring JNICALL getType(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return env->NewStringUTF("Entity");
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID getClsM = env->GetMethodID(entCls, "getClass", "()Ljava/lang/Class;");
    jobject clsObj = env->CallObjectMethod(mcEnt, getClsM);
    jclass cCls = env->GetObjectClass(clsObj);
    jmethodID getSimpleName = env->GetMethodID(cCls, "getSimpleName", "()Ljava/lang/String;");
    jstring name = (jstring)env->CallObjectMethod(clsObj, getSimpleName);
    env->DeleteLocalRef(cCls);
    env->DeleteLocalRef(clsObj);
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return name ? name : env->NewStringUTF("Entity");
}

static jstring JNICALL getDisplayName(JNIEnv* env, jobject self) {
    return getName(env, self);
}

static jstring JNICALL getCustomNameTag(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return env->NewStringUTF("");
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID getTag = lc->GetMethodID(entCls, "getCustomNameTag", "()Ljava/lang/String;", "func_94057_bL", "aM");
    jstring tag = getTag ? (jstring)env->CallObjectMethod(mcEnt, getTag) : env->NewStringUTF("");
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return tag ? tag : env->NewStringUTF("");
}

static jstring JNICALL getUUID(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return env->NewStringUTF("");
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID getUUIDM = lc->GetMethodID(entCls, "getUniqueID", "()Ljava/util/UUID;", "func_110124_au", "aK");
    if (!getUUIDM) {
        env->DeleteLocalRef(entCls);
        env->DeleteLocalRef(mcEnt);
        return env->NewStringUTF("");
    }
    jobject uuidObj = env->CallObjectMethod(mcEnt, getUUIDM);
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    if (!uuidObj) return env->NewStringUTF("");
    jclass uuidCls = env->GetObjectClass(uuidObj);
    jmethodID toStr = env->GetMethodID(uuidCls, "toString", "()Ljava/lang/String;");
    jstring str = (jstring)env->CallObjectMethod(uuidObj, toStr);
    env->DeleteLocalRef(uuidCls);
    env->DeleteLocalRef(uuidObj);
    return str ? str : env->NewStringUTF("");
}

static jboolean JNICALL isLiving(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass livingCls = lc->GetClass("net.minecraft.entity.EntityLivingBase");
    jboolean res = livingCls ? env->IsInstanceOf(mcEnt, livingCls) : JNI_FALSE;
    env->DeleteLocalRef(mcEnt);
    return res;
}

static jboolean JNICALL isPlayer(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass playerCls = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
    jboolean res = playerCls ? env->IsInstanceOf(mcEnt, playerCls) : JNI_FALSE;
    env->DeleteLocalRef(mcEnt);
    return res;
}

static jboolean JNICALL isUser(JNIEnv* env, jobject self) {
    jint id = getEntityIdFromJava(env, self);
    jobject player = getPlayer(env);
    if (!player) return JNI_FALSE;
    jclass playerCls = env->GetObjectClass(player);
    jmethodID getId = lc->GetMethodID(playerCls, "getEntityId", "()I", "func_145782_y", "F");
    jboolean res = (getId && env->CallIntMethod(player, getId) == id) ? JNI_TRUE : JNI_FALSE;
    env->DeleteLocalRef(playerCls);
    env->DeleteLocalRef(player);
    return res;
}

static jboolean JNICALL isDead(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_TRUE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "isDead", "Z", "field_70128_L", "K");
    jboolean val = f ? env->GetBooleanField(mcEnt, f) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL onGround(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "onGround", "Z", "field_70122_E", "C");
    jboolean val = f ? env->GetBooleanField(mcEnt, f) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isCollided(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "isCollided", "Z", "field_70132_H", "D");
    jboolean val = f ? env->GetBooleanField(mcEnt, f) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isCollidedHorizontally(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "isCollidedHorizontally", "Z", "field_70123_F", "E");
    jboolean val = f ? env->GetBooleanField(mcEnt, f) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isCollidedVertically(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "isCollidedVertically", "Z", "field_70124_G", "F");
    jboolean val = f ? env->GetBooleanField(mcEnt, f) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL inWater(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isInWater", "()Z", "func_70090_H", "V");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL inLava(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "handleLavaMovement", "()Z", "func_71061_d_", "ab");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isSprinting(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isSprinting", "()Z", "func_70051_ag", "ai");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isSneaking(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isSneaking", "()Z", "func_70093_af", "ah");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isUsingItem(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isUsingItem", "()Z", "func_71039_bw", "bX");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isBurning(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isBurning", "()Z", "func_70027_ad", "ae");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isInvisible(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isInvisible", "()Z", "func_82150_aj", "ax");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isSleeping(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isPlayerSleeping", "()Z", "func_70608_bn", "cc");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isCreative(JNIEnv* env, jobject self) {
    return JNI_FALSE;
}

static jboolean JNICALL isRiding(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "ridingEntity", "Lnet/minecraft/entity/Entity;", "field_70153_n", "m");
    jobject riding = f ? env->GetObjectField(mcEnt, f) : nullptr;
    jboolean val = riding != nullptr ? JNI_TRUE : JNI_FALSE;
    if (riding) env->DeleteLocalRef(riding);
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isOnLadder(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return JNI_FALSE;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "isOnLadder", "()Z", "func_70617_f_", "h_");
    jboolean val = m ? env->CallBooleanMethod(mcEnt, m) : JNI_FALSE;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jboolean JNICALL isOnEdge(JNIEnv* env, jobject self) {
    return JNI_FALSE;
}

static jfloat JNICALL getHealth(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0.0f;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "getHealth", "()F", "func_110143_aJ", "bn");
    jfloat val = m ? env->CallFloatMethod(mcEnt, m) : 20.0f;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jfloat JNICALL getMaxHealth(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 20.0f;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "getMaxHealth", "()F", "func_110138_aP", "bu");
    jfloat val = m ? env->CallFloatMethod(mcEnt, m) : 20.0f;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jfloat JNICALL getAbsorption(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0.0f;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "getAbsorptionAmount", "()F", "func_110139_bj", "bO");
    jfloat val = m ? env->CallFloatMethod(mcEnt, m) : 0.0f;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jint JNICALL getHurtTime(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "hurtTime", "I", "field_70737_aN", "au");
    jint val = f ? env->GetIntField(mcEnt, f) : 0;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jint JNICALL getMaxHurtTime(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "maxHurtTime", "I", "field_70738_aO", "av");
    jint val = f ? env->GetIntField(mcEnt, f) : 0;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jint JNICALL getHunger(JNIEnv* env, jobject self) {
    return 20;
}

static jfloat JNICALL getSaturation(JNIEnv* env, jobject self) {
    return 5.0f;
}

static jint JNICALL getAir(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 300;
    jclass entCls = env->GetObjectClass(mcEnt);
    jmethodID m = lc->GetMethodID(entCls, "getAir", "()I", "func_70086_ai", "aF");
    jint val = m ? env->CallIntMethod(mcEnt, m) : 300;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jfloat JNICALL getExperience(JNIEnv* env, jobject self) { return 0.0f; }
static jint JNICALL getExperienceLevel(JNIEnv* env, jobject self) { return 0; }
static jfloat JNICALL getFallDistance(JNIEnv* env, jobject self) { return 0.0f; }
static jint JNICALL getTicksExisted(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID fTicks = lc->GetFieldID(entCls, "ticksExisted", "I", "field_70173_aa", "W");
    jint ticks = fTicks ? env->GetIntField(mcEnt, fTicks) : 0;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return ticks;
}

static jobject JNICALL getPosition(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return createVec3(env, 0, 0, 0);
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID fx = lc->GetFieldID(entCls, "posX", "D", "field_70165_t", "s");
    jfieldID fy = lc->GetFieldID(entCls, "posY", "D", "field_70163_u", "t");
    jfieldID fz = lc->GetFieldID(entCls, "posZ", "D", "field_70161_v", "u");
    double x = fx ? env->GetDoubleField(mcEnt, fx) : 0.0;
    double y = fy ? env->GetDoubleField(mcEnt, fy) : 0.0;
    double z = fz ? env->GetDoubleField(mcEnt, fz) : 0.0;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return createVec3(env, x, y, z);
}

static jobject JNICALL getLastPosition(JNIEnv* env, jobject self) { return getPosition(env, self); }
static jobject JNICALL getServerPosition(JNIEnv* env, jobject self) { return getPosition(env, self); }
static jobject JNICALL getBlockPosition(JNIEnv* env, jobject self) { return getPosition(env, self); }
static jobject JNICALL getMotion(JNIEnv* env, jobject self) { return createVec3(env, 0, 0, 0); }
static void JNICALL setMotion(JNIEnv* env, jobject self, jdouble x, jdouble y, jdouble z) {}
static void JNICALL setPosition(JNIEnv* env, jobject self, jdouble x, jdouble y, jdouble z) {}
static jdouble JNICALL getSpeed(JNIEnv* env, jobject self) { return 0.0; }
static jdouble JNICALL getBPS(JNIEnv* env, jobject self) { return 0.0; }

static jfloat JNICALL getYaw(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0.0f;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "rotationYaw", "F", "field_70177_z", "y");
    jfloat val = f ? env->GetFloatField(mcEnt, f) : 0.0f;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static jfloat JNICALL getPitch(JNIEnv* env, jobject self) {
    jobject mcEnt = getMCEntity(env, self);
    if (!mcEnt) return 0.0f;
    jclass entCls = env->GetObjectClass(mcEnt);
    jfieldID f = lc->GetFieldID(entCls, "rotationPitch", "F", "field_70125_A", "z");
    jfloat val = f ? env->GetFloatField(mcEnt, f) : 0.0f;
    env->DeleteLocalRef(entCls);
    env->DeleteLocalRef(mcEnt);
    return val;
}

static void JNICALL setYaw(JNIEnv* env, jobject self, jfloat yaw) {}
static void JNICALL setPitch(JNIEnv* env, jobject self, jfloat pitch) {}
static void JNICALL moveTo(JNIEnv* env, jobject self, jdouble x, jdouble y, jdouble z) {}
static jobject JNICALL getHeldItem(JNIEnv* env, jobject self) { return nullptr; }
static jobject JNICALL getArmorInSlot(JNIEnv* env, jobject self, jint slot) { return nullptr; }
static jboolean JNICALL isHoldingWeapon(JNIEnv* env, jobject self) { return JNI_FALSE; }
static jboolean JNICALL isHoldingBlock(JNIEnv* env, jobject self) { return JNI_FALSE; }

static jobject JNICALL getPotionEffects(JNIEnv* env, jobject self) {
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
    jobject list = env->NewObject(arrayListClass, init);
    env->DeleteLocalRef(arrayListClass);
    return list;
}

static jobject JNICALL getRidingEntity(JNIEnv* env, jobject self) { return nullptr; }
static jobject JNICALL getRiddenByEntity(JNIEnv* env, jobject self) { return nullptr; }
static jfloat JNICALL getEyeHeight(JNIEnv* env, jobject self) { return 1.62f; }
static jobject JNICALL getEyePosition(JNIEnv* env, jobject self) { return getPosition(env, self); }

static jdouble JNICALL getDistanceToEntity(JNIEnv* env, jobject self, jobject other) { return 0.0; }
static jdouble JNICALL getDistanceToVec(JNIEnv* env, jobject self, jobject pos) { return 0.0; }
static jboolean JNICALL canSee(JNIEnv* env, jobject self, jobject other) { return JNI_TRUE; }

static jfloatArray JNICALL getRotationsToEntity(JNIEnv* env, jobject self, jobject other) {
    jfloatArray arr = env->NewFloatArray(2);
    jfloat rots[2] = {0.0f, 0.0f};
    env->SetFloatArrayRegion(arr, 0, 2, rots);
    return arr;
}

static jfloatArray JNICALL getRotationsToVec(JNIEnv* env, jobject self, jobject pos) {
    jfloatArray arr = env->NewFloatArray(2);
    jfloat rots[2] = {0.0f, 0.0f};
    env->SetFloatArrayRegion(arr, 0, 2, rots);
    return arr;
}

namespace EntityAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls) {
        static JNINativeMethod methods[] = {
            {(char*)"getType", (char*)"()Ljava/lang/String;", (void*)getType},
            {(char*)"getName", (char*)"()Ljava/lang/String;", (void*)getName},
            {(char*)"getDisplayName", (char*)"()Ljava/lang/String;", (void*)getDisplayName},
            {(char*)"getCustomNameTag", (char*)"()Ljava/lang/String;", (void*)getCustomNameTag},
            {(char*)"getUUID", (char*)"()Ljava/lang/String;", (void*)getUUID},
            {(char*)"isLiving", (char*)"()Z", (void*)isLiving},
            {(char*)"isPlayer", (char*)"()Z", (void*)isPlayer},
            {(char*)"isUser", (char*)"()Z", (void*)isUser},
            {(char*)"isDead", (char*)"()Z", (void*)isDead},
            {(char*)"onGround", (char*)"()Z", (void*)onGround},
            {(char*)"isCollided", (char*)"()Z", (void*)isCollided},
            {(char*)"isCollidedHorizontally", (char*)"()Z", (void*)isCollidedHorizontally},
            {(char*)"isCollidedVertically", (char*)"()Z", (void*)isCollidedVertically},
            {(char*)"inWater", (char*)"()Z", (void*)inWater},
            {(char*)"inLava", (char*)"()Z", (void*)inLava},
            {(char*)"isSprinting", (char*)"()Z", (void*)isSprinting},
            {(char*)"isSneaking", (char*)"()Z", (void*)isSneaking},
            {(char*)"isUsingItem", (char*)"()Z", (void*)isUsingItem},
            {(char*)"isBurning", (char*)"()Z", (void*)isBurning},
            {(char*)"isInvisible", (char*)"()Z", (void*)isInvisible},
            {(char*)"isSleeping", (char*)"()Z", (void*)isSleeping},
            {(char*)"isCreative", (char*)"()Z", (void*)isCreative},
            {(char*)"isRiding", (char*)"()Z", (void*)isRiding},
            {(char*)"isOnLadder", (char*)"()Z", (void*)isOnLadder},
            {(char*)"isOnEdge", (char*)"()Z", (void*)isOnEdge},
            {(char*)"getHealth", (char*)"()F", (void*)getHealth},
            {(char*)"getMaxHealth", (char*)"()F", (void*)getMaxHealth},
            {(char*)"getAbsorption", (char*)"()F", (void*)getAbsorption},
            {(char*)"getHurtTime", (char*)"()I", (void*)getHurtTime},
            {(char*)"getMaxHurtTime", (char*)"()I", (void*)getMaxHurtTime},
            {(char*)"getHunger", (char*)"()I", (void*)getHunger},
            {(char*)"getSaturation", (char*)"()F", (void*)getSaturation},
            {(char*)"getAir", (char*)"()I", (void*)getAir},
            {(char*)"getExperience", (char*)"()F", (void*)getExperience},
            {(char*)"getExperienceLevel", (char*)"()I", (void*)getExperienceLevel},
            {(char*)"getFallDistance", (char*)"()F", (void*)getFallDistance},
            {(char*)"getTicksExisted", (char*)"()I", (void*)getTicksExisted},
            {(char*)"getPosition", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getPosition},
            {(char*)"getLastPosition", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getLastPosition},
            {(char*)"getServerPosition", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getServerPosition},
            {(char*)"getBlockPosition", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getBlockPosition},
            {(char*)"getMotion", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getMotion},
            {(char*)"setMotion", (char*)"(DDD)V", (void*)setMotion},
            {(char*)"setPosition", (char*)"(DDD)V", (void*)setPosition},
            {(char*)"getSpeed", (char*)"()D", (void*)getSpeed},
            {(char*)"getBPS", (char*)"()D", (void*)getBPS},
            {(char*)"getYaw", (char*)"()F", (void*)getYaw},
            {(char*)"getPitch", (char*)"()F", (void*)getPitch},
            {(char*)"setYaw", (char*)"(F)V", (void*)setYaw},
            {(char*)"setPitch", (char*)"(F)V", (void*)setPitch},
            {(char*)"moveTo", (char*)"(DDD)V", (void*)moveTo},
            {(char*)"getHeldItem", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getHeldItem},
            {(char*)"getArmorInSlot", (char*)"(I)Lnet/ovson/api/model/ItemStack;", (void*)getArmorInSlot},
            {(char*)"isHoldingWeapon", (char*)"()Z", (void*)isHoldingWeapon},
            {(char*)"isHoldingBlock", (char*)"()Z", (void*)isHoldingBlock},
            {(char*)"getPotionEffects", (char*)"()Ljava/util/List;", (void*)getPotionEffects},
            {(char*)"getRidingEntity", (char*)"()Lnet/ovson/api/model/Entity;", (void*)getRidingEntity},
            {(char*)"getRiddenByEntity", (char*)"()Lnet/ovson/api/model/Entity;", (void*)getRiddenByEntity},
            {(char*)"getEyeHeight", (char*)"()F", (void*)getEyeHeight},
            {(char*)"getEyePosition", (char*)"()Lnet/ovson/api/model/Vec3;", (void*)getEyePosition},
            {(char*)"getDistanceTo", (char*)"(Lnet/ovson/api/model/Entity;)D", (void*)getDistanceToEntity},
            {(char*)"getDistanceTo", (char*)"(Lnet/ovson/api/model/Vec3;)D", (void*)getDistanceToVec},
            {(char*)"canSee", (char*)"(Lnet/ovson/api/model/Entity;)Z", (void*)canSee},
            {(char*)"getRotationsTo", (char*)"(Lnet/ovson/api/model/Entity;)[F", (void*)getRotationsToEntity},
            {(char*)"getRotationsTo", (char*)"(Lnet/ovson/api/model/Vec3;)[F", (void*)getRotationsToVec}
        };
        env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    }
}
