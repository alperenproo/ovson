#include "AdvancedAPI_Bridge.h"
#include "../Render/NotificationManager.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include <string>
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

int g_fakeLagDurationMs = 0;
int g_chokedPacketsCount = 0;

namespace AdvancedAPIBridge {

    static std::string jstringToString(JNIEnv* env, jstring jstr) {
        if (!jstr) return "";
        const char* chars = env->GetStringUTFChars(jstr, nullptr);
        std::string result(chars);
        env->ReleaseStringUTFChars(jstr, chars);
        return result;
    }

    void JNICALL sendNotification(JNIEnv* env, jclass clazz, jstring jtitle, jstring jmessage, jint type, jfloat duration) {
        std::string title = jstringToString(env, jtitle);
        std::string msg = jstringToString(env, jmessage);
        
        Render::NotificationType nType = Render::NotificationType::Info;
        if(type == 1) nType = Render::NotificationType::Success;
        else if(type == 2) nType = Render::NotificationType::Warning;
        else if(type == 3) nType = Render::NotificationType::Error;

        Render::NotificationManager::getInstance()->add(title, msg, nType, duration);
    }

    static const JNINativeMethod notificationMethods[] = {
        {(char*)"sendNotification", (char*)"(Ljava/lang/String;Ljava/lang/String;IF)V", (void*)sendNotification}
    };

    void registerNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, notificationMethods, sizeof(notificationMethods)/sizeof(notificationMethods[0]));
    }

    jstring JNICALL getBedLocation(JNIEnv* env, jclass clazz, jstring jteam) {
        std::string team = jstringToString(env, jteam);
        auto* mgr = BedDefense::BedDefenseManager::getInstance();
        std::lock_guard<std::mutex> lock(mgr->getMutex());
        const auto& beds = mgr->getBeds();
        for(const auto& pair : beds) {
            if(pair.second.teamColor == team) {
                std::string loc = std::to_string(pair.second.x) + "," + std::to_string(pair.second.y) + "," + std::to_string(pair.second.z);
                return env->NewStringUTF(loc.c_str());
            }
        }
        return env->NewStringUTF("");
    }

    jstring JNICALL getBedLayers(JNIEnv* env, jclass clazz, jstring jteam) {
        std::string team = jstringToString(env, jteam);
        std::string layersStr = "";
        auto* mgr = BedDefense::BedDefenseManager::getInstance();
        std::lock_guard<std::mutex> lock(mgr->getMutex());
        const auto& beds = mgr->getBeds();
        for(const auto& pair : beds) {
            if(pair.second.teamColor == team) {
                for(size_t i = 0; i < pair.second.layers.size(); i++) {
                    layersStr += pair.second.layers[i].blockName;
                    if(i != pair.second.layers.size() - 1) layersStr += ",";
                }
                break;
            }
        }
        return env->NewStringUTF(layersStr.c_str());
    }

    static const JNINativeMethod bedMethods[] = {
        {(char*)"getBedLocation", (char*)"(Ljava/lang/String;)Ljava/lang/String;", (void*)getBedLocation},
        {(char*)"getBedLayers", (char*)"(Ljava/lang/String;)Ljava/lang/String;", (void*)getBedLayers}
    };

    void registerBedDefenseNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, bedMethods, sizeof(bedMethods)/sizeof(bedMethods[0]));
    }

    void JNICALL playSoundFile(JNIEnv* env, jclass clazz, jstring jpath, jfloat volume) {
        std::string path = jstringToString(env, jpath);
        PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }

    jstring JNICALL getClipboard(JNIEnv* env, jclass clazz) {
        if (!OpenClipboard(nullptr)) return env->NewStringUTF("");
        HANDLE hData = GetClipboardData(CF_TEXT);
        std::string result = "";
        if (hData != nullptr) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText != nullptr) {
                result = pszText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
        return env->NewStringUTF(result.c_str());
    }

    void JNICALL setClipboard(JNIEnv* env, jclass clazz, jstring jtext) {
        std::string text = jstringToString(env, jtext);
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }

    static const JNINativeMethod audioMethods[] = {
        {(char*)"playSoundFile", (char*)"(Ljava/lang/String;F)V", (void*)playSoundFile},
        {(char*)"getClipboard", (char*)"()Ljava/lang/String;", (void*)getClipboard},
        {(char*)"setClipboard", (char*)"(Ljava/lang/String;)V", (void*)setClipboard}
    };

    void registerAudioNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, audioMethods, sizeof(audioMethods)/sizeof(audioMethods[0]));
    }

    void JNICALL setFakeLag(JNIEnv* env, jclass clazz, jint ms) {
        g_fakeLagDurationMs = ms;
    }

    jint JNICALL getChokedPacketsCount(JNIEnv* env, jclass clazz) {
        return g_chokedPacketsCount;
    }

    static const JNINativeMethod spoofMethods[] = {
        {(char*)"setFakeLag", (char*)"(I)V", (void*)setFakeLag},
        {(char*)"getChokedPacketsCount", (char*)"()I", (void*)getChokedPacketsCount}
    };

    void registerSpoofNatives(JNIEnv* env, jclass cls) {
        env->RegisterNatives(cls, spoofMethods, sizeof(spoofMethods)/sizeof(spoofMethods[0]));
    }
}
