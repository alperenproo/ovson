#include "ScoreboardAPI_Bridge.h"
#include "../Java.h"
#include <string>
#include <vector>

namespace ScoreboardAPIBridge {
    
    static jobject getScoreboard(JNIEnv* env) {
        jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
        if (!mcClass) return nullptr;
        
        jmethodID getMinecraft = lc->GetStaticMethodID(mcClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A");
        if (!getMinecraft) return nullptr;
        jobject mc = env->CallStaticObjectMethod(mcClass, getMinecraft);
        
        jfieldID worldField = lc->GetFieldID(mcClass, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;", "field_71441_e", "f");
        if (!worldField) { env->DeleteLocalRef(mc); return nullptr; }
        
        jobject world = env->GetObjectField(mc, worldField);
        env->DeleteLocalRef(mc);
        if (!world) return nullptr;
        
        jclass worldClass = env->GetObjectClass(world);
        jmethodID getScoreboard = env->GetMethodID(worldClass, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
        if (!getScoreboard) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getScoreboard = env->GetMethodID(worldClass, "func_96441_U", "()Lnet/minecraft/scoreboard/Scoreboard;");
        }
        if (!getScoreboard) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getScoreboard = env->GetMethodID(worldClass, "Z", "()Lauo;");
        }
        env->DeleteLocalRef(worldClass);
        if (!getScoreboard) { env->DeleteLocalRef(world); return nullptr; }
        
        jobject scoreboard = env->CallObjectMethod(world, getScoreboard);
        env->DeleteLocalRef(world);
        return scoreboard;
    }

    static jstring JNICALL getObjectiveTitle(JNIEnv* env, jclass) {
        jobject scoreboard = getScoreboard(env);
        if (!scoreboard) return nullptr;
        
        jclass sbClass = env->GetObjectClass(scoreboard);
        jmethodID getObjective = env->GetMethodID(sbClass, "getObjectiveInDisplaySlot", "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
        if (!getObjective) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getObjective = env->GetMethodID(sbClass, "func_96539_a", "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
        }
        if (!getObjective) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getObjective = env->GetMethodID(sbClass, "a", "(I)Laum;");
        }
        
        if (!getObjective) { env->DeleteLocalRef(sbClass); env->DeleteLocalRef(scoreboard); return nullptr; }
        
        jobject objective = env->CallObjectMethod(scoreboard, getObjective, 1);
        env->DeleteLocalRef(sbClass);
        env->DeleteLocalRef(scoreboard);
        
        if (!objective) return nullptr;
        
        jclass objClass = env->GetObjectClass(objective);
        jmethodID getDisplayName = env->GetMethodID(objClass, "getDisplayName", "()Ljava/lang/String;");
        if (!getDisplayName) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getDisplayName = env->GetMethodID(objClass, "func_96678_d", "()Ljava/lang/String;");
        }
        if (!getDisplayName) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getDisplayName = env->GetMethodID(objClass, "d", "()Ljava/lang/String;");
        }
        
        jstring title = nullptr;
        if (getDisplayName) {
            title = (jstring)env->CallObjectMethod(objective, getDisplayName);
        }
        env->DeleteLocalRef(objClass);
        env->DeleteLocalRef(objective);
        return title;
    }

    static jobject JNICALL getLines(JNIEnv* env, jclass) {
        jobject scoreboard = getScoreboard(env);
        if (!scoreboard) return nullptr;
        
        jclass sbClass = env->GetObjectClass(scoreboard);
        jmethodID getObjective = env->GetMethodID(sbClass, "getObjectiveInDisplaySlot", "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
        if (!getObjective) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getObjective = env->GetMethodID(sbClass, "func_96539_a", "(I)Lnet/minecraft/scoreboard/ScoreObjective;");
        }
        if (!getObjective) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getObjective = env->GetMethodID(sbClass, "a", "(I)Laum;");
        }
        
        jobject objective = nullptr;
        if (getObjective) {
            objective = env->CallObjectMethod(scoreboard, getObjective, 1);
        }
        
        if (!objective) {
            env->DeleteLocalRef(sbClass);
            env->DeleteLocalRef(scoreboard);
            return nullptr;
        }
        
        jmethodID getSortedScores = env->GetMethodID(sbClass, "getSortedScores", "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;");
        if (!getSortedScores) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getSortedScores = env->GetMethodID(sbClass, "func_96534_i", "(Lnet/minecraft/scoreboard/ScoreObjective;)Ljava/util/Collection;");
        }
        if (!getSortedScores) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getSortedScores = env->GetMethodID(sbClass, "i", "(Laum;)Ljava/util/Collection;");
        }
        
        if (!getSortedScores) {
            env->DeleteLocalRef(objective);
            env->DeleteLocalRef(sbClass);
            env->DeleteLocalRef(scoreboard);
            return nullptr;
        }
        
        jobject scoresColl = env->CallObjectMethod(scoreboard, getSortedScores, objective);
        env->DeleteLocalRef(objective);
        
        if (!scoresColl) {
            env->DeleteLocalRef(sbClass);
            env->DeleteLocalRef(scoreboard);
            return nullptr;
        }
        
        jclass collClass = env->GetObjectClass(scoresColl);
        jmethodID toArray = env->GetMethodID(collClass, "toArray", "()[Ljava/lang/Object;");
        jobjectArray scoreArray = (jobjectArray)env->CallObjectMethod(scoresColl, toArray);
        env->DeleteLocalRef(collClass);
        env->DeleteLocalRef(scoresColl);
        
        if (!scoreArray) {
            env->DeleteLocalRef(sbClass);
            env->DeleteLocalRef(scoreboard);
            return nullptr;
        }
        
        jsize len = env->GetArrayLength(scoreArray);
        
        jclass arrayListClass = env->FindClass("java/util/ArrayList");
        jmethodID alCtor = env->GetMethodID(arrayListClass, "<init>", "()V");
        jmethodID alAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
        jobject list = env->NewObject(arrayListClass, alCtor);
        
        jclass scoreCls = lc->GetClass("net.minecraft.scoreboard.Score");
        jmethodID getPlayerName = nullptr;
        if (scoreCls) {
            getPlayerName = env->GetMethodID(scoreCls, "getPlayerName", "()Ljava/lang/String;");
            if (!getPlayerName) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getPlayerName = env->GetMethodID(scoreCls, "func_96653_e", "()Ljava/lang/String;");
            }
            if (!getPlayerName) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getPlayerName = env->GetMethodID(scoreCls, "e", "()Ljava/lang/String;");
            }
        }
        
        jmethodID getPlayersTeam = env->GetMethodID(sbClass, "getPlayersTeam", "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
        if (!getPlayersTeam) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getPlayersTeam = env->GetMethodID(sbClass, "func_96509_i", "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
        }
        if (!getPlayersTeam) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getPlayersTeam = env->GetMethodID(sbClass, "h", "(Ljava/lang/String;)Laul;");
        }
        
        jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
        jmethodID getColorPrefix = nullptr;
        jmethodID getColorSuffix = nullptr;
        if (teamCls) {
            getColorPrefix = env->GetMethodID(teamCls, "getColorPrefix", "()Ljava/lang/String;");
            if (!getColorPrefix) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getColorPrefix = env->GetMethodID(teamCls, "func_96668_e", "()Ljava/lang/String;");
            }
            if (!getColorPrefix) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getColorPrefix = env->GetMethodID(teamCls, "e", "()Ljava/lang/String;");
            }
            
            getColorSuffix = env->GetMethodID(teamCls, "getColorSuffix", "()Ljava/lang/String;");
            if (!getColorSuffix) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getColorSuffix = env->GetMethodID(teamCls, "func_96663_f", "()Ljava/lang/String;");
            }
            if (!getColorSuffix) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                getColorSuffix = env->GetMethodID(teamCls, "f", "()Ljava/lang/String;");
            }
        }
        
        if (scoreCls && getPlayerName && getPlayersTeam && teamCls && getColorPrefix && getColorSuffix) {
            for (jsize i = 0; i < len; ++i) {
                jobject score = env->GetObjectArrayElement(scoreArray, i);
                if (score) {
                    jstring playerName = (jstring)env->CallObjectMethod(score, getPlayerName);
                    if (playerName) {
                        jobject team = env->CallObjectMethod(scoreboard, getPlayersTeam, playerName);
                        std::string fullLine = "";
                        
                        if (team) {
                            jstring prefix = (jstring)env->CallObjectMethod(team, getColorPrefix);
                            if (prefix) {
                                const char* pCStr = env->GetStringUTFChars(prefix, nullptr);
                                fullLine += pCStr;
                                env->ReleaseStringUTFChars(prefix, pCStr);
                                env->DeleteLocalRef(prefix);
                            }
                        }
                        
                        const char* nCStr = env->GetStringUTFChars(playerName, nullptr);
                        fullLine += nCStr;
                        env->ReleaseStringUTFChars(playerName, nCStr);
                        
                        if (team) {
                            jstring suffix = (jstring)env->CallObjectMethod(team, getColorSuffix);
                            if (suffix) {
                                const char* sCStr = env->GetStringUTFChars(suffix, nullptr);
                                fullLine += sCStr;
                                env->ReleaseStringUTFChars(suffix, sCStr);
                                env->DeleteLocalRef(suffix);
                            }
                            env->DeleteLocalRef(team);
                        }
                        
                        jstring jFullLine = env->NewStringUTF(fullLine.c_str());
                        env->CallBooleanMethod(list, alAdd, jFullLine);
                        env->DeleteLocalRef(jFullLine);
                        env->DeleteLocalRef(playerName);
                    }
                    env->DeleteLocalRef(score);
                }
            }
        }
        
        env->DeleteLocalRef(sbClass);
        env->DeleteLocalRef(scoreboard);
        env->DeleteLocalRef(scoreArray);
        env->DeleteLocalRef(arrayListClass);
        
        return list;
    }

    void registerNatives(JNIEnv* env, jclass cls) {
        JNINativeMethod methods[] = {
            {(char*)"getObjectiveTitle", (char*)"()Ljava/lang/String;", (void*)&getObjectiveTitle},
            {(char*)"getLines", (char*)"()Ljava/util/List;", (void*)&getLines}
        };
        env->RegisterNatives(cls, methods, 2);
    }
}
