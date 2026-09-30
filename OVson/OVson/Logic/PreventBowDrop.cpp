#include "PreventBowDrop.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Render/NotificationManager.h"

namespace PreventBowDrop {

static jmethodID s_m_getHeldItem = nullptr;
static jclass s_itemBowCls = nullptr;
static bool s_fieldsInited = false;

static void initFields(JNIEnv* env) {
    if (s_fieldsInited || !env) return;
    jclass elbCls = lc->GetClass("net.minecraft.entity.EntityLivingBase");
    if (elbCls) {
        s_m_getHeldItem = lc->GetMethodID(elbCls, "getHeldItem", "()Lnet/minecraft/item/ItemStack;", "func_70694_bm", "bA");
    }
    s_itemBowCls = lc->GetClass("net.minecraft.item.ItemBow");
    s_fieldsInited = true;
}

static jobject getMinecraft(JNIEnv* env) {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return nullptr;
    jmethodID getMcM = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (!getMcM) return nullptr;
    return env->CallStaticObjectMethod(mcCls, getMcM);
}

bool isHoldingBow(JNIEnv* env) {
    if (!env) return false;
    initFields(env);
    if (!s_m_getHeldItem) return false;

    jobject mc = getMinecraft(env);
    if (!mc) return false;

    jclass mcCls = env->GetObjectClass(mc);
    jfieldID fPlayer = lc->GetFieldID(mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h");
    env->DeleteLocalRef(mcCls);
    if (!fPlayer) {
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject player = env->GetObjectField(mc, fPlayer);
    env->DeleteLocalRef(mc);
    if (!player) return false;

    jobject itemStack = env->CallObjectMethod(player, s_m_getHeldItem);
    env->DeleteLocalRef(player);
    if (!itemStack) return false;

    jclass isCls = env->GetObjectClass(itemStack);
    jmethodID getItemM = lc->GetMethodID(isCls, "getItem", "()Lnet/minecraft/item/Item;", "func_77973_b", "b");
    env->DeleteLocalRef(isCls);
    if (!getItemM) {
        env->DeleteLocalRef(itemStack);
        return false;
    }

    jobject item = env->CallObjectMethod(itemStack, getItemM);
    env->DeleteLocalRef(itemStack);
    if (!item) return false;

    bool isBow = false;
    if (s_itemBowCls && env->IsInstanceOf(item, s_itemBowCls)) {
        isBow = true;
    }
    if (!isBow) {
        jclass itemClass = env->GetObjectClass(item);
        jmethodID getIdM = lc->GetStaticMethodID(itemClass, "getIdFromItem", "(Lnet/minecraft/item/Item;)I", "func_150891_b", "b", "(Lzw;)I");
        if (getIdM) {
            jint id = env->CallStaticIntMethod(itemClass, getIdM, item);
            if (id == 261) isBow = true;
        }
        if (!isBow) {
            jmethodID getUnlocM = lc->GetMethodID(itemClass, "getUnlocalizedName", "()Ljava/lang/String;", "func_77658_a", "a");
            if (getUnlocM) {
                jstring jName = (jstring)env->CallObjectMethod(item, getUnlocM);
                if (jName) {
                    const char* chars = env->GetStringUTFChars(jName, nullptr);
                    if (chars) {
                        std::string s = chars;
                        env->ReleaseStringUTFChars(jName, chars);
                        if (s.find("bow") != std::string::npos) isBow = true;
                    }
                    env->DeleteLocalRef(jName);
                }
            }
        }
        env->DeleteLocalRef(itemClass);
    }
    env->DeleteLocalRef(item);
    return isBow;
}

bool shouldBlockDropKey(int vkCode) {
    if (!Config::isPreventBowDropEnabled()) return false;
    // default Minecraft drop key is 'q' (0x51)
    if (vkCode != 'Q' && vkCode != 0x51) return false;

    JNIEnv* env = lc->getEnv();
    if (!env) return false;

    if (isHoldingBow(env)) {
        Render::NotificationManager::getInstance()->add(
            "Bow Drop", "Prevented dropping bow!", Render::NotificationType::Warning);
        return true;
    }

    return false;
}

} // namespace PreventBowDrop
