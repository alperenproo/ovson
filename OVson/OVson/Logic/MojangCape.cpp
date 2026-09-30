// this shit crashed the whole thing btw
#include "MojangCape.h"
#include "StatsTracker.internal.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Net/Http.h"
#include "../Utils/Logger.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace MojangCape {

static jobject s_realUserCapeLoc = nullptr;
static jobject s_originalCape = nullptr;
static bool s_hasRealCape = false;
static bool s_capeApplied = false;

static std::atomic<bool> s_fetchingCape{false};
static std::mutex s_capeMutex;
static std::string s_pendingCapeUrl;
static ULONGLONG s_lastFetchAttempt = 0;
static bool s_capeFetchAttempted = false;

static std::string s_sessionUsername;
static std::string s_sessionUuid;

const char* getStyleName(int style) {
    (void)style;
    return s_hasRealCape ? "Account Cape (Detected)" : "Account Cape (Loading...)";
}

int getStyleCount() {
    return 1;
}

bool hasAccountCape() {
    return s_hasRealCape;
}

void reset() {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (env) {
        if (s_originalCape) {
            env->DeleteGlobalRef(s_originalCape);
            s_originalCape = nullptr;
        }
        if (s_realUserCapeLoc) {
            env->DeleteGlobalRef(s_realUserCapeLoc);
            s_realUserCapeLoc = nullptr;
        }
    }
    s_capeApplied = false;
    s_hasRealCape = false;
    s_sessionUsername.clear();
    s_sessionUuid.clear();
    s_capeFetchAttempted = false;
    s_lastFetchAttempt = 0;
    {
        std::lock_guard<std::mutex> lock(s_capeMutex);
        s_pendingCapeUrl.clear();
    }
}

static jobject getMinecraft(JNIEnv* env) {
    if (!env || !lc) return nullptr;
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return nullptr;
    jmethodID getMcM = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
    if (!getMcM) return nullptr;
    return env->CallStaticObjectMethod(mcCls, getMcM);
}

static jobject getPlayer(JNIEnv* env) {
    jobject mc = getMinecraft(env);
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

static std::string base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static void startAsyncCapeFetch(const std::string& uuid) {
    ULONGLONG now = GetTickCount64();
    if (s_capeFetchAttempted && (now - s_lastFetchAttempt < 120000)) {
        return;
    }
    if (s_fetchingCape.exchange(true)) return;
    s_lastFetchAttempt = now;
    s_capeFetchAttempted = true;

    std::thread([uuid]() {
        std::string cleanUuid;
        for (char c : uuid) {
            if (c != '-') cleanUuid += c;
        }
        if (cleanUuid.empty()) {
            s_fetchingCape.store(false);
            return;
        }

        std::string url = "https://sessionserver.mojang.com/session/minecraft/profile/" + cleanUuid;
        std::string body;
        if (Http::get(url, body)) {
            size_t valPos = body.find("\"value\"");
            if (valPos != std::string::npos) {
                size_t q1 = body.find('"', valPos + 7);
                if (q1 != std::string::npos) {
                    size_t q2 = body.find('"', q1 + 1);
                    if (q2 != std::string::npos) {
                        std::string b64 = body.substr(q1 + 1, q2 - (q1 + 1));
                        std::string decoded = base64Decode(b64);
                        size_t capePos = decoded.find("\"CAPE\"");
                        if (capePos != std::string::npos) {
                            size_t urlPos = decoded.find("\"url\"", capePos);
                            if (urlPos != std::string::npos) {
                                size_t uq1 = decoded.find('"', urlPos + 5);
                                if (uq1 != std::string::npos) {
                                    size_t uq2 = decoded.find('"', uq1 + 1);
                                    if (uq2 != std::string::npos) {
                                        std::string capeUrl = decoded.substr(uq1 + 1, uq2 - (uq1 + 1));
                                        std::lock_guard<std::mutex> lock(s_capeMutex);
                                        s_pendingCapeUrl = capeUrl;
                                        Logger::info("[MojangCape] Retrieved authentic Mojang cape URL from API: %s", capeUrl.c_str());
                                    }
                                }
                            }
                        } else {
                            Logger::info("[MojangCape] Mojang session profile contains no cape.");
                        }
                    }
                }
            }
        }
        s_fetchingCape.store(false);
    }).detach();
}

static jobject createTextureFromUrl(JNIEnv* env, const std::string& capeUrl) {
    if (!env || !lc || capeUrl.empty()) return nullptr;

    jobject mc = getMinecraft(env);
    if (!mc) return nullptr;

    jclass mcClass = env->GetObjectClass(mc);
    jmethodID getTexMgr = lc->GetMethodID(mcClass, "getTextureManager", "()Lnet/minecraft/client/renderer/texture/TextureManager;", "func_110434_K", "O", "()Lbmh;");
    env->DeleteLocalRef(mcClass);
    if (!getTexMgr) {
        env->DeleteLocalRef(mc);
        return nullptr;
    }
    jobject texMgr = env->CallObjectMethod(mc, getTexMgr);
    env->DeleteLocalRef(mc);
    if (!texMgr) return nullptr;

    jclass urlCls = env->FindClass("java/net/URL");
    if (!urlCls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }
    jmethodID urlCtor = env->GetMethodID(urlCls, "<init>", "(Ljava/lang/String;)V");
    if (!urlCtor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(urlCls);
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }
    jstring jCapeUrl = env->NewStringUTF(capeUrl.c_str());
    jobject urlObj = env->NewObject(urlCls, urlCtor, jCapeUrl);
    env->DeleteLocalRef(jCapeUrl);
    env->DeleteLocalRef(urlCls);
    if (!urlObj) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }

    jclass imageIOCls = env->FindClass("javax/imageio/ImageIO");
    if (!imageIOCls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }
    jmethodID readM = env->GetStaticMethodID(imageIOCls, "read", "(Ljava/net/URL;)Ljava/awt/image/BufferedImage;");
    if (!readM) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(imageIOCls);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }
    jobject img = env->CallStaticObjectMethod(imageIOCls, readM, urlObj);
    env->DeleteLocalRef(urlObj);
    env->DeleteLocalRef(imageIOCls);
    if (!img) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(texMgr);
        return nullptr;
    }

    jclass imgCls = env->GetObjectClass(img);
    jmethodID getWidthM = env->GetMethodID(imgCls, "getWidth", "()I");
    jmethodID getHeightM = env->GetMethodID(imgCls, "getHeight", "()I");
    int w = getWidthM ? env->CallIntMethod(img, getWidthM) : 0;
    int h = getHeightM ? env->CallIntMethod(img, getHeightM) : 0;

    jobject resultLoc = nullptr;
    if (w > 0 && h > 0) {
        jintArray pixelsArr = env->NewIntArray(w * h);
        jmethodID getRGBM = env->GetMethodID(imgCls, "getRGB", "(IIII[III)[I");
        if (getRGBM && pixelsArr) {
            env->CallObjectMethod(img, getRGBM, 0, 0, w, h, pixelsArr, 0, w);

            jclass dynTexClass = lc->GetClass("net.minecraft.client.renderer.texture.DynamicTexture");
            if (dynTexClass) {
                jmethodID dynTexCtor = lc->GetMethodID(dynTexClass, "<init>", "(II)V");
                if (dynTexCtor) {
                    jobject dynTex = env->NewObject(dynTexClass, dynTexCtor, w, h);
                    if (dynTex) {
                        jmethodID getTexData = lc->GetMethodID(dynTexClass, "getTextureData", "()[I", "func_110565_c", "e", "()[I");
                        if (getTexData) {
                            jintArray dataArr = (jintArray)env->CallObjectMethod(dynTex, getTexData);
                            if (dataArr) {
                                jint* src = env->GetIntArrayElements(pixelsArr, nullptr);
                                if (src) {
                                    env->SetIntArrayRegion(dataArr, 0, w * h, src);
                                    env->ReleaseIntArrayElements(pixelsArr, src, JNI_ABORT);
                                }
                                env->DeleteLocalRef(dataArr);
                            }
                        }

                        jmethodID updateTex = lc->GetMethodID(dynTexClass, "updateDynamicTexture", "()V", "func_110564_a", "c", "()V");
                        if (updateTex) {
                            env->CallVoidMethod(dynTex, updateTex);
                        }

                        jclass texMgrClass = env->GetObjectClass(texMgr);
                        jmethodID getDynLoc = lc->GetMethodID(texMgrClass, "getDynamicTextureLocation",
                            "(Ljava/lang/String;Lnet/minecraft/client/renderer/texture/DynamicTexture;)Lnet/minecraft/util/ResourceLocation;",
                            "func_110578_a", "a", "(Ljava/lang/String;Lbml;)Ljy;");
                        env->DeleteLocalRef(texMgrClass);

                        if (getDynLoc) {
                            jstring jName = env->NewStringUTF("ovson_real_mojang_cape");
                            resultLoc = env->CallObjectMethod(texMgr, getDynLoc, jName, dynTex);
                            env->DeleteLocalRef(jName);
                        }
                        env->DeleteLocalRef(dynTex);
                    }
                }
            }
            env->DeleteLocalRef(pixelsArr);
        }
    }
    env->DeleteLocalRef(imgCls);
    env->DeleteLocalRef(img);
    env->DeleteLocalRef(texMgr);

    return resultLoc;
}

static bool isLocalPlayerNicked(JNIEnv* env, jobject player, jobject playerInfo) {
    if (!Config::isMojangCapeOnlyNicked()) return true;
    if (OVson::g_isNicked) return true;

    std::string sessName = s_sessionUsername;
    if (sessName.empty()) {
        sessName = OVson::getRealLocalUsername();
        s_sessionUsername = sessName;
    }

    if (sessName.empty()) return false;

    if (playerInfo) {
        jclass infoClass = env->GetObjectClass(playerInfo);
        jmethodID getProf = lc->GetMethodID(infoClass, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;", "func_178845_a", "a", "()Lcom/mojang/authlib/GameProfile;");
        env->DeleteLocalRef(infoClass);
        if (getProf) {
            jobject prof = env->CallObjectMethod(playerInfo, getProf);
            if (prof) {
                jclass profClass = env->GetObjectClass(prof);
                jmethodID getName = lc->GetMethodID(profClass, "getName", "()Ljava/lang/String;");
                env->DeleteLocalRef(profClass);
                if (getName) {
                    jstring jProfName = (jstring)env->CallObjectMethod(prof, getName);
                    if (jProfName) {
                        const char* pUtf = env->GetStringUTFChars(jProfName, nullptr);
                        if (pUtf) {
                            std::string tabName = pUtf;
                            env->ReleaseStringUTFChars(jProfName, pUtf);
                            if (!tabName.empty() && tabName != sessName) {
                                OVson::g_isNicked = true;
                                OVson::g_activeNick = tabName;
                                env->DeleteLocalRef(jProfName);
                                env->DeleteLocalRef(prof);
                                return true;
                            }
                        }
                        env->DeleteLocalRef(jProfName);
                    }
                }
                env->DeleteLocalRef(prof);
            }
        }
    }

    if (OVson::g_inHypixelGame && !sessName.empty() && !OVson::g_onlinePlayers.empty()) {
        bool foundSess = false;
        for (const auto& name : OVson::g_onlinePlayers) {
            if (name == sessName) {
                foundSess = true;
                break;
            }
        }
        if (!foundSess) {
            OVson::g_isNicked = true;
            return true;
        }
    }

    return OVson::g_isNicked;
}

void tick(JNIEnv* env) {
    if (!env || !lc) return;
    try {
        if (env->ExceptionCheck()) env->ExceptionClear();
    static ULONGLONG s_lastTick = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastTick < 500) return;
    s_lastTick = now;

    const bool enabled = Config::isMojangCapeEnabled();
    if (!enabled && !s_capeApplied) {
        return;
    }

    jobject player = getPlayer(env);
    if (!player) {
        s_capeApplied = false;
        return;
    }

    if (enabled && s_sessionUuid.empty()) {
        if (s_sessionUsername.empty()) {
            s_sessionUsername = OVson::getRealLocalUsername();
        }
        jobject mc = getMinecraft(env);
        if (mc) {
            jclass mcClass = env->GetObjectClass(mc);
            jobject sess = nullptr;
            jmethodID getSessM = lc->GetMethodID(mcClass, "getSession", "()Lnet/minecraft/util/Session;", "func_110432_I", "L", "()Lavm;");
            if (getSessM) sess = env->CallObjectMethod(mc, getSessM);
            if (!sess) {
                jfieldID sessField = lc->GetFieldID(mcClass, "session", "Lnet/minecraft/util/Session;", "field_71449_j", "ae", "Lavm;");
                if (sessField) sess = env->GetObjectField(mc, sessField);
            }
            env->DeleteLocalRef(mcClass);
            if (sess) {
                jclass sessClass = env->GetObjectClass(sess);
                jmethodID getPlayerID = lc->GetMethodID(sessClass, "getPlayerID", "()Ljava/lang/String;", "func_148255_b", "a");
                jmethodID getUsername = lc->GetMethodID(sessClass, "getUsername", "()Ljava/lang/String;", "func_111285_a", "c");
                env->DeleteLocalRef(sessClass);
                if (getUsername && s_sessionUsername.empty()) {
                    jstring jU = (jstring)env->CallObjectMethod(sess, getUsername);
                    if (jU) {
                        const char* utf = env->GetStringUTFChars(jU, nullptr);
                        if (utf) { s_sessionUsername = utf; env->ReleaseStringUTFChars(jU, utf); }
                        env->DeleteLocalRef(jU);
                    }
                }
                if (getPlayerID) {
                    jstring jId = (jstring)env->CallObjectMethod(sess, getPlayerID);
                    if (jId) {
                        const char* utf = env->GetStringUTFChars(jId, nullptr);
                        if (utf) { s_sessionUuid = utf; env->ReleaseStringUTFChars(jId, utf); }
                        env->DeleteLocalRef(jId);
                    }
                }
                env->DeleteLocalRef(sess);
            }
            env->DeleteLocalRef(mc);
        }
    }

    if (enabled && !s_hasRealCape && !s_sessionUuid.empty()) {
        startAsyncCapeFetch(s_sessionUuid);
    }

    std::string urlToCreate;
    {
        std::lock_guard<std::mutex> lock(s_capeMutex);
        if (!s_pendingCapeUrl.empty()) {
            urlToCreate = s_pendingCapeUrl;
            s_pendingCapeUrl.clear();
        }
    }
    if (!urlToCreate.empty()) {
        jobject resLoc = createTextureFromUrl(env, urlToCreate);
        if (resLoc) {
            if (s_realUserCapeLoc) env->DeleteGlobalRef(s_realUserCapeLoc);
            s_realUserCapeLoc = env->NewGlobalRef(resLoc);
            s_hasRealCape = true;
            env->DeleteLocalRef(resLoc);
            Logger::info("[MojangCape] Authentic Mojang cape registered and ready.");
        }
    }

    jclass playerClass = env->GetObjectClass(player);
    jmethodID getPlayerInfo = lc->GetMethodID(playerClass, "getPlayerInfo",
        "()Lnet/minecraft/client/network/NetworkPlayerInfo;", "func_175155_b", "m", "()Lbdc;");
    env->DeleteLocalRef(playerClass);
    if (!getPlayerInfo) {
        env->DeleteLocalRef(player);
        return;
    }

    jobject playerInfo = env->CallObjectMethod(player, getPlayerInfo);
    if (!playerInfo) {
        env->DeleteLocalRef(player);
        return;
    }

    jclass infoClass = env->GetObjectClass(playerInfo);
    jfieldID capeField = lc->GetFieldID(infoClass, "locationCape",
        "Lnet/minecraft/util/ResourceLocation;", "field_178864_d", "f", "Ljy;");
    if (!capeField) {
        capeField = lc->GetFieldID(infoClass, "locationCape",
            "Lnet/minecraft/util/ResourceLocation;", "field_178864_d", "e", "Ljy;");
    }
    env->DeleteLocalRef(infoClass);

    const bool nicked = isLocalPlayerNicked(env, player, playerInfo);

    if (!nicked && capeField) {
        jobject currentCape = env->GetObjectField(playerInfo, capeField);
        if (currentCape && currentCape != s_realUserCapeLoc) {
            if (s_realUserCapeLoc) env->DeleteGlobalRef(s_realUserCapeLoc);
            s_realUserCapeLoc = env->NewGlobalRef(currentCape);
            s_hasRealCape = true;
            Logger::info("[MojangCape] Captured authentic Mojang cape from active player session.");
        }
        if (currentCape) env->DeleteLocalRef(currentCape);
    }

    if (capeField && s_realUserCapeLoc) {
        if (enabled && nicked) {
            jobject currentCape = env->GetObjectField(playerInfo, capeField);
            if (currentCape != s_realUserCapeLoc) {
                if (!s_capeApplied && currentCape) {
                    if (s_originalCape) env->DeleteGlobalRef(s_originalCape);
                    s_originalCape = env->NewGlobalRef(currentCape);
                }
                env->SetObjectField(playerInfo, capeField, s_realUserCapeLoc);
                s_capeApplied = true;
            }
            if (currentCape) env->DeleteLocalRef(currentCape);
        } else if (s_capeApplied) {
            env->SetObjectField(playerInfo, capeField, s_originalCape);
            s_capeApplied = false;
        }
    }

    env->DeleteLocalRef(playerInfo);
    env->DeleteLocalRef(player);
    if (env->ExceptionCheck()) env->ExceptionClear();
  } catch (...) {}
}

} // namespace MojangCape

