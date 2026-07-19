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

    void initialize();

    void shutdown();

    const std::vector<PluginContext>& getLoadedPlugins();

    jobject getEventBus();
    
    jclass getPluginManagerClass(JNIEnv* env);
    
    jobject getAPIClassLoader();
    
    jclass loadAPIClass(JNIEnv* env, const char* name);
    
    void postEvent(jobject eventInstance);
}
