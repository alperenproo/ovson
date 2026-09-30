#include "WorldAPI_Bridge.h"
#include "../Java.h"
#include "../Logic/StatsTracker.internal.h"
#include <map>
#include <vector>

namespace WorldAPIBridge {

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

    jboolean JNICALL exists(JNIEnv* env, jobject obj) {
        jobject world = getWorld(env);
        if (world) {
            env->DeleteLocalRef(world);
            return JNI_TRUE;
        }
        return JNI_FALSE;
    }

    jint JNICALL getDimension(JNIEnv* env, jobject obj) {
        return 0; // STUB
    }

    jlong JNICALL getWorldTime(JNIEnv* env, jobject obj) {
        jobject world = getWorld(env);
        if (!world) return 0;
        jclass worldCls = env->GetObjectClass(world);
        jmethodID m = lc->GetMethodID(worldCls, "getWorldTime", "()J", "func_72820_D", "K");
        jlong val = m ? env->CallLongMethod(world, m) : 0;
        env->DeleteLocalRef(worldCls);
        env->DeleteLocalRef(world);
        return val;
    }

    jlong JNICALL getTotalWorldTime(JNIEnv* env, jobject obj) {
        jobject world = getWorld(env);
        if (!world) return 0;
        jclass worldCls = env->GetObjectClass(world);
        jmethodID m = lc->GetMethodID(worldCls, "getTotalWorldTime", "()J", "func_82737_E", "J");
        jlong val = m ? env->CallLongMethod(world, m) : 0;
        env->DeleteLocalRef(worldCls);
        env->DeleteLocalRef(world);
        return val;
    }

    jboolean JNICALL isRaining(JNIEnv* env, jobject obj) {
        jobject world = getWorld(env);
        if (!world) return JNI_FALSE;
        jclass worldCls = env->GetObjectClass(world);
        jmethodID m = lc->GetMethodID(worldCls, "isRaining", "()Z", "func_72911_I", "P");
        jboolean val = m ? env->CallBooleanMethod(world, m) : JNI_FALSE;
        env->DeleteLocalRef(worldCls);
        env->DeleteLocalRef(world);
        return val;
    }

    jboolean JNICALL isThundering(JNIEnv* env, jobject obj) {
        jobject world = getWorld(env);
        if (!world) return JNI_FALSE;
        jclass worldCls = env->GetObjectClass(world);
        jmethodID m = lc->GetMethodID(worldCls, "isThundering", "()Z", "func_72896_J", "Q");
        jboolean val = m ? env->CallBooleanMethod(world, m) : JNI_FALSE;
        env->DeleteLocalRef(worldCls);
        env->DeleteLocalRef(world);
        return val;
    }

    jfloat JNICALL getSunAngle(JNIEnv* env, jobject obj) {
        return 0.0f; // STUB
    }

    jint JNICALL getDifficulty(JNIEnv* env, jobject obj) {
        return 0; // STUB
    }

    jobject JNICALL getEntities(JNIEnv* env, jobject obj) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jobject JNICALL getPlayerEntities(JNIEnv* env, jobject obj) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jobject JNICALL getEntityById(JNIEnv* env, jobject obj, jint id) {
        return nullptr; // STUB
    }

    jboolean JNICALL isValidEntity(JNIEnv* env, jobject obj, jobject entity) {
        return JNI_FALSE; // STUB
    }

    jobject JNICALL getEntitiesInRadius(JNIEnv* env, jobject obj, jobject vec3, jdouble radius) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jobject JNICALL getEntitiesByType(JNIEnv* env, jobject obj, jstring type) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jobject JNICALL getClosestEntity(JNIEnv* env, jobject obj, jdouble distance) {
        return nullptr; // STUB
    }

    jobject JNICALL getClosestPlayer(JNIEnv* env, jobject obj, jdouble distance) {
        return nullptr; // STUB
    }

    jint JNICALL getBlockIdAt(JNIEnv* env, jobject obj, jint x, jint y, jint z) {
        return 0; // STUB
    }

    jint JNICALL getLightLevelAt(JNIEnv* env, jobject obj, jint x, jint y, jint z) {
        return 15; // STUB
    }

    jboolean JNICALL isBlockSolid(JNIEnv* env, jobject obj, jint x, jint y, jint z) {
        return JNI_TRUE; // STUB
    }

    jboolean JNICALL canSeeBlock(JNIEnv* env, jobject obj, jobject vec1, jobject vec2) {
        return JNI_TRUE; // STUB
    }

    jobject JNICALL getTileEntities(JNIEnv* env, jobject obj) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jobject JNICALL getNetworkPlayers(JNIEnv* env, jobject obj) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);
        return list;
    }

    jstring JNICALL getTabHeader(JNIEnv* env, jobject obj) {
        return env->NewStringUTF(""); // STUB
    }

    jstring JNICALL getTabFooter(JNIEnv* env, jobject obj) {
        return env->NewStringUTF(""); // STUB
    }

    jobject JNICALL getTeams(JNIEnv* env, jobject obj) {
        jclass mapClass = env->FindClass("java/util/HashMap");
        if (!mapClass) return nullptr;
        jmethodID mapInit = env->GetMethodID(mapClass, "<init>", "()V");
        jmethodID mapPut = env->GetMethodID(mapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        jobject resultMap = env->NewObject(mapClass, mapInit);
        env->DeleteLocalRef(mapClass);

        jclass listClass = env->FindClass("java/util/ArrayList");
        if (!listClass) return resultMap;
        jmethodID listInit = env->GetMethodID(listClass, "<init>", "()V");
        jmethodID listAdd = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");

        std::map<std::string, std::vector<std::string>> teamGroups;
        {
            std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
            for (const auto& kv : OVson::g_playerTeamColor) {
                if (!kv.second.empty() && !kv.first.empty()) {
                    teamGroups[kv.second].push_back(kv.first);
                }
            }
        }

        for (const auto& tg : teamGroups) {
            jstring jTeamName = env->NewStringUTF(tg.first.c_str());
            jobject jPlayerList = env->NewObject(listClass, listInit);
            for (const auto& p : tg.second) {
                jstring jp = env->NewStringUTF(p.c_str());
                env->CallBooleanMethod(jPlayerList, listAdd, jp);
                env->DeleteLocalRef(jp);
            }
            env->CallObjectMethod(resultMap, mapPut, jTeamName, jPlayerList);
            env->DeleteLocalRef(jTeamName);
            env->DeleteLocalRef(jPlayerList);
        }
        env->DeleteLocalRef(listClass);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return resultMap;
    }

    jobject JNICALL getScoreboardLines(JNIEnv* env, jobject obj) {
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        if (!arrayListClass) return nullptr;
        jmethodID init = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID addM = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
        jobject list = env->NewObject(arrayListClass, init);
        env->DeleteLocalRef(arrayListClass);

        jobject world = getWorld(env);
        if (!world) return list;

        jclass worldCls = env->GetObjectClass(world);
        jmethodID m_getSb = lc->GetMethodID(worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;", "func_96441_U", "Z", "()Lauo;");
        if (!m_getSb) m_getSb = lc->FindMethodBySignature(worldCls, "()Lnet/minecraft/scoreboard/Scoreboard;");
        if (!m_getSb) m_getSb = lc->FindMethodBySignature(worldCls, "()Lauo;");
        if (!m_getSb) {
            env->DeleteLocalRef(worldCls);
            env->DeleteLocalRef(world);
            return list;
        }

        jobject sb = env->CallObjectMethod(world, m_getSb);
        env->DeleteLocalRef(worldCls);
        env->DeleteLocalRef(world);
        if (!sb) return list;

        jclass sbCls = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
        if (!sbCls) {
            env->DeleteLocalRef(sb);
            return list;
        }

        jmethodID getObjSlot = lc->GetMethodID(sbCls, "getObjectiveInDisplaySlot", "(I)Lnet/minecraft/scoreboard/ScoreObjective;", "func_96539_a", "a", "(I)Lauk;");
        if (!getObjSlot) getObjSlot = lc->FindMethodBySignature(sbCls, "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
        if (!getObjSlot) getObjSlot = lc->FindMethodBySignature(sbCls, "(I)Lauk;");

        jobject objScore = getObjSlot ? env->CallObjectMethod(sb, getObjSlot, 1) : nullptr;
        if (!objScore) {
            env->DeleteLocalRef(sb);
            return list;
        }

        jmethodID getScores = lc->GetMethodID(sbCls, "getSortedScores", "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;", "func_96534_i", "i", "(Lauk;)Ljava/util/Collection;");
        if (!getScores) getScores = lc->FindMethodBySignature(sbCls, "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;");
        if (!getScores) getScores = lc->FindMethodBySignature(sbCls, "(Lauk;)Ljava/util/Collection;");

        jobject collection = getScores ? env->CallObjectMethod(sb, getScores, objScore) : nullptr;
        env->DeleteLocalRef(objScore);
        if (!collection) {
            env->DeleteLocalRef(sb);
            return list;
        }

        jclass collCls = env->GetObjectClass(collection);
        jmethodID toArrayM = env->GetMethodID(collCls, "toArray", "()[Ljava/lang/Object;");
        jobjectArray arr = toArrayM ? (jobjectArray)env->CallObjectMethod(collection, toArrayM) : nullptr;
        env->DeleteLocalRef(collCls);
        env->DeleteLocalRef(collection);

        if (arr) {
            int len = env->GetArrayLength(arr);
            jclass scoreCls = lc->GetClass("net.minecraft.scoreboard.Score");
            jmethodID getPlayerNameM = scoreCls ? lc->GetMethodID(scoreCls, "getPlayerName", "()Ljava/lang/String;", "func_96653_e", "e") : nullptr;
            
            jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
            jmethodID getPlayersTeamM = lc->GetMethodID(sbCls, "getPlayersTeam", "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;", "func_96509_i", "h", "(Ljava/lang/String;)Laul;");
            jmethodID formatNameM = teamCls ? lc->GetStaticMethodID(teamCls, "formatPlayerName", "(Lnet/minecraft/scoreboard/Team;Ljava/lang/String;)Ljava/lang/String;", "func_96667_a", "a", "(Laum;Ljava/lang/String;)Ljava/lang/String;") : nullptr;

            for (int i = 0; i < len; i++) {
                jobject scoreItem = env->GetObjectArrayElement(arr, i);
                if (scoreItem) {
                    if (getPlayerNameM) {
                        jstring jpn = (jstring)env->CallObjectMethod(scoreItem, getPlayerNameM);
                        if (jpn) {
                            jobject teamObj = (getPlayersTeamM) ? env->CallObjectMethod(sb, getPlayersTeamM, jpn) : nullptr;
                            jstring formatted = nullptr;
                            if (formatNameM && teamObj) {
                                formatted = (jstring)env->CallStaticObjectMethod(teamCls, formatNameM, teamObj, jpn);
                            }
                            if (!formatted) formatted = jpn;
                            env->CallBooleanMethod(list, addM, formatted);
                            if (formatted != jpn) env->DeleteLocalRef(formatted);
                            if (teamObj) env->DeleteLocalRef(teamObj);
                            env->DeleteLocalRef(jpn);
                        }
                    }
                    env->DeleteLocalRef(scoreItem);
                }
            }
            env->DeleteLocalRef(arr);
        }

        env->DeleteLocalRef(sb);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return list;
    }

    void JNICALL playSound(JNIEnv* env, jobject obj, jstring sound, jfloat volume, jfloat pitch, jdouble x, jdouble y, jdouble z) {
        if (!sound) return;

        if (x == 0 && y == 0 && z == 0) {
            jobject player = getPlayer(env);
            if (player) {
                jclass playerCls = env->GetObjectClass(player);
                jmethodID playSoundM = lc->GetMethodID(playerCls, "playSound", "(Ljava/lang/String;FF)V", "func_85030_a", "a");
                if (!playSoundM) playSoundM = lc->FindMethodBySignature(playerCls, "(Ljava/lang/String;FF)V");
                
                if (playSoundM) {
                    env->CallVoidMethod(player, playSoundM, sound, volume, pitch);
                    env->DeleteLocalRef(playerCls);
                    env->DeleteLocalRef(player);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    return;
                }
                env->DeleteLocalRef(playerCls);
                env->DeleteLocalRef(player);
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }

        jobject world = getWorld(env);
        if (world) {
            jclass worldCls = env->GetObjectClass(world);
            jmethodID playSoundM = lc->GetMethodID(worldCls, "playSound", "(DDDLjava/lang/String;FFZ)V", "func_72980_b", "a");
            if (!playSoundM) playSoundM = lc->FindMethodBySignature(worldCls, "(DDDLjava/lang/String;FFZ)V");
            
            if (playSoundM) {
                env->CallVoidMethod(world, playSoundM, x, y, z, sound, volume, pitch, JNI_FALSE);
            }
            env->DeleteLocalRef(worldCls);
            env->DeleteLocalRef(world);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    jobject JNICALL spawnClientEntity(JNIEnv* env, jobject obj, jstring name, jdouble x, jdouble y, jdouble z) {
        return nullptr; // STUB
    }

    void JNICALL removeClientEntity(JNIEnv* env, jobject obj, jobject entity) {
        // STUB
    }

    void JNICALL clearClientEntities(JNIEnv* env, jobject obj) {
        // STUB
    }

    jstring JNICALL getTitleText(JNIEnv* env, jobject obj) {
        return env->NewStringUTF(""); // STUB
    }

    void JNICALL setTitleText(JNIEnv* env, jobject obj, jstring title, jstring subtitle, jint fadeIn, jint stay, jint fadeOut) {
        // STUB
    }

    void JNICALL clearTitleText(JNIEnv* env, jobject obj) {
        // STUB
    }

    jstring JNICALL getBiomeAt(JNIEnv* env, jobject obj, jint x, jint z) {
        return env->NewStringUTF("plains"); // STUB
    }

    static JNINativeMethod methods[] = {
        {(char*)"exists", (char*)"()Z", (void*)exists},
        {(char*)"getDimension", (char*)"()I", (void*)getDimension},
        {(char*)"getWorldTime", (char*)"()J", (void*)getWorldTime},
        {(char*)"getTotalWorldTime", (char*)"()J", (void*)getTotalWorldTime},
        {(char*)"isRaining", (char*)"()Z", (void*)isRaining},
        {(char*)"isThundering", (char*)"()Z", (void*)isThundering},
        {(char*)"getSunAngle", (char*)"()F", (void*)getSunAngle},
        {(char*)"getDifficulty", (char*)"()I", (void*)getDifficulty},
        {(char*)"getEntities", (char*)"()Ljava/util/List;", (void*)getEntities},
        {(char*)"getPlayerEntities", (char*)"()Ljava/util/List;", (void*)getPlayerEntities},
        {(char*)"getEntityById", (char*)"(I)Lnet/ovson/api/model/Entity;", (void*)getEntityById},
        {(char*)"isValidEntity", (char*)"(Lnet/ovson/api/model/Entity;)Z", (void*)isValidEntity},
        {(char*)"getEntitiesInRadius", (char*)"(Lnet/ovson/api/model/Vec3;D)Ljava/util/List;", (void*)getEntitiesInRadius},
        {(char*)"getEntitiesByType", (char*)"(Ljava/lang/String;)Ljava/util/List;", (void*)getEntitiesByType},
        {(char*)"getClosestEntity", (char*)"(D)Lnet/ovson/api/model/Entity;", (void*)getClosestEntity},
        {(char*)"getClosestPlayer", (char*)"(D)Lnet/ovson/api/model/Entity;", (void*)getClosestPlayer},
        {(char*)"getBlockIdAt", (char*)"(III)I", (void*)getBlockIdAt},
        {(char*)"getLightLevelAt", (char*)"(III)I", (void*)getLightLevelAt},
        {(char*)"isBlockSolid", (char*)"(III)Z", (void*)isBlockSolid},
        {(char*)"canSeeBlock", (char*)"(Lnet/ovson/api/model/Vec3;Lnet/ovson/api/model/Vec3;)Z", (void*)canSeeBlock},
        {(char*)"getTileEntities", (char*)"()Ljava/util/List;", (void*)getTileEntities},
        {(char*)"getNetworkPlayers", (char*)"()Ljava/util/List;", (void*)getNetworkPlayers},
        {(char*)"getTabHeader", (char*)"()Ljava/lang/String;", (void*)getTabHeader},
        {(char*)"getTabFooter", (char*)"()Ljava/lang/String;", (void*)getTabFooter},
        {(char*)"getTeams", (char*)"()Ljava/util/Map;", (void*)getTeams},
        {(char*)"getScoreboardLines", (char*)"()Ljava/util/List;", (void*)getScoreboardLines},
        {(char*)"playSound", (char*)"(Ljava/lang/String;FFDDD)V", (void*)playSound},
        {(char*)"spawnClientEntity", (char*)"(Ljava/lang/String;DDD)Lnet/ovson/api/model/Entity;", (void*)spawnClientEntity},
        {(char*)"removeClientEntity", (char*)"(Lnet/ovson/api/model/Entity;)V", (void*)removeClientEntity},
        {(char*)"clearClientEntities", (char*)"()V", (void*)clearClientEntities},
        {(char*)"getTitleText", (char*)"()Ljava/lang/String;", (void*)getTitleText},
        {(char*)"setTitleText", (char*)"(Ljava/lang/String;Ljava/lang/String;III)V", (void*)setTitleText},
        {(char*)"clearTitleText", (char*)"()V", (void*)clearTitleText},
        {(char*)"getBiomeAt", (char*)"(II)Ljava/lang/String;", (void*)getBiomeAt}
    };

    void registerNatives(JNIEnv* env, jclass cls) { env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0])); }
}
