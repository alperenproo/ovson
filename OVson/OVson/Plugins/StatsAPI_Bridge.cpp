#include "StatsAPI_Bridge.h"
#include "../Logic/StatsTracker.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"
#include "../Services/Hypixel.h"
#include "../Utils/Anticheat/Anticheat.h"
#include <string>

namespace StatsAPIBridge {

    static std::string jstringToString(JNIEnv* env, jstring jstr) {
        if (!jstr) return "";
        const char* chars = env->GetStringUTFChars(jstr, nullptr);
        std::string result(chars);
        env->ReleaseStringUTFChars(jstr, chars);
        return result;
    }

    static Hypixel::PlayerStats* getStatsPtr(const std::string& name) {
        std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
        auto it = OVson::g_playerStatsMap.find(name);
        if (it != OVson::g_playerStatsMap.end()) {
            return &it->second;
        }
        return nullptr;
    }

    jdouble JNICALL getFkdr(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        if (stats) {
            if (stats->bedwarsFinalDeaths == 0) return stats->bedwarsFinalKills;
            return (double)stats->bedwarsFinalKills / (double)stats->bedwarsFinalDeaths;
        }
        return 0.0;
    }

    jdouble JNICALL getWlr(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        if (stats) {
            if (stats->bedwarsLosses == 0) return stats->bedwarsWins;
            return (double)stats->bedwarsWins / (double)stats->bedwarsLosses;
        }
        return 0.0;
    }

    jint JNICALL getFinalKills(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        return stats ? stats->bedwarsFinalKills : 0;
    }

    jint JNICALL getWinstreak(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        return stats ? stats->winstreak : 0;
    }

    jint JNICALL getNetworkLevel(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        return stats ? stats->networkLevel : 0;
    }

    jint JNICALL getBedwarsStar(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        return stats ? stats->bedwarsStar : 0;
    }

    jstring JNICALL getTeamColor(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        if (stats) {
            return env->NewStringUTF(stats->teamColor.c_str());
        }
        return env->NewStringUTF("");
    }

    jboolean JNICALL isNicked(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        return stats ? stats->isNicked : JNI_FALSE;
    }

    jstring JNICALL getRank(JNIEnv* env, jclass clazz, jstring jname) {
        auto stats = getStatsPtr(jstringToString(env, jname));
        if (stats) {
            return env->NewStringUTF(stats->rank.c_str());
        }
        return env->NewStringUTF("");
    }

    jboolean JNICALL isInHypixelGame(JNIEnv* env, jclass clazz) {
        return OVson::isInHypixelGame() ? JNI_TRUE : JNI_FALSE;
    }

    jint JNICALL getGameMode(JNIEnv* env, jclass clazz) {
        return OVson::getGameMode();
    }

    jstring JNICALL getMapName(JNIEnv* env, jclass clazz) {
        std::string map = OVson::Bedwars::Runtime::instance().snapshot().mapName;
        return env->NewStringUTF(map.c_str());
    }

    static const JNINativeMethod methods[] = {
        {(char*)"getFkdr", (char*)"(Ljava/lang/String;)D", (void*)getFkdr},
        {(char*)"getWlr", (char*)"(Ljava/lang/String;)D", (void*)getWlr},
        {(char*)"getFinalKills", (char*)"(Ljava/lang/String;)I", (void*)getFinalKills},
        {(char*)"getWinstreak", (char*)"(Ljava/lang/String;)I", (void*)getWinstreak},
        {(char*)"getNetworkLevel", (char*)"(Ljava/lang/String;)I", (void*)getNetworkLevel},
        {(char*)"getBedwarsStar", (char*)"(Ljava/lang/String;)I", (void*)getBedwarsStar},
        {(char*)"getTeamColor", (char*)"(Ljava/lang/String;)Ljava/lang/String;", (void*)getTeamColor},
        {(char*)"isNicked", (char*)"(Ljava/lang/String;)Z", (void*)isNicked},
        {(char*)"getRank", (char*)"(Ljava/lang/String;)Ljava/lang/String;", (void*)getRank},
        {(char*)"isInHypixelGame", (char*)"()Z", (void*)isInHypixelGame},
        {(char*)"getGameMode", (char*)"()I", (void*)getGameMode},
        {(char*)"getMapName", (char*)"()Ljava/lang/String;", (void*)getMapName}
    };

    void registerNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, methods, sizeof(methods)/sizeof(methods[0]));
    }
    
    // --- AnticheatAPI ---
    jboolean JNICALL isPlayerFlagged(JNIEnv* env, jclass clazz, jstring jname) {
        return Anticheat::isPlayerFlagged(jstringToString(env, jname)) ? JNI_TRUE : JNI_FALSE;
    }
    
    jboolean JNICALL isPlayerSneaking(JNIEnv* env, jclass clazz, jstring jname) {
        return Anticheat::isPlayerSneaking(jstringToString(env, jname)) ? JNI_TRUE : JNI_FALSE;
    }

    static const JNINativeMethod acMethods[] = {
        {(char*)"isPlayerFlagged", (char*)"(Ljava/lang/String;)Z", (void*)isPlayerFlagged},
        {(char*)"isPlayerSneaking", (char*)"(Ljava/lang/String;)Z", (void*)isPlayerSneaking}
    };

    void registerAnticheatNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, acMethods, sizeof(acMethods)/sizeof(acMethods[0]));
    }
}
