#include "ClickGUI_Bridge.h"
#include "../Java.h"
#include "../Plugins/PluginLoader.h"
#include "../Utils/Logger.h"
#include "ClickGUI.h"
#include <algorithm>

namespace ClickGUIBridge {

static std::vector<CustomJavaModule> s_cachedJavaModules;

const std::vector<CustomJavaModule>& getCachedModules() {
    if (s_cachedJavaModules.empty()) {
        s_cachedJavaModules = fetchCustomModules();
    }
    return s_cachedJavaModules;
}

void clearCache() {
    freeCustomModules(s_cachedJavaModules);
    s_cachedJavaModules.clear();
}

static std::string JStringToStdString(JNIEnv* env, jstring jStr) {
    if (!jStr) return "";
    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    std::string ret(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(jStr, chars);
    return ret;
}

JNIEXPORT void JNICALL Java_net_ovson_api_clickgui_ClickGUI_registerModuleNative
  (JNIEnv *env, jclass cls, jobject moduleObj)
{
    Logger::info("[ClickGUIBridge] Java registered a custom module!");
    clearCache();
    Render::ClickGUI::resetLayoutB();
}

void registerNatives(JNIEnv* env, jclass cls) {
    if (!cls) return;

    JNINativeMethod methods[] = {
        {(char*)"registerModuleNative", (char*)"(Lnet/ovson/api/clickgui/CustomModule;)V", (void*)&Java_net_ovson_api_clickgui_ClickGUI_registerModuleNative}
    };

    jint res = env->RegisterNatives(cls, methods, 1);
    if (res != JNI_OK || env->ExceptionCheck()) {
        Logger::error("[ClickGUIBridge] Failed to register ClickGUI native methods!");
        if (env->ExceptionCheck()) env->ExceptionClear();
    } else {
        Logger::info("[ClickGUIBridge] Registered ClickGUI native methods successfully.");
    }
}

std::vector<CustomJavaModule> fetchCustomModules() {
    std::vector<CustomJavaModule> result;
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env) return result;

    jobject classLoader = PluginLoader::getAPIClassLoader();
    if (!classLoader) return result;

    jclass guiCls = PluginLoader::loadAPIClass(env, "net.ovson.api.clickgui.ClickGUI");
    if (!guiCls) return result;

    jmethodID getModulesM = env->GetStaticMethodID(guiCls, "getCustomModules", "()Ljava/util/List;");
    if (!getModulesM) {
        env->DeleteLocalRef(guiCls);
        return result;
    }

    jobject listObj = env->CallStaticObjectMethod(guiCls, getModulesM);
    env->DeleteLocalRef(guiCls);
    if (!listObj) return result;

    jclass listCls = env->GetObjectClass(listObj);
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");

    int count = env->CallIntMethod(listObj, sizeM);
    for (int i = 0; i < count; i++) {
        jobject moduleObj = env->CallObjectMethod(listObj, getM, i);
        if (!moduleObj) continue;

        jclass moduleCls = env->GetObjectClass(moduleObj);
        jmethodID getNameM = env->GetMethodID(moduleCls, "getName", "()Ljava/lang/String;");
        jmethodID getCategoryM = env->GetMethodID(moduleCls, "getCategory", "()Ljava/lang/String;");
        jmethodID getDescM = env->GetMethodID(moduleCls, "getDescription", "()Ljava/lang/String;");
        jmethodID isEnabledM = env->GetMethodID(moduleCls, "isEnabled", "()Z");
        jmethodID getSettingsM = env->GetMethodID(moduleCls, "getSettings", "()Ljava/util/List;");

        CustomJavaModule mod;
        mod.moduleObj = env->NewGlobalRef(moduleObj);

        jstring jName = (jstring)env->CallObjectMethod(moduleObj, getNameM);
        mod.name = JStringToStdString(env, jName);
        if (jName) env->DeleteLocalRef(jName);

        jstring jCat = (jstring)env->CallObjectMethod(moduleObj, getCategoryM);
        mod.category = JStringToStdString(env, jCat);
        if (jCat) env->DeleteLocalRef(jCat);

        jstring jDesc = (jstring)env->CallObjectMethod(moduleObj, getDescM);
        mod.description = JStringToStdString(env, jDesc);
        if (jDesc) env->DeleteLocalRef(jDesc);

        mod.enabled = env->CallBooleanMethod(moduleObj, isEnabledM);

        jobject settingsListObj = env->CallObjectMethod(moduleObj, getSettingsM);
        if (settingsListObj) {
            jclass sListCls = env->GetObjectClass(settingsListObj);
            jmethodID sSizeM = env->GetMethodID(sListCls, "size", "()I");
            jmethodID sGetM = env->GetMethodID(sListCls, "get", "(I)Ljava/lang/Object;");

            int sCount = env->CallIntMethod(settingsListObj, sSizeM);
            for (int j = 0; j < sCount; j++) {
                jobject settingObj = env->CallObjectMethod(settingsListObj, sGetM, j);
                if (!settingObj) continue;

                jclass settingCls = env->GetObjectClass(settingObj);
                jmethodID sGetNameM = env->GetMethodID(settingCls, "getName", "()Ljava/lang/String;");
                jmethodID sGetKindM = env->GetMethodID(settingCls, "getKind", "()Ljava/lang/String;");

                CustomJavaSetting set;
                set.settingObj = env->NewGlobalRef(settingObj);

                jstring jsName = (jstring)env->CallObjectMethod(settingObj, sGetNameM);
                set.name = JStringToStdString(env, jsName);
                if (jsName) env->DeleteLocalRef(jsName);

                jstring jsKind = (jstring)env->CallObjectMethod(settingObj, sGetKindM);
                set.kind = JStringToStdString(env, jsKind);
                if (jsKind) env->DeleteLocalRef(jsKind);

                if (set.kind == "slider") {
                    jmethodID getMinM = env->GetMethodID(settingCls, "getMin", "()F");
                    jmethodID getMaxM = env->GetMethodID(settingCls, "getMax", "()F");
                    set.minVal = env->CallFloatMethod(settingObj, getMinM);
                    set.maxVal = env->CallFloatMethod(settingObj, getMaxM);
                } else if (set.kind == "choice") {
                    jmethodID getChoicesM = env->GetMethodID(settingCls, "getChoices", "()[Ljava/lang/String;");
                    jobjectArray choicesArr = (jobjectArray)env->CallObjectMethod(settingObj, getChoicesM);
                    if (choicesArr) {
                        jsize len = env->GetArrayLength(choicesArr);
                        for (jsize k = 0; k < len; k++) {
                            jstring jChoice = (jstring)env->GetObjectArrayElement(choicesArr, k);
                            set.choices.push_back(JStringToStdString(env, jChoice));
                            if (jChoice) env->DeleteLocalRef(jChoice);
                        }
                        env->DeleteLocalRef(choicesArr);
                    }
                }

                mod.settings.push_back(set);
                env->DeleteLocalRef(settingCls);
                env->DeleteLocalRef(settingObj);
            }
            env->DeleteLocalRef(sListCls);
            env->DeleteLocalRef(settingsListObj);
        }

        result.push_back(mod);
        env->DeleteLocalRef(moduleCls);
        env->DeleteLocalRef(moduleObj);
    }

    env->DeleteLocalRef(listCls);
    env->DeleteLocalRef(listObj);

    if (env->ExceptionCheck()) env->ExceptionClear();
    return result;
}

void freeCustomModules(std::vector<CustomJavaModule>& modules) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env) return;

    for (auto& mod : modules) {
        if (mod.moduleObj) {
            env->DeleteGlobalRef(mod.moduleObj);
            mod.moduleObj = nullptr;
        }
        for (auto& set : mod.settings) {
            if (set.settingObj) {
                env->DeleteGlobalRef(set.settingObj);
                set.settingObj = nullptr;
            }
        }
    }
}

bool hasCustomModules() {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env) return false;

    jobject classLoader = PluginLoader::getAPIClassLoader();
    if (!classLoader) return false;

    jclass guiCls = PluginLoader::loadAPIClass(env, "net.ovson.api.clickgui.ClickGUI");
    if (!guiCls) return false;

    jmethodID getModulesM = env->GetStaticMethodID(guiCls, "getCustomModules", "()Ljava/util/List;");
    if (!getModulesM) {
        env->DeleteLocalRef(guiCls);
        return false;
    }

    jobject listObj = env->CallStaticObjectMethod(guiCls, getModulesM);
    env->DeleteLocalRef(guiCls);
    if (!listObj) return false;

    jclass listCls = env->GetObjectClass(listObj);
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    int count = env->CallIntMethod(listObj, sizeM);

    env->DeleteLocalRef(listCls);
    env->DeleteLocalRef(listObj);
    if (env->ExceptionCheck()) env->ExceptionClear();

    return count > 0;
}

void setModuleEnabled(jobject moduleObj, bool enabled) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !moduleObj) return;

    jclass cls = env->GetObjectClass(moduleObj);
    jmethodID setEn = env->GetMethodID(cls, "setEnabled", "(Z)V");
    if (setEn) {
        env->CallVoidMethod(moduleObj, setEn, enabled ? JNI_TRUE : JNI_FALSE);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void setToggleValue(jobject settingObj, bool val) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID setVal = env->GetMethodID(cls, "setValue", "(Z)V");
    if (setVal) {
        env->CallVoidMethod(settingObj, setVal, val ? JNI_TRUE : JNI_FALSE);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void setSliderValue(jobject settingObj, float val) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID setVal = env->GetMethodID(cls, "setValue", "(F)V");
    if (setVal) {
        env->CallVoidMethod(settingObj, setVal, val);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void setChoiceValue(jobject settingObj, const std::string& val) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID setVal = env->GetMethodID(cls, "setValue", "(Ljava/lang/String;)V");
    if (setVal) {
        jstring jval = env->NewStringUTF(val.c_str());
        env->CallVoidMethod(settingObj, setVal, jval);
        env->DeleteLocalRef(jval);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

void setInputValue(jobject settingObj, const std::string& val) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID setVal = env->GetMethodID(cls, "setValue", "(Ljava/lang/String;)V");
    if (setVal) {
        jstring jval = env->NewStringUTF(val.c_str());
        env->CallVoidMethod(settingObj, setVal, jval);
        env->DeleteLocalRef(jval);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

bool getToggleValue(jobject settingObj) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return false;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID getVal = env->GetMethodID(cls, "getValue", "()Z");
    bool ret = false;
    if (getVal) {
        ret = env->CallBooleanMethod(settingObj, getVal);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

float getSliderValue(jobject settingObj) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return 0.0f;

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID getVal = env->GetMethodID(cls, "getValue", "()F");
    float ret = 0.0f;
    if (getVal) {
        ret = env->CallFloatMethod(settingObj, getVal);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

std::string getChoiceValue(jobject settingObj) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return "";

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID getVal = env->GetMethodID(cls, "getValue", "()Ljava/lang/String;");
    std::string ret = "";
    if (getVal) {
        jstring jval = (jstring)env->CallObjectMethod(settingObj, getVal);
        ret = JStringToStdString(env, jval);
        if (jval) env->DeleteLocalRef(jval);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

std::string getInputValue(jobject settingObj) {
    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (!env || !settingObj) return "";

    jclass cls = env->GetObjectClass(settingObj);
    jmethodID getVal = env->GetMethodID(cls, "getValue", "()Ljava/lang/String;");
    std::string ret = "";
    if (getVal) {
        jstring jval = (jstring)env->CallObjectMethod(settingObj, getVal);
        ret = JStringToStdString(env, jval);
        if (jval) env->DeleteLocalRef(jval);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return ret;
}

void requestLayoutRefresh() {
    clearCache();
    Render::ClickGUI::resetLayoutB();
}

} // namespace ClickGUIBridge
