#include "PluginLoader.h"
#include "../Java.h"
#include "../Utils/Logger.h"
#include "../Chat/ChatAPI_Bridge.h"
#include "../ClickGUI/ClickGUI_Bridge.h"
#include "PlayerAPI_Bridge.h"
#include "OVsonAPI_Bridge.h"
#include "RenderAPI_Bridge.h"
#include "GL_Bridge.h"
#include "EventDispatcher.h"
#include "WorldAPI_Bridge.h"
#include "ScoreboardAPI_Bridge.h"
#include "InventoryAPI_Bridge.h"
#include "KeybindAPI_Bridge.h"
#include "PacketAPI_Bridge.h"
#include "StatsAPI_Bridge.h"
#include "AdvancedAPI_Bridge.h"
#include "EntityAPI_Bridge.h"
#include "../resource.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <unordered_map>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace fs = std::filesystem;

namespace PluginLoader {

    static jobject s_eventBusObj = nullptr;
    static jmethodID s_postEventMethod = nullptr;
    static jobject s_classLoaderRef = nullptr;
    static std::atomic<bool> s_hasPlugins{false};

    std::wstring getAppDataDir() {
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

        if (!fs::exists(apiJarPath)) {
            HMODULE hMod = (HMODULE)&__ImageBase;
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
                                Logger::info("[PluginLoader] Successfully extracted OVsonAPI.jar from resources.");
                            } else {
                                Logger::info("[PluginLoader] Failed to write extracted OVsonAPI.jar.");
                            }
                        }
                    }
                } else {
                    Logger::error("[PluginLoader] OVsonAPI.jar resource not found in DLL!");
                }
            } else {
                Logger::error("[PluginLoader] Could not get module handle for OVson.dll!");
            }
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

        jclass playerApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.player.PlayerAPI");
        if (playerApiClass) {
            PlayerAPIBridge::registerNatives(env, playerApiClass);
            env->DeleteLocalRef(playerApiClass);
            Logger::info("[PluginLoader] PlayerAPI natives registered.");
        } else {
            Logger::error("[PluginLoader] Could not load PlayerAPI class!");
        }

        jclass ovsonApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.OVsonAPI");
        if (ovsonApiClass) {
            OVsonAPIBridge::registerNatives(env, ovsonApiClass);
            env->DeleteLocalRef(ovsonApiClass);
            Logger::info("[PluginLoader] OVsonAPI natives registered.");
        } else {
            Logger::error("[PluginLoader] Could not load OVsonAPI class!");
        }

        jclass renderApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.render.RenderAPI");
        if (renderApiClass) {
            RenderAPIBridge::registerNatives(env, renderApiClass);
            env->DeleteLocalRef(renderApiClass);
            Logger::info("[PluginLoader] RenderAPI natives registered.");
        } else {
            Logger::error("[PluginLoader] Could not load RenderAPI class!");
        }

        jclass glClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.render.gl.GL");
        if (glClass) {
            GLBridge::registerNatives(env, glClass);
            env->DeleteLocalRef(glClass);
            Logger::info("[PluginLoader] GL natives registered.");
        } else {
            Logger::error("[PluginLoader] Could not load GL class!");
        }

        jclass worldApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.world.WorldAPI");
        if (worldApiClass) {
            WorldAPIBridge::registerNatives(env, worldApiClass);
            env->DeleteLocalRef(worldApiClass);
            Logger::info("[PluginLoader] WorldAPI natives registered.");
        }

        jclass scoreboardApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.world.ScoreboardAPI");
        if (scoreboardApiClass) {
            ScoreboardAPIBridge::registerNatives(env, scoreboardApiClass);
            env->DeleteLocalRef(scoreboardApiClass);
            Logger::info("[PluginLoader] ScoreboardAPI natives registered.");
        }

        jclass invApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.inventory.InventoryAPI");
        if (invApiClass) {
            InventoryAPIBridge::registerNatives(env, invApiClass);
            env->DeleteLocalRef(invApiClass);
            Logger::info("[PluginLoader] InventoryAPI natives registered.");
        }

        jclass keybindApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.input.KeybindAPI");
        if (keybindApiClass) {
            KeybindAPIBridge::registerNatives(env, keybindApiClass);
            env->DeleteLocalRef(keybindApiClass);
            Logger::info("[PluginLoader] KeybindAPI natives registered.");
        }

        jclass packetFactoryClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.packet.PacketFactory");
        if (packetFactoryClass) {
            PacketAPIBridge::registerFactoryNatives(env, packetFactoryClass);
            env->DeleteLocalRef(packetFactoryClass);
        }
        
        jclass packetHelperClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.packet.PacketHelper");
        if (packetHelperClass) {
            PacketAPIBridge::registerHelperNatives(env, packetHelperClass);
            env->DeleteLocalRef(packetHelperClass);
            Logger::info("[PluginLoader] PacketAPI natives registered.");
        }

        jclass statsApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.player.StatsAPI");
        if (statsApiClass) {
            StatsAPIBridge::registerNatives(env, statsApiClass);
            env->DeleteLocalRef(statsApiClass);
            Logger::info("[PluginLoader] StatsAPI natives registered.");
        }

        jclass acApiClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.player.AnticheatAPI");
        if (acApiClass) {
            StatsAPIBridge::registerAnticheatNatives(env, acApiClass);
            env->DeleteLocalRef(acApiClass);
            Logger::info("[PluginLoader] AnticheatAPI natives registered.");
        }

        jclass notifClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.ui.NotificationAPI");
        if (notifClass) { AdvancedAPIBridge::registerNatives(env, notifClass); env->DeleteLocalRef(notifClass); }

        jclass bedClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.world.BedDefenseAPI");
        if (bedClass) { AdvancedAPIBridge::registerBedDefenseNatives(env, bedClass); env->DeleteLocalRef(bedClass); }

        jclass audioClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.media.AudioAPI");
        if (audioClass) { AdvancedAPIBridge::registerAudioNatives(env, audioClass); env->DeleteLocalRef(audioClass); }

        jclass spoofClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.net.SpoofAPI");
        if (spoofClass) { AdvancedAPIBridge::registerSpoofNatives(env, spoofClass); env->DeleteLocalRef(spoofClass); }

        jclass entityClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.model.Entity");
        if (entityClass) {
            EntityAPIBridge::registerNatives(env, entityClass);
            env->DeleteLocalRef(entityClass);
            Logger::info("[PluginLoader] Entity model natives registered.");
        }

        EventDispatcher::initialize();

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

                jmethodID hasPluginsMethod = env->GetStaticMethodID(pmClass, "hasPlugins", "()Z");
                if (hasPluginsMethod) {
                    jboolean has = env->CallStaticBooleanMethod(pmClass, hasPluginsMethod);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    s_hasPlugins.store(has == JNI_TRUE, std::memory_order_relaxed);
                    Logger::info("[PluginLoader] Has active plugins: %s", (has == JNI_TRUE) ? "YES" : "NO");
                }
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
        JNIEnv* env = lc ? lc->getEnv() : nullptr;
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
            if (env->ExceptionCheck()) env->ExceptionClear();

            auto unregisterCls = [&](const char* name) {
                jclass cls = loadClassViaLoader(env, s_classLoaderRef, name);
                if (cls) {
                    env->UnregisterNatives(cls);
                    env->DeleteLocalRef(cls);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
            };

            unregisterCls("net.ovson.api.chat.ChatAPI");
            unregisterCls("net.ovson.api.clickgui.ClickGUI");
            unregisterCls("net.ovson.api.player.PlayerAPI");
            unregisterCls("net.ovson.api.OVsonAPI");
            unregisterCls("net.ovson.api.render.RenderAPI");
            unregisterCls("net.ovson.api.render.gl.GL");
            unregisterCls("net.ovson.api.world.WorldAPI");
            unregisterCls("net.ovson.api.world.ScoreboardAPI");
            unregisterCls("net.ovson.api.inventory.InventoryAPI");
            unregisterCls("net.ovson.api.input.KeybindAPI");
            unregisterCls("net.ovson.api.packet.PacketFactory");
            unregisterCls("net.ovson.api.packet.PacketHelper");
            unregisterCls("net.ovson.api.player.StatsAPI");
            unregisterCls("net.ovson.api.player.AnticheatAPI");
            unregisterCls("net.ovson.api.ui.NotificationAPI");
            unregisterCls("net.ovson.api.world.BedDefenseAPI");
            unregisterCls("net.ovson.api.media.AudioAPI");
            unregisterCls("net.ovson.api.net.SpoofAPI");
            unregisterCls("net.ovson.api.model.Entity");

            env->DeleteGlobalRef(s_classLoaderRef);
            s_classLoaderRef = nullptr;
        }

        EventDispatcher::shutdown();

        if (s_eventBusObj) {
            env->DeleteGlobalRef(s_eventBusObj);
            s_eventBusObj = nullptr;
        }
        s_hasPlugins.store(false, std::memory_order_relaxed);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    void reloadPlugins() {
        JNIEnv* env = lc ? lc->getEnv() : nullptr;
        if (!env || !s_classLoaderRef) return;

        std::wstring appDataDir = getAppDataDir();
        if (appDataDir.empty()) return;
        std::wstring pluginsDir = appDataDir + L"\\plugins";

        jclass pmClass = loadClassViaLoader(env, s_classLoaderRef, "net.ovson.api.PluginManager");
        if (pmClass) {
            jmethodID loadMethod = env->GetStaticMethodID(pmClass, "loadPlugins",
                "(Ljava/lang/String;Ljava/lang/ClassLoader;)V");
            if (loadMethod) {
                std::string utf8Dir = fs::path(pluginsDir).string();
                jstring jPluginsPath = env->NewStringUTF(utf8Dir.c_str());

                Logger::info("[PluginLoader] Reloading plugins from: %s", utf8Dir.c_str());
                env->CallStaticVoidMethod(pmClass, loadMethod, jPluginsPath, s_classLoaderRef);

                if (env->ExceptionCheck()) {
                    Logger::error("[PluginLoader] Exception in PluginManager.loadPlugins during reload");
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(jPluginsPath);
            }
            env->DeleteLocalRef(pmClass);
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

    bool hasPlugins() {
        return s_hasPlugins.load(std::memory_order_relaxed);
    }
}
