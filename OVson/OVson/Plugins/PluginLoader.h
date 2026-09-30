#pragma once

#include <string>
#include <vector>
#include <jni.h>

namespace PluginLoader {

    struct PluginContext {
        std::string name;
        std::string version;
        std::string author;
        jobject pluginInstance;
        bool enabled;
    };

    std::wstring getAppDataDir();

    void initialize();

    void shutdown();

    void reloadPlugins();

    const std::vector<PluginContext>& getLoadedPlugins();

    jobject getEventBus();
    
    jclass getPluginManagerClass(JNIEnv* env);
    
    jobject getAPIClassLoader();
    
    jclass loadAPIClass(JNIEnv* env, const char* name);
    
    void postEvent(jobject eventInstance);

    bool hasPlugins();
}
