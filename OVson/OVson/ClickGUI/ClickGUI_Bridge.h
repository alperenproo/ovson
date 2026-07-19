#pragma once

#include <jni.h>
#include <string>
#include <vector>

namespace ClickGUIBridge {

    struct CustomJavaSetting {
        std::string name;
        std::string kind;
        jobject settingObj;

        std::vector<std::string> choices;
        
        float minVal;
        float maxVal;

        bool typingState = false;
        std::string inputBuf = "";
    };

    struct CustomJavaModule {
        std::string name;
        std::string category;
        std::string description;
        bool enabled;
        jobject moduleObj;
        std::vector<CustomJavaSetting> settings;
    };

    void registerNatives(JNIEnv* env, jclass cls);

    std::vector<CustomJavaModule> fetchCustomModules();

    const std::vector<CustomJavaModule>& getCachedModules();

    void clearCache();

    void freeCustomModules(std::vector<CustomJavaModule>& modules);

    bool hasCustomModules();

    void setModuleEnabled(jobject moduleObj, bool enabled);
    void setToggleValue(jobject settingObj, bool val);
    void setSliderValue(jobject settingObj, float val);
    void setChoiceValue(jobject settingObj, const std::string& val);
    void setInputValue(jobject settingObj, const std::string& val);

    bool getToggleValue(jobject settingObj);
    float getSliderValue(jobject settingObj);
    std::string getChoiceValue(jobject settingObj);
    std::string getInputValue(jobject settingObj);

    void requestLayoutRefresh();
}
