#include "PluginLoader.h"
#include "../Java.h"
#include "../Utils/Logger.h"
#include "../Chat/ChatAPI_Bridge.h"
#include "../ClickGUI/ClickGUI_Bridge.h"
#include "../resource.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace PluginLoader {

    static jobject s_eventBusObj = nullptr;
    static jmethodID s_postEventMethod = nullptr;
    static jobject s_classLoaderRef = nullptr;

    static std::wstring getAppDataDir() {
        wchar_t* localAppData;
        if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData) == S_OK) {
            std::wstring path = std::wstring(localAppData) + L"\\OVson";
            CoTaskMemFree(localAppData);
            return path;
        }
        return L"";
    }

    static jclass loadClassViaLoader(JNIEnv* env, jobject classLoader, const char* dotName) {
        jclass clsLoaderClass = env->GetObjectClass(classLoader);
        jmethodID loadClassMethod = env->GetMethodID(clsLoaderClass, "loadClass",
                                                      "(Ljava/lang/String;)Ljava/lang/Class;");
        env->DeleteLocalRef(clsLoaderClass);
        if (!loadClassMethod) return nullptr;

        jstring jName = env->NewStringUTF(dotName);
        jclass result = (jclass)env->CallObjectMethod(classLoader, loadClassMethod, jName);
        env->DeleteLocalRef(jName);

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return nullptr;
        }
        return result;
    }

    void initialize() {
        JNIEnv* env = lc->getEnv();
        if (!env) return;

        Logger::info("[PluginLoader] Initializing Plugin Loader...");

        std::wstring appDataDir = getAppDataDir();
        if (appDataDir.empty()) return;
        std::wstring pluginsDir = appDataDir + L"\\plugins";
        std::wstring apiJarPath = appDataDir + L"\\OVsonAPI.jar";

        if (!fs::exists(pluginsDir)) {
            fs::create_directories(pluginsDir);
        }

        HMODULE hMod = GetModuleHandleW(L"OVson.dll");
        if (hMod) {
            HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(IDR_OVSON_API_JAR), MAKEINTRESOURCEW(10));
            if (hRes) {
                HGLOBAL hLoad = LoadResource(hMod, hRes);
                if (hLoad) {
                    DWORD size = SizeofResource(hMod, hRes);
                    void* data = LockResource(hLoad);
                    if (data && size > 0) {
                        FILE* f = nullptr;
                        if (_wfopen_s(&f, apiJarPath.c_str(), L"wb") == 0 && f) {
                            fwrite(data, 1, size, f);
                            fclose(f);
                            Logger::info("[PluginLoader] Successfully extracted/updated OVsonAPI.jar from resources.");
                        } else {
                            Logger::error("[PluginLoader] Failed to write extracted OVsonAPI.jar.");
                        }
                    }
                }
            } else {
                Logger::error("[PluginLoader] OVsonAPI.jar resource not found in DLL!");
            }
        } else {
            Logger::error("[PluginLoader] Could not get module handle for OVson.dll!");
        }

        if (!fs::exists(apiJarPath)) {
            Logger::error("[PluginLoader] OVsonAPI.jar not found and extraction failed!");
            return;
        }

        jclass mcClass = lc->GetClass("net.minecraft.client.Minecraft");
        if (!mcClass) {
            Logger::error("[PluginLoader] Could not find Minecraft class.");
            return;
        }
        jclass classClass = env->FindClass("java/lang/Class");
        jmethodID getClassLoaderMethod = env->GetMethodID(classClass, "getClassLoader",
                                                           "()Ljava/lang/ClassLoader;");
        jobject mcClassLoader = env->CallObjectMethod(mcClass, getClassLoaderMethod);
        env->DeleteLocalRef(classClass);

        if (!mcClassLoader) {
            Logger::error("[PluginLoader] Minecraft ClassLoader is null!");
            return;
        }

        jclass fileClass = env->FindClass("java/io/File");
        jmethodID fileCtor = env->GetMethodID(fileClass, "<init>", "(Ljava/lang/String;)V");
        jmethodID fileToURI = env->GetMethodID(fileClass, "toURI", "()Ljava/net/URI;");
        jclass uriClass = env->FindClass("java/net/URI");
        jmethodID uriToURL = env->GetMethodID(uriClass, "toURL", "()Ljava/net/URL;");

        std::string utf8ApiJar = fs::path(apiJarPath).string();
        jstring jPath = env->NewStringUTF(utf8ApiJar.c_str());
        jobject fileObj = env->NewObject(fileClass, fileCtor, jPath);
        jobject uriObj = env->CallObjectMethod(fileObj, fileToURI);
        jobject urlObj = env->CallObjectMethod(uriObj, uriToURL);

        jclass urlClassLoaderCls = env->FindClass("java/net/URLClassLoader");
        jmethodID urlLoaderCtor = env->GetMethodID(urlClassLoaderCls, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
        
        jclass urlCls = env->FindClass("java/net/URL");
        jobjectArray urlArray = env->NewObjectArray(1, urlCls, urlObj);
        
        jobject ovsonLoader = env->NewObject(urlClassLoaderCls, urlLoaderCtor, urlArray, mcClassLoader);
        
        if (env->ExceptionCheck() || !ovsonLoader) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            Logger::error("[PluginLoader] Failed to create OVson URLClassLoader!");
            return;
        }

        s_classLoaderRef = env->NewGlobalRef(ovsonLoader);

        Logger::info("[PluginLoader] Successfully created custom URLClassLoader for OVsonAPI.jar");

        jclass eventBusCls = loadClassViaLoader(env, s_classLoaderRef,
                                                 "net.ovson.api.event.EventBus");
        if (eventBusCls) {
            Logger::info("[PluginLoader] EventBus class loaded successfully.");
            jmethodID getInstanceMethod = env->GetStaticMethodID(eventBusCls, "getInstance",
                                                                  "()Lnet/ovson/api/event/EventBus;");
            if (getInstanceMethod) {
                jobject eventBusLocal = env->CallStaticObjectMethod(eventBusCls, getInstanceMethod);
                if (eventBusLocal) {
                    s_eventBusObj = env->NewGlobalRef(eventBusLocal);
                    env->DeleteLocalRef(eventBusLocal);
                    s_postEventMethod = env->GetMethodID(eventBusCls, "post",
                                                          "(Lnet/ovson/api/event/Event;)V");
                    Logger::info("[PluginLoader] EventBus post method resolved.");
                }
            }
            env->DeleteLocalRef(eventBusCls);
        } else {
            Logger::error("[PluginLoader] Could not load EventBus class via ClassLoader!");
        }

        jclass chatApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.chat.ChatAPI");
        if (chatApiClass) {
            ChatAPIBridge::registerNatives(env, chatApiClass);
            env->DeleteLocalRef(chatApiClass);
        } else {
            Logger::error("[PluginLoader] Could not load ChatAPI class!");
        }

        jclass clickGuiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.clickgui.ClickGUI");
        if (clickGuiClass) {
            ClickGUIBridge::registerNatives(env, clickGuiClass);
            env->DeleteLocalRef(clickGuiClass);
        } else {
            Logger::error("[PluginLoader] Could not load ClickGUI class!");
        }

        jclass pmClass = loadClassViaLoader(env, s_classLoaderRef,
                                             "net.ovson.api.PluginManager");
        if (pmClass) {
            Logger::info("[PluginLoader] PluginManager class loaded successfully.");
            jmethodID loadMethod = env->GetStaticMethodID(pmClass, "loadPlugins",
                "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
            if (loadMethod) {
                std::string utf8Dir = fs::path(pluginsDir).string();
                jstring jPluginsPath = env->NewStringUTF(utf8Dir.c_str());

                Logger::info("[PluginLoader] Loading plugins from: %s", utf8Dir.c_str());
                env->CallStaticVoidMethod(pmClass, loadMethod, jPluginsPath, s_classLoaderRef);

                if (env->ExceptionCheck()) {
                    Logger::error("[PluginLoader] Exception in PluginManager.loadPlugins");
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(jPluginsPath);
            }
            env->DeleteLocalRef(pmClass);
        } else {
            Logger::error("[PluginLoader] Could not load PluginManager class!");
        }

        env->DeleteLocalRef(mcClassLoader);
        env->DeleteLocalRef(ovsonLoader);
        env->DeleteLocalRef(urlArray);
        env->DeleteLocalRef(urlCls);
        env->DeleteLocalRef(urlClassLoaderCls);
        env->DeleteLocalRef(jPath);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(fileClass);
        env->DeleteLocalRef(uriClass);
        Logger::info("[PluginLoader] Initialization complete.");
    }

    void shutdown() {
        JNIEnv* env = lc->getEnv();
        if (!env) return;

        Logger::info("[PluginLoader] Disabling all plugins...");

        if (s_classLoaderRef) {
            jclass pmClass = loadClassViaLoader(env, s_classLoaderRef,
                                                 "net.ovson.api.PluginManager");
            if (pmClass) {
                Logger::info("[PluginLoader] Found PluginManager in shutdown");
                jmethodID disableMethod = env->GetStaticMethodID(pmClass, "disablePlugins", "()V");
                if (disableMethod) {
                    Logger::info("[PluginLoader] Calling disablePlugins...");
                    env->CallStaticVoidMethod(pmClass, disableMethod);
                    Logger::info("[PluginLoader] disablePlugins called.");
                } else {
                    Logger::error("[PluginLoader] disablePlugins method not found!");
                }
                env->DeleteLocalRef(pmClass);
            } else {
                Logger::error("[PluginLoader] Could not load PluginManager during shutdown!");
            }
            env->DeleteGlobalRef(s_classLoaderRef);
            s_classLoaderRef = nullptr;
        }

        if (s_eventBusObj) {
            env->DeleteGlobalRef(s_eventBusObj);
            s_eventBusObj = nullptr;
        }
    }

    const std::vector<PluginContext>& getLoadedPlugins() {
        static std::vector<PluginContext> empty;
        return empty;
    }

    jobject getEventBus() {
        return s_eventBusObj;
    }

    void postEvent(jobject eventInstance) {
        if (!s_eventBusObj || !s_postEventMethod || !eventInstance) return;
        JNIEnv* env = lc->getEnv();
        if (!env) return;

        env->CallVoidMethod(s_eventBusObj, s_postEventMethod, eventInstance);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    jclass getPluginManagerClass(JNIEnv* env) {
        if (!env || !s_classLoaderRef) return nullptr;
        return loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.PluginManager");
    }

    jobject getAPIClassLoader() {
        return s_classLoaderRef;
    }

    jclass loadAPIClass(JNIEnv* env, const char* name) {
        if (!env || !s_classLoaderRef) return nullptr;
        return loadClassViaLoader(env, s_classLoaderRef, name);
    }
}
