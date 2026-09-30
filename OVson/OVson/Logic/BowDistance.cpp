#include "BowDistance.h"
#include "BowDistanceDebug.h"
#include "../Config/Config.h"
#include "../Java.h"
#include <cmath>
#include <mutex>
#include <cctype>
#include <Windows.h>

namespace BowDistance {

static std::mutex s_bowMutex;
static double s_lastShootX = 0.0;
static double s_lastShootY = 0.0;
static double s_lastShootZ = 0.0;
static ULONGLONG s_lastShootTime = 0;
static bool s_wasUsingBow = false;

static jfieldID s_f_posX = nullptr;
static jfieldID s_f_posY = nullptr;
static jfieldID s_f_posZ = nullptr;
static jmethodID s_m_isUsingItem = nullptr;
static jmethodID s_m_getHeldItem = nullptr;
static bool s_bowFieldsInited = false;

static void initBowFields(JNIEnv* env) {
    if (s_bowFieldsInited || !env) return;

    jclass entCls = lc->GetClass("net.minecraft.entity.Entity");
    if (entCls) {
        s_f_posX = lc->GetFieldID(entCls, "posX", "D", "field_70165_t", "s");
        s_f_posY = lc->GetFieldID(entCls, "posY", "D", "field_70163_u", "t");
        s_f_posZ = lc->GetFieldID(entCls, "posZ", "D", "field_70161_v", "u");
    }

    jclass elbCls = lc->GetClass("net.minecraft.entity.EntityLivingBase");
    if (elbCls) {
        s_m_getHeldItem = lc->GetMethodID(elbCls, "getHeldItem", "()Lnet/minecraft/item/ItemStack;", "func_70694_bm", "bA", "()Lzx;");
    }

    jclass epCls = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
    if (!epCls) epCls = lc->GetClass("net.minecraft.client.entity.EntityPlayerSP");
    if (epCls) {
        s_m_isUsingItem = lc->GetMethodID(epCls, "isUsingItem", "()Z", "func_71039_bw", "bS");
        if (!s_m_isUsingItem) {
            s_m_isUsingItem = lc->GetMethodID(epCls, "isUsingItem", "()Z", "func_71039_bw", "bX");
        }
        if (!s_m_getHeldItem) {
            s_m_getHeldItem = lc->GetMethodID(epCls, "getHeldItem", "()Lnet/minecraft/item/ItemStack;", "func_70694_bm", "bA", "()Lzx;");
        }
    }

    s_bowFieldsInited = true;
    BowDistanceDebug::log("Bow fields initialized: posX=%p, posY=%p, posZ=%p, isUsingItem=%p, getHeldItem=%p",
        s_f_posX, s_f_posY, s_f_posZ, s_m_isUsingItem, s_m_getHeldItem);
}

static jobject getMinecraft(JNIEnv* env) {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return nullptr;
    jmethodID getMcM = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (!getMcM) return nullptr;
    return env->CallStaticObjectMethod(mcCls, getMcM);
}

static bool isHoldingBow(JNIEnv* env, jobject player) {
    if (!env || !player || !s_m_getHeldItem) return false;
    jobject itemStack = env->CallObjectMethod(player, s_m_getHeldItem);
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
    jclass itemClass = env->GetObjectClass(item);

    jmethodID getIdM = lc->GetStaticMethodID(itemClass, "getIdFromItem", "(Lnet/minecraft/item/Item;)I", "func_150891_b", "b", "(Lzw;)I");
    if (getIdM) {
        jint id = env->CallStaticIntMethod(itemClass, getIdM, item);
        if (id == 261) {
            isBow = true;
        }
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
                    if (s.find("bow") != std::string::npos) {
                        isBow = true;
                    }
                }
                env->DeleteLocalRef(jName);
            }
        }
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(itemClass);
    env->DeleteLocalRef(item);
    return isBow;
}

void recordBowRelease(double x, double y, double z) {
    std::lock_guard<std::mutex> lock(s_bowMutex);
    s_lastShootX = x;
    s_lastShootY = y;
    s_lastShootZ = z;
    s_lastShootTime = GetTickCount64();
    BowDistanceDebug::logRelease(x, y, z, s_lastShootTime);
}

void tick(JNIEnv* env) {
    if (!env || !Config::isBowDistanceEnabled()) return;
    initBowFields(env);

    jobject mc = getMinecraft(env);
    if (!mc) return;

    jclass mcCls = env->GetObjectClass(mc);
    jfieldID fPlayer = lc->GetFieldID(mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h");
    env->DeleteLocalRef(mcCls);
    if (!fPlayer) {
        env->DeleteLocalRef(mc);
        return;
    }

    jobject player = env->GetObjectField(mc, fPlayer);
    env->DeleteLocalRef(mc);
    if (!player) return;

    if (!s_m_isUsingItem) {
        jclass playerObjCls = env->GetObjectClass(player);
        if (playerObjCls) {
            s_m_isUsingItem = lc->GetMethodID(playerObjCls, "isUsingItem", "()Z", "func_71039_bw", "bS");
            if (!s_m_isUsingItem) {
                s_m_isUsingItem = lc->GetMethodID(playerObjCls, "isUsingItem", "()Z", "func_71039_bw", "bX");
            }
            env->DeleteLocalRef(playerObjCls);
            if (s_m_isUsingItem) {
                BowDistanceDebug::log("Runtime resolved s_m_isUsingItem: %p", s_m_isUsingItem);
            }
        }
    }

    bool holdingBow = isHoldingBow(env, player);
    bool usingItem = false;
    if (holdingBow && s_m_isUsingItem) {
        usingItem = env->CallBooleanMethod(player, s_m_isUsingItem) == JNI_TRUE;
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    if (s_wasUsingBow && !usingItem && s_f_posX && s_f_posY && s_f_posZ) {
        double px = env->GetDoubleField(player, s_f_posX);
        double py = env->GetDoubleField(player, s_f_posY);
        double pz = env->GetDoubleField(player, s_f_posZ);
        recordBowRelease(px, py, pz);
    }

    s_wasUsingBow = (holdingBow && usingItem);
    env->DeleteLocalRef(player);
}

static std::string stripCodes(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0xC2 && i + 2 < text.size() && (unsigned char)text[i+1] == 0xA7) {
            i += 2;
            continue;
        }
        if (c == 0xA7 && i + 1 < text.size()) {
            i += 1;
            continue;
        }
        result += text[i];
    }
    return result;
}

std::string appendDistanceToJson(const std::string& rawJson, double dist) {
    if (rawJson.empty()) return rawJson;

    char distBuf[64];
    snprintf(distBuf, sizeof(distBuf), "%.1fm", dist);
    std::string node = "{\"text\":\" (\"},{\"text\":\"" + std::string(distBuf) + "\",\"color\":\"green\"},{\"text\":\")\",\"color\":\"gray\"}";

    size_t extraIdx = rawJson.find("\"extra\":");
    if (extraIdx != std::string::npos) {
        size_t bracketOpen = rawJson.find('[', extraIdx);
        if (bracketOpen != std::string::npos) {
            int depth = 0;
            size_t bracketClose = std::string::npos;
            for (size_t i = bracketOpen; i < rawJson.length(); ++i) {
                if (rawJson[i] == '[') depth++;
                else if (rawJson[i] == ']') {
                    depth--;
                    if (depth == 0) {
                        bracketClose = i;
                        break;
                    }
                }
            }
            if (bracketClose != std::string::npos) {
                bool hasElements = false;
                for (size_t i = bracketOpen + 1; i < bracketClose; ++i) {
                    if (!isspace((unsigned char)rawJson[i])) {
                        hasElements = true;
                        break;
                    }
                }
                std::string res = rawJson;
                if (hasElements) {
                    res.insert(bracketClose, "," + node);
                } else {
                    res.insert(bracketClose, node);
                }
                return res;
            }
        }
    }

    size_t lastBrace = rawJson.rfind('}');
    if (lastBrace != std::string::npos) {
        std::string res = rawJson;
        res.insert(lastBrace, ",\"extra\":[" + node + "]");
        return res;
    }

    return rawJson;
}

bool processChat(const std::string& unformatted, const std::string& rawJson, std::string& modifiedJson) {
    if (!Config::isBowDistanceEnabled()) return false;

    std::string clean = stripCodes(unformatted);
    size_t isOnPos = clean.find(" is on ");
    size_t hpPos = clean.rfind(" HP!");
    if (isOnPos == std::string::npos || hpPos == std::string::npos || hpPos <= isOnPos) {
        return false;
    }

    std::string targetName = clean.substr(0, isOnPos);
    while (!targetName.empty() && (targetName.front() == ' ' || targetName.front() == '[')) {
        size_t bracketEnd = targetName.find(']');
        if (bracketEnd != std::string::npos && bracketEnd + 1 < targetName.size()) {
            targetName = targetName.substr(bracketEnd + 1);
        } else {
            break;
        }
    }
    while (!targetName.empty() && targetName.front() == ' ') targetName.erase(targetName.begin());
    while (!targetName.empty() && targetName.back() == ' ') targetName.pop_back();

    if (targetName.empty() || targetName.size() > 16) return false;

    JNIEnv* env = lc->getEnv();
    if (!env) return false;

    jobject mc = getMinecraft(env);
    if (!mc) return false;

    double shootX = 0, shootY = 0, shootZ = 0;
    bool hasShootPos = false;
    {
        std::lock_guard<std::mutex> lock(s_bowMutex);
        if (s_lastShootTime != 0 && (GetTickCount64() - s_lastShootTime) <= 15000) {
            shootX = s_lastShootX;
            shootY = s_lastShootY;
            shootZ = s_lastShootZ;
            hasShootPos = true;
        }
    }

    if (!hasShootPos) {
        jclass mcCls = env->GetObjectClass(mc);
        jfieldID fPlayer = lc->GetFieldID(mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;", "field_71439_g", "h");
        env->DeleteLocalRef(mcCls);
        if (fPlayer) {
            jobject player = env->GetObjectField(mc, fPlayer);
            if (player) {
                if (s_f_posX && s_f_posY && s_f_posZ) {
                    shootX = env->GetDoubleField(player, s_f_posX);
                    shootY = env->GetDoubleField(player, s_f_posY);
                    shootZ = env->GetDoubleField(player, s_f_posZ);
                    hasShootPos = true;
                }
                env->DeleteLocalRef(player);
            }
        }
    }

    if (!hasShootPos) {
        BowDistanceDebug::logChat(unformatted, rawJson, false, targetName, 0.0);
        env->DeleteLocalRef(mc);
        return false;
    }

    jclass mcCls = env->GetObjectClass(mc);
    jfieldID fWorld = lc->GetFieldID(mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;", "field_71441_e", "f");
    env->DeleteLocalRef(mcCls);
    if (!fWorld) {
        env->DeleteLocalRef(mc);
        return false;
    }

    jobject world = env->GetObjectField(mc, fWorld);
    env->DeleteLocalRef(mc);
    if (!world) return false;

    jclass worldCls = env->GetObjectClass(world);
    jfieldID fPlayers = lc->GetFieldID(worldCls, "playerEntities", "Ljava/util/List;", "field_73010_i", "j");
    env->DeleteLocalRef(worldCls);
    if (!fPlayers) {
        env->DeleteLocalRef(world);
        return false;
    }

    jobject playerList = env->GetObjectField(world, fPlayers);
    env->DeleteLocalRef(world);
    if (!playerList) return false;

    jclass listCls = env->GetObjectClass(playerList);
    jmethodID mSize = env->GetMethodID(listCls, "size", "()I");
    jmethodID mGet = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(listCls);
    if (!mSize || !mGet) {
        env->DeleteLocalRef(playerList);
        return false;
    }

    jclass epCls = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
    jmethodID mGetName = epCls ? lc->GetMethodID(epCls, "getName", "()Ljava/lang/String;", "func_70005_c_", "e_") : nullptr;
    if (!mGetName) {
        env->DeleteLocalRef(playerList);
        return false;
    }

    int count = env->CallIntMethod(playerList, mSize);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(playerList);
        return false;
    }

    double targetX = 0, targetY = 0, targetZ = 0;
    bool found = false;

    for (int i = 0; i < count; ++i) {
        jobject ent = env->CallObjectMethod(playerList, mGet, i);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            break;
        }
        if (!ent) continue;

        jstring jName = (jstring)env->CallObjectMethod(ent, mGetName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            env->DeleteLocalRef(ent);
            continue;
        }

        if (jName) {
            const char* nameChars = env->GetStringUTFChars(jName, nullptr);
            if (nameChars) {
                if (_stricmp(nameChars, targetName.c_str()) == 0) {
                    if (s_f_posX && s_f_posY && s_f_posZ) {
                        targetX = env->GetDoubleField(ent, s_f_posX);
                        targetY = env->GetDoubleField(ent, s_f_posY);
                        targetZ = env->GetDoubleField(ent, s_f_posZ);
                        found = true;
                    }
                }
                env->ReleaseStringUTFChars(jName, nameChars);
            }
            env->DeleteLocalRef(jName);
        }
        env->DeleteLocalRef(ent);
        if (found) break;
    }
    env->DeleteLocalRef(playerList);

    if (!found) {
        BowDistanceDebug::logChat(unformatted, rawJson, false, targetName, 0.0);
        return false;
    }

    double dx = targetX - shootX;
    double dy = targetY - shootY;
    double dz = targetZ - shootZ;
    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    dist = std::round(dist * 10.0) / 10.0;

    modifiedJson = appendDistanceToJson(rawJson, dist);
    BowDistanceDebug::logChat(unformatted, rawJson, true, targetName, dist);
    return true;
}

} // namespace BowDistance
