#include "ColoredHitboxes.h"
#include "../Java.h"
#include "../Config/Config.h"
#include "../SDK/McAccess.h"
#include "../Logic/StatsTracker.h"
#include "../Logic/StatsTracker.internal.h"
#include "HitboxDebug.h"
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <cctype>

namespace OVson {
namespace Utils {

struct ColorCacheEntry {
    uint32_t color = 0xFFFFFF;
    ULONGLONG expireTick = 0;
};

static std::unordered_map<std::string, ColorCacheEntry> s_playerColorCache;
static std::mutex s_colorCacheMutex;

void clearHitboxColorCache() {
    std::lock_guard<std::mutex> lock(s_colorCacheMutex);
    s_playerColorCache.clear();
}

static std::string toLower(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

uint32_t getTeamColorArgb(const std::string& team) {
    if (team.empty()) return 0xFFFFFFFF;

    std::string s = toLower(team);
    if (s.find("red") != std::string::npos || s == "c" || s == "§c" || s == "&c") return 0xFFFF5555;
    if (s.find("blue") != std::string::npos || s == "9" || s == "1" || s == "§9" || s == "&9") return 0xFF5555FF;
    if (s.find("green") != std::string::npos || s == "a" || s == "2" || s == "§a" || s == "&a") return 0xFF55FF55;
    if (s.find("yellow") != std::string::npos || s == "e" || s == "§e" || s == "&e") return 0xFFFFFF55;
    if (s.find("aqua") != std::string::npos || s.find("cyan") != std::string::npos || s == "b" || s == "3" || s == "§b" || s == "&b") return 0xFF55FFFF;
    if (s.find("pink") != std::string::npos || s == "d" || s == "5" || s == "§d" || s == "&d") return 0xFFFF55FF;
    if (s.find("gray") != std::string::npos || s.find("grey") != std::string::npos || s == "7" || s == "§7" || s == "&7") return 0xFFAAAAAA;
    if (s.find("white") != std::string::npos || s == "f" || s == "§f" || s == "&f") return 0xFFFFFFFE;

    for (size_t i = 0; i + 1 < team.size(); ++i) {
        if (team[i] == '§' || team[i] == '&' ||
            ((unsigned char)team[i] == 0xC2 && (unsigned char)team[i+1] == 0xA7)) {
            char code = ' ';
            if ((unsigned char)team[i] == 0xC2 && i + 2 < team.size()) {
                code = std::tolower((unsigned char)team[i+2]);
            } else {
                code = std::tolower((unsigned char)team[i+1]);
            }
            switch (code) {
                case '1': return 0xFF0000AA;
                case '2': return 0xFF00AA00;
                case '3': return 0xFF00AAAA;
                case '4': return 0xFFAA0000;
                case '5': return 0xFFAA00AA;
                case '6': return 0xFFFFAA00;
                case '7': return 0xFFAAAAAA;
                case '8': return 0xFF555555;
                case '9': return 0xFF5555FF;
                case 'a': return 0xFF55FF55;
                case 'b': return 0xFF55FFFF;
                case 'c': return 0xFFFF5555;
                case 'd': return 0xFFFF55FF;
                case 'e': return 0xFFFFFF55;
                case 'f': return 0xFFFFFFFE;
                default: break;
            }
        }
    }

    return 0xFFFFFFFF;
}

static bool isBedwarsNPC(const std::string& name) {
    if (name.empty()) return false;
    if (name.find("§k") != std::string::npos || 
        name.find("\xC2\xA7k") != std::string::npos ||
        name.rfind("A k", 0) == 0) {
        return true;
    }
    if (name.size() == 10) {
        bool allAlnum = true;
        int digitCount = 0;
        int alphaCount = 0;
        for (char c : name) {
            if (!std::isalnum((unsigned char)c) || std::isupper((unsigned char)c)) {
                allAlnum = false;
                break;
            }
            if (std::isdigit((unsigned char)c)) digitCount++;
            if (std::isalpha((unsigned char)c)) alphaCount++;
        }
        if (allAlnum && digitCount >= 2 && alphaCount >= 2) {
            return true;
        }
    }
    return false;
}

uint32_t parseColorCodeFromText(const std::string& text) {
    if (text.empty()) return 0;

    for (size_t i = 0; i + 1 < text.size(); ++i) {
        if (text[i] == '§' || text[i] == '&' ||
            ((unsigned char)text[i] == 0xC2 && (unsigned char)text[i+1] == 0xA7)) {
            char code = ' ';
            if ((unsigned char)text[i] == 0xC2) {
                if (i + 2 < text.size()) code = std::tolower((unsigned char)text[i+2]);
            } else {
                code = std::tolower((unsigned char)text[i+1]);
            }

            if (code == '8' || code == '0' || code == '7') {
                continue;
            }

            switch (code) {
                case '1': return 0x0000AA;
                case '2': return 0x00AA00;
                case '3': return 0x00AAAA;
                case '4': return 0xAA0000;
                case '5': return 0xAA00AA;
                case '6': return 0xFFAA00;
                case '9': return 0x5555FF;
                case 'a': return 0x55FF55;
                case 'b': return 0x55FFFF;
                case 'c': return 0xFF5555;
                case 'd': return 0xFF55FF;
                case 'e': return 0xFFFF55;
                case 'f': return 0xFFFFFE;
                default: break;
            }
        }
    }
    return 0;
}

static bool isRankPrefix(const std::string& str) {
    if (str.empty()) return false;
    std::string s = toLower(str);
    if (s.find("vip") != std::string::npos ||
        s.find("mvp") != std::string::npos ||
        s.find("helper") != std::string::npos ||
        s.find("mod") != std::string::npos ||
        s.find("admin") != std::string::npos ||
        s.find("yt") != std::string::npos ||
        s.find("youtube") != std::string::npos ||
        s.find("hypixel") != std::string::npos ||
        s.find("build team") != std::string::npos ||
        s.find("mojang") != std::string::npos) {
        return true;
    }
    return false;
}

std::string getEntityPlayerName(JNIEnv* env, jobject entity, jclass entClass) {
    if (!env || !entity || !entClass || !lc) return "";

    jmethodID getName = lc->GetMethodID(entClass, "getName", "()Ljava/lang/String;", "func_70005_c_", "e_");
    if (!getName) {
        getName = lc->GetMethodID(entClass, "getCommandSenderName", "()Ljava/lang/String;", "func_70005_c_", "e_");
    }
    if (getName) {
        jstring jName = (jstring)env->CallObjectMethod(entity, getName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (jName) {
            const char* chars = env->GetStringUTFChars(jName, nullptr);
            if (chars) {
                std::string n = chars;
                env->ReleaseStringUTFChars(jName, chars);
                env->DeleteLocalRef(jName);
                if (!n.empty()) return n;
            }
            env->DeleteLocalRef(jName);
        }
    }

    jmethodID getCustomName = lc->GetMethodID(entClass, "getCustomNameTag", "()Ljava/lang/String;", "func_96094_a", "aO_");
    if (getCustomName) {
        jstring jName = (jstring)env->CallObjectMethod(entity, getCustomName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (jName) {
            const char* chars = env->GetStringUTFChars(jName, nullptr);
            if (chars) {
                std::string n = chars;
                env->ReleaseStringUTFChars(jName, chars);
                env->DeleteLocalRef(jName);
                if (!n.empty()) return n;
            }
            env->DeleteLocalRef(jName);
        }
    }

    jmethodID getGameProfile = lc->GetMethodID(entClass, "getGameProfile",
        "()Lcom/mojang/authlib/GameProfile;", "func_146103_bH", "cd", "()Lcom/mojang/authlib/GameProfile;");
    if (!getGameProfile) {
        getGameProfile = lc->GetMethodID(entClass, "getGameProfile",
            "()Lcom/mojang/authlib/GameProfile;", "func_146103_bH", "bH", "()Lcom/mojang/authlib/GameProfile;");
    }
    if (!getGameProfile) {
        getGameProfile = lc->GetMethodID(entClass, "getGameProfile",
            "()Lcom/mojang/authlib/GameProfile;", "func_146103_bH", "cf", "()Lcom/mojang/authlib/GameProfile;");
    }
    if (getGameProfile) {
        jobject profile = env->CallObjectMethod(entity, getGameProfile);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (profile) {
            jclass profileClass = env->GetObjectClass(profile);
            jmethodID gpGetName = env->GetMethodID(profileClass, "getName", "()Ljava/lang/String;");
            env->DeleteLocalRef(profileClass);
            if (gpGetName) {
                jstring gpName = (jstring)env->CallObjectMethod(profile, gpGetName);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                } else if (gpName) {
                    const char* chars = env->GetStringUTFChars(gpName, nullptr);
                    if (chars) {
                        std::string n = chars;
                        env->ReleaseStringUTFChars(gpName, chars);
                        env->DeleteLocalRef(gpName);
                        env->DeleteLocalRef(profile);
                        if (!n.empty()) return n;
                    }
                    env->DeleteLocalRef(gpName);
                }
            }
            env->DeleteLocalRef(profile);
        }
    }

    return "";
}

uint32_t resolvePlayerTeamColor(JNIEnv* env, jobject entity) {
    if (!env || !entity || !lc) return (uint32_t)-1;

    jclass entClass = env->GetObjectClass(entity);
    if (!entClass) return (uint32_t)-1;

    std::string playerName = getEntityPlayerName(env, entity, entClass);
    if (playerName.empty()) {
        env->DeleteLocalRef(entClass);
        return (uint32_t)-1;
    }

    std::string lowerName = toLower(playerName);
    ULONGLONG now = GetTickCount64();

    {
        std::lock_guard<std::mutex> lock(s_colorCacheMutex);
        auto it = s_playerColorCache.find(lowerName);
        if (it != s_playerColorCache.end() && now < it->second.expireTick) {
            env->DeleteLocalRef(entClass);
            return it->second.color;
        }
    }

    auto cacheAndReturn = [&](uint32_t col, bool isResolved, const char* source) -> uint32_t {
        ColorCacheEntry entry;
        entry.color = col;
        entry.expireTick = now + (isResolved ? 3000 : 500);
        {
            std::lock_guard<std::mutex> lock(s_colorCacheMutex);
            s_playerColorCache[lowerName] = entry;
        }
        HitboxDebug::logColorResolve(playerName, col, source);
        env->DeleteLocalRef(entClass);
        return col;
    };


    {
        jclass epCls = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
        if (epCls && env->IsInstanceOf(entity, epCls)) {
            jfieldID f_inv = lc->GetFieldID(epCls, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;", "field_71071_by", "bi", "Lwm;");
            jclass ipCls = lc->GetClass("net.minecraft.entity.player.InventoryPlayer");
            jfieldID f_armor = lc->GetFieldID(ipCls, "armorInventory", "[Lnet/minecraft/item/ItemStack;", "field_70460_b", "b", "[Lzx;");
            jclass isCls = lc->GetClass("net.minecraft.item.ItemStack");
            jmethodID m_getItem = lc->GetMethodID(isCls, "getItem", "()Lnet/minecraft/item/Item;", "func_77973_b", "b", "()Lzw;");
            jclass iaCls = lc->GetClass("net.minecraft.item.ItemArmor");
            jmethodID m_getColor = lc->GetMethodID(iaCls, "getColor", "(Lnet/minecraft/item/ItemStack;)I", "func_82814_b", "b", "(Lzx;)I");

            if (f_inv && ipCls && f_armor && isCls && m_getItem && iaCls && m_getColor) {
                jobject inv = env->GetObjectField(entity, f_inv);
                if (inv) {
                    jobjectArray armor = (jobjectArray)env->GetObjectField(inv, f_armor);
                    if (armor) {
                        if (env->GetArrayLength(armor) >= 4) {
                            jobject helmet = env->GetObjectArrayElement(armor, 3);
                            if (helmet) {
                                jobject item = env->CallObjectMethod(helmet, m_getItem);
                                env->ExceptionClear();
                                if (item && env->IsInstanceOf(item, iaCls)) {
                                    int color = env->CallIntMethod(item, m_getColor, helmet);
                                    env->ExceptionClear();
                                    std::string hTeam = OVson::closestTeamColor(color);
                                    if (!hTeam.empty() && OVson::isRealBedwarsTeam(hTeam)) {
                                        OVson::setTeamColorSticky(playerName, hTeam, true);
                                        uint32_t c = getTeamColorArgb(hTeam);
                                        if (c != 0xFFFFFFFF && c != 0) {
                                            if (item) env->DeleteLocalRef(item);
                                            env->DeleteLocalRef(helmet);
                                            env->DeleteLocalRef(armor);
                                            env->DeleteLocalRef(inv);
                                            HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via Helmet: '%s'", playerName.c_str(), hTeam.c_str());
                                            return cacheAndReturn(c & 0xFFFFFF, true, "Leather Helmet");
                                        }
                                    }
                                }
                                if (item) env->DeleteLocalRef(item);
                                env->DeleteLocalRef(helmet);
                            }
                        }
                        env->DeleteLocalRef(armor);
                    }
                    env->DeleteLocalRef(inv);
                }
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
        auto it = OVson::g_playerTeamColor.find(playerName);
        if (it != OVson::g_playerTeamColor.end() && !it->second.empty()) {
            uint32_t c = getTeamColorArgb(it->second);
            if (c != 0xFFFFFFFF && c != 0) {
                HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via Tab/Team Map (exact: '%s')", playerName.c_str(), it->second.c_str());
                return cacheAndReturn(c & 0xFFFFFF, true, "Tab/Team Map");
            }
        }
        for (const auto& kv : OVson::g_playerTeamColor) {
            if (iequals(kv.first, playerName) && !kv.second.empty()) {
                uint32_t c = getTeamColorArgb(kv.second);
                if (c != 0xFFFFFFFF && c != 0) {
                    HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via Tab/Team Map (iequals '%s': '%s')", playerName.c_str(), kv.first.c_str(), kv.second.c_str());
                    return cacheAndReturn(c & 0xFFFFFF, true, "Tab/Team Map");
                }
            }
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
        auto it = OVson::g_playerStatsMap.find(playerName);
        if (it != OVson::g_playerStatsMap.end() && !it->second.teamColor.empty()) {
            uint32_t c = getTeamColorArgb(it->second.teamColor);
            if (c != 0xFFFFFFFF && c != 0) {
                HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via Player Stats Map: '%s'", playerName.c_str(), it->second.teamColor.c_str());
                return cacheAndReturn(c & 0xFFFFFF, true, "Player Stats Map");
            }
        }
        for (const auto& kv : OVson::g_playerStatsMap) {
            if (iequals(kv.first, playerName) && !kv.second.teamColor.empty()) {
                uint32_t c = getTeamColorArgb(kv.second.teamColor);
                if (c != 0xFFFFFFFF && c != 0) {
                    HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via Player Stats Map (iequals '%s': '%s')", playerName.c_str(), kv.first.c_str(), kv.second.teamColor.c_str());
                    return cacheAndReturn(c & 0xFFFFFF, true, "Player Stats Map");
                }
            }
        }
    }

    std::string team = OVson::resolveTeamForName(playerName);
    if (!team.empty()) {
        uint32_t c = getTeamColorArgb(team);
        if (c != 0xFFFFFFFF && c != 0) {
            HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' resolved via resolveTeamForName: '%s'", playerName.c_str(), team.c_str());
            return cacheAndReturn(c & 0xFFFFFF, true, "Scoreboard Name");
        }
    }

    jmethodID getTeam = lc->GetMethodID(entClass, "getTeam",
        "()Lnet/minecraft/scoreboard/Team;", "func_96124_cp", "bO", "()Lauq;");
    if (!getTeam) {
        getTeam = lc->GetMethodID(entClass, "getTeam",
            "()Lnet/minecraft/scoreboard/Team;", "func_96124_cp", "bP", "()Lauq;");
    }
    if (getTeam) {
        jobject teamObj = env->CallObjectMethod(entity, getTeam);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (teamObj) {
            jclass teamClass = env->GetObjectClass(teamObj);
            jmethodID getColorPrefix = lc->GetMethodID(teamClass, "getColorPrefix",
                "()Ljava/lang/String;", "func_96668_e", "e", "()Ljava/lang/String;");
            if (!getColorPrefix) getColorPrefix = lc->GetMethodID(teamClass, "getColorPrefix", "()Ljava/lang/String;", "func_96668_e", "c");
            if (!getColorPrefix) getColorPrefix = lc->GetMethodID(teamClass, "getColorPrefix", "()Ljava/lang/String;", "func_96668_e", "d");
            if (getColorPrefix) {
                jstring jPrefix = (jstring)env->CallObjectMethod(teamObj, getColorPrefix);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                } else if (jPrefix) {
                    const char* pChars = env->GetStringUTFChars(jPrefix, nullptr);
                    if (pChars) {
                        std::string pStr = pChars;
                        env->ReleaseStringUTFChars(jPrefix, pChars);
                        HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' entity.getTeam().getColorPrefix() = '%s'", playerName.c_str(), pStr.c_str());
                        if (!isRankPrefix(pStr)) {
                            uint32_t col = parseColorCodeFromText(pStr);
                            if (col != 0 && col != 0xFFFFFF) {
                                env->DeleteLocalRef(jPrefix);
                                env->DeleteLocalRef(teamClass);
                                env->DeleteLocalRef(teamObj);
                                return cacheAndReturn(col, true, "Scoreboard Prefix");
                            }
                        }
                    }
                    env->DeleteLocalRef(jPrefix);
                }
            }

            jmethodID getTeamName = lc->GetMethodID(teamClass, "getRegisteredName",
                "()Ljava/lang/String;", "func_96661_b", "b", "()Ljava/lang/String;");
            if (getTeamName) {
                jstring jTName = (jstring)env->CallObjectMethod(teamObj, getTeamName);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                } else if (jTName) {
                    const char* tnChars = env->GetStringUTFChars(jTName, nullptr);
                    if (tnChars) {
                        std::string tNameStr = tnChars;
                        env->ReleaseStringUTFChars(jTName, tnChars);
                        HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' entity.getTeam().getRegisteredName() = '%s'", playerName.c_str(), tNameStr.c_str());
                        uint32_t c = getTeamColorArgb(tNameStr);
                        if (c != 0xFFFFFFFF && c != 0) {
                            env->DeleteLocalRef(jTName);
                            env->DeleteLocalRef(teamClass);
                            env->DeleteLocalRef(teamObj);
                            return cacheAndReturn(c & 0xFFFFFF, true, "Scoreboard Registered Name");
                        }
                    }
                    env->DeleteLocalRef(jTName);
                }
            }

            env->DeleteLocalRef(teamClass);
            env->DeleteLocalRef(teamObj);
        }
    }

    jmethodID getDisplayName = lc->GetMethodID(entClass, "getDisplayName",
        "()Lnet/minecraft/util/IChatComponent;", "func_145748_c_", "f_", "()Leu;");
    if (getDisplayName) {
        jobject chatComp = env->CallObjectMethod(entity, getDisplayName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        } else if (chatComp) {
            jclass ccClass = env->GetObjectClass(chatComp);
            jmethodID getFormatted = lc->GetMethodID(ccClass, "getFormattedText",
                "()Ljava/lang/String;", "func_150254_d", "d", "()Ljava/lang/String;");
            env->DeleteLocalRef(ccClass);
            if (getFormatted) {
                jstring jFormatted = (jstring)env->CallObjectMethod(chatComp, getFormatted);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                } else if (jFormatted) {
                    const char* fChars = env->GetStringUTFChars(jFormatted, nullptr);
                    if (fChars) {
                        std::string fStr = fChars;
                        env->ReleaseStringUTFChars(jFormatted, fChars);
                        HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' entity.getDisplayName() = '%s'", playerName.c_str(), fStr.c_str());
                        if (!isRankPrefix(fStr)) {
                            uint32_t col = parseColorCodeFromText(fStr);
                            if (col != 0 && col != 0xFFFFFF) {
                                env->DeleteLocalRef(jFormatted);
                                env->DeleteLocalRef(chatComp);
                                return cacheAndReturn(col, true, "Display Name");
                            }
                        }
                    }
                    env->DeleteLocalRef(jFormatted);
                }
            }
            env->DeleteLocalRef(chatComp);
        }
    }

    if (isBedwarsNPC(playerName)) {
        return cacheAndReturn((uint32_t)-1, true, "Bedwars NPC Filtered");
    }

    HitboxDebug::log("[RESOLVE-DEBUG] Player '%s' could NOT be resolved to any team -> Default (Unresolved)", playerName.c_str());
    return cacheAndReturn((uint32_t)-1, false, "Default (Unresolved)");
}

static jfieldID s_fRm = nullptr;
static jfieldID s_fvX = nullptr, s_fvY = nullptr, s_fvZ = nullptr;
static jfieldID s_fPlayers = nullptr;
static jmethodID s_mListSize = nullptr;
static jmethodID s_mListGet = nullptr;
static jfieldID s_fPosX = nullptr, s_fPosY = nullptr, s_fPosZ = nullptr;

static jfieldID s_fMinX = nullptr, s_fMinY = nullptr, s_fMinZ = nullptr;
static jfieldID s_fMaxX = nullptr, s_fMaxY = nullptr, s_fMaxZ = nullptr;
static bool s_bbInit = false;

void resetBoxColorState(JNIEnv* env) {
    s_fRm = nullptr;
    s_fvX = s_fvY = s_fvZ = nullptr;
    s_fPlayers = nullptr;
    s_mListSize = s_mListGet = nullptr;
    s_fPosX = s_fPosY = s_fPosZ = nullptr;
    s_fMinX = s_fMinY = s_fMinZ = nullptr;
    s_fMaxX = s_fMaxY = s_fMaxZ = nullptr;
    s_bbInit = false;
}

uint32_t resolveBoxColor(JNIEnv* env, jobject bb, uint32_t originalColor) {
    if (!env || !bb || !lc) return originalColor;
    if (!Config::isTeamColoredHitboxesEnabled()) return originalColor;

    jclass bbClass = env->GetObjectClass(bb);
    if (!bbClass) return originalColor;

    if (!s_bbInit) {
        s_fMinX = lc->GetFieldID(bbClass, "minX", "D", "field_72340_a", "a");
        s_fMinY = lc->GetFieldID(bbClass, "minY", "D", "field_72338_b", "b");
        s_fMinZ = lc->GetFieldID(bbClass, "minZ", "D", "field_72339_c", "c");
        s_fMaxX = lc->GetFieldID(bbClass, "maxX", "D", "field_72336_d", "d");
        s_fMaxY = lc->GetFieldID(bbClass, "maxY", "D", "field_72337_e", "e");
        s_fMaxZ = lc->GetFieldID(bbClass, "maxZ", "D", "field_72334_f", "f");
        s_bbInit = (s_fMinX && s_fMinY && s_fMinZ && s_fMaxX && s_fMaxY && s_fMaxZ);
    }

    if (!s_bbInit) {
        env->DeleteLocalRef(bbClass);
        return originalColor;
    }

    double minX = env->GetDoubleField(bb, s_fMinX);
    double minY = env->GetDoubleField(bb, s_fMinY);
    double minZ = env->GetDoubleField(bb, s_fMinZ);
    double maxX = env->GetDoubleField(bb, s_fMaxX);
    double maxY = env->GetDoubleField(bb, s_fMaxY);
    double maxZ = env->GetDoubleField(bb, s_fMaxZ);
    env->DeleteLocalRef(bbClass);

    double w = maxX - minX;
    double h = maxY - minY;
    double d = maxZ - minZ;

    bool isHumanoid = (h >= 1.3 && h <= 2.05 && w >= 0.35 && w <= 0.85 && d >= 0.35 && d <= 0.85);

    static ULONGLONG s_lastDimDbg = 0;
    ULONGLONG nowDim = GetTickCount64();
    if (nowDim - s_lastDimDbg > 3000) {
        s_lastDimDbg = nowDim;
        HitboxDebug::log("[BOX-DIMS] Sample box: w=%.2f, h=%.2f, d=%.2f -> isHumanoid: %s",
                         w, h, d, isHumanoid ? "YES" : "NO");
    }

    if (!isHumanoid) {
        return originalColor;
    }

    jobject mcObj = Mc::theMinecraft(env);
    if (!mcObj) return originalColor;

    jobject worldObj = Mc::theWorld(env);
    if (!worldObj) {
        env->DeleteLocalRef(mcObj);
        return originalColor;
    }

    if (!s_fRm) {
        jclass mcCls = env->GetObjectClass(mcObj);
        s_fRm = lc->GetFieldID(mcCls, "renderManager", "Lnet/minecraft/client/renderer/entity/RenderManager;", "field_175616_W", "aa", "Lbiu;");
        if (!s_fRm) s_fRm = lc->FindFieldBySignature(mcCls, "Lbiu;");
        if (!s_fRm) s_fRm = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/renderer/entity/RenderManager;");
        env->DeleteLocalRef(mcCls);
    }

    if (!s_fRm) {
        env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(mcObj);
        return originalColor;
    }

    jobject rmObj = env->GetObjectField(mcObj, s_fRm);
    env->DeleteLocalRef(mcObj);
    if (!rmObj) {
        env->DeleteLocalRef(worldObj);
        return originalColor;
    }

    if (!s_fvX || !s_fvY || !s_fvZ) {
        jclass rmCls = env->GetObjectClass(rmObj);
        s_fvX = lc->GetFieldID(rmCls, "renderPosX", "D", "field_78725_b", "o");
        s_fvY = lc->GetFieldID(rmCls, "renderPosY", "D", "field_78726_c", "p");
        s_fvZ = lc->GetFieldID(rmCls, "renderPosZ", "D", "field_78723_d", "q");
        if (!s_fvZ) s_fvZ = lc->GetFieldID(rmCls, "viewerPosZ", "D", "field_78728_n", "q");
        env->DeleteLocalRef(rmCls);
    }

    if (!s_fvX || !s_fvY || !s_fvZ) {
        env->DeleteLocalRef(rmObj);
        env->DeleteLocalRef(worldObj);
        return originalColor;
    }

    double camX = env->GetDoubleField(rmObj, s_fvX);
    double camY = env->GetDoubleField(rmObj, s_fvY);
    double camZ = env->GetDoubleField(rmObj, s_fvZ);
    env->DeleteLocalRef(rmObj);

    if (!s_fPlayers) {
        jclass worldCls = env->GetObjectClass(worldObj);
        const char* playerFieldCandidates[] = {"playerEntities", "field_73010_i", "j", "k", "i", "l", "m", "h", "g", nullptr};
        for (int ci = 0; playerFieldCandidates[ci] && !s_fPlayers; ++ci) {
            s_fPlayers = lc->GetFieldID(worldCls, playerFieldCandidates[ci], "Ljava/util/List;");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (!s_fPlayers) s_fPlayers = lc->FindFieldBySignature(worldCls, "Ljava/util/List;");
        env->DeleteLocalRef(worldCls);
    }

    if (!s_fPlayers) {
        env->DeleteLocalRef(worldObj);
        return originalColor;
    }

    jobject playerList = env->GetObjectField(worldObj, s_fPlayers);
    env->DeleteLocalRef(worldObj);
    if (!playerList) return originalColor;

    if (!s_mListSize || !s_mListGet) {
        jclass listCls = env->FindClass("java/util/List");
        if (listCls) {
            s_mListSize = env->GetMethodID(listCls, "size", "()I");
            s_mListGet = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
            env->DeleteLocalRef(listCls);
        }
    }

    if (!s_mListSize || !s_mListGet) {
        env->DeleteLocalRef(playerList);
        return originalColor;
    }

    jint count = env->CallIntMethod(playerList, s_mListSize);
    if (count <= 0) {
        env->DeleteLocalRef(playerList);
        return originalColor;
    }

    double boxWorldX = (minX + maxX) * 0.5 + camX;
    double boxWorldY = minY + camY;
    double boxWorldZ = (minZ + maxZ) * 0.5 + camZ;

    double bestDistSq = 49.0;
    jobject bestPlayer = nullptr;
    double nearestAnyDistSq = 999999.0;
    std::string nearestAnyName;

    for (jint i = 0; i < count; ++i) {
        jobject p = env->CallObjectMethod(playerList, s_mListGet, i);
        if (!p) continue;

        if (!s_fPosX || !s_fPosY || !s_fPosZ) {
            jclass entCls = env->GetObjectClass(p);
            s_fPosX = lc->GetFieldID(entCls, "posX", "D", "field_70165_t", "s");
            s_fPosY = lc->GetFieldID(entCls, "posY", "D", "field_70163_u", "t");
            s_fPosZ = lc->GetFieldID(entCls, "posZ", "D", "field_70161_v", "u");
            env->DeleteLocalRef(entCls);
        }

        if (!s_fPosX || !s_fPosY || !s_fPosZ) {
            env->DeleteLocalRef(p);
            continue;
        }

        double px = env->GetDoubleField(p, s_fPosX);
        double py = env->GetDoubleField(p, s_fPosY);
        double pz = env->GetDoubleField(p, s_fPosZ);

        double dx = px - boxWorldX;
        double dy = py - boxWorldY;
        double dz = pz - boxWorldZ;
        double distSq = dx * dx + dy * dy + dz * dz;

        if (distSq < nearestAnyDistSq) {
            nearestAnyDistSq = distSq;
            jclass entClass = env->GetObjectClass(p);
            nearestAnyName = getEntityPlayerName(env, p, entClass);
            env->DeleteLocalRef(entClass);
        }

        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            if (bestPlayer) env->DeleteLocalRef(bestPlayer);
            bestPlayer = p;
        } else {
            env->DeleteLocalRef(p);
        }
    }

    env->DeleteLocalRef(playerList);

    uint32_t resultColor = (uint32_t)-1;
    std::string matchedName;
    if (bestPlayer) {
        jclass entClass = env->GetObjectClass(bestPlayer);
        matchedName = getEntityPlayerName(env, bestPlayer, entClass);
        env->DeleteLocalRef(entClass);

        uint32_t col = resolvePlayerTeamColor(env, bestPlayer);
        env->DeleteLocalRef(bestPlayer);
        if (col != 0xFFFFFF && col != (uint32_t)-1) {
            resultColor = col;
        }
    }

    if (resultColor == (uint32_t)-1 && !nearestAnyName.empty()) {
        std::string lowerNearest = toLower(nearestAnyName);
        std::lock_guard<std::mutex> lock(s_colorCacheMutex);
        auto it = s_playerColorCache.find(lowerNearest);
        if (it != s_playerColorCache.end() && it->second.color != (uint32_t)-1 && it->second.color != 0xFFFFFF) {
            resultColor = it->second.color;
            matchedName = nearestAnyName + "(cache_held)";
        }
    }

    ULONGLONG nowMs = GetTickCount64();

    static ULONGLONG s_lastMatchDbg = 0;
    if (nowMs - s_lastMatchDbg > 1500) {
        s_lastMatchDbg = nowMs;
        if (!matchedName.empty()) {
            HitboxDebug::logBoxMatch(w, h, d, boxWorldX, boxWorldY, boxWorldZ, count, 
                                     matchedName.c_str(), sqrt(bestDistSq), 
                                     resultColor, "resolveBoxColor");
        } else if (!nearestAnyName.empty()) {
            HitboxDebug::logBoxMatch(w, h, d, boxWorldX, boxWorldY, boxWorldZ, count, 
                                     (nearestAnyName + "(too_far)").c_str(), sqrt(nearestAnyDistSq), 
                                     (uint32_t)-1, "exceededThreshold");
        } else {
            HitboxDebug::logBoxMatch(w, h, d, boxWorldX, boxWorldY, boxWorldZ, count, 
                                     "", -1.0, (uint32_t)-1, "noPlayersInList");
        }
    }

    return (resultColor != (uint32_t)-1) ? resultColor : (uint32_t)-1;
}

} // namespace Utils
} // namespace OVson
