#include "EventDispatcher.h"
#include "../Java.h"
#include "PluginLoader.h"
#include <windows.h>
#include <atomic>
#include <chrono>

static std::atomic<bool> s_active{false};

// Cached Refs
static jclass s_render2DEventClass = nullptr;
static jmethodID s_render2DEventCtor = nullptr;
static jclass s_render3DEventClass = nullptr;
static jmethodID s_render3DEventCtor = nullptr;
static jclass s_tickEventClass = nullptr;
static jmethodID s_tickEventCtor = nullptr;
static jclass s_chatReceivedEventClass = nullptr;
static jmethodID s_chatReceivedEventCtor = nullptr;
static jclass s_chatSendEventClass = nullptr;
static jmethodID s_chatSendEventCtor = nullptr;
static jmethodID s_chatSendEventIsCancelled = nullptr;
static jclass s_playerJoinEventClass = nullptr;
static jmethodID s_playerJoinEventCtor = nullptr;
static jclass s_packetSendEventClass = nullptr;
static jmethodID s_packetSendEventCtor = nullptr;
static jmethodID s_packetSendEventIsCancelled = nullptr;
static jclass s_packetReceiveEventClass = nullptr;
static jmethodID s_packetReceiveEventCtor = nullptr;
static jmethodID s_packetReceiveEventIsCancelled = nullptr;
static jclass s_attackEventClass = nullptr;
static jmethodID s_attackEventCtor = nullptr;
static jmethodID s_attackEventIsCancelled = nullptr;
static jclass s_updateEventClass = nullptr;
static jmethodID s_updateEventCtor = nullptr;
static jclass s_preMotionEventClass = nullptr;
static jmethodID s_preMotionEventCtor = nullptr;
static jclass s_postMotionEventClass = nullptr;
static jmethodID s_postMotionEventCtor = nullptr;
static jclass s_keyPressEventClass = nullptr;
static jmethodID s_keyPressEventCtor = nullptr;
static jmethodID s_keyPressEventIsCancelled = nullptr;
static jclass s_keyEventClass = nullptr;
static jmethodID s_keyEventCtor = nullptr;
static jclass s_mouseEventClass = nullptr;
static jmethodID s_mouseEventCtor = nullptr;
static jmethodID s_mouseEventIsCancelled = nullptr;
static jclass s_guiOpenEventClass = nullptr;
static jmethodID s_guiOpenEventCtor = nullptr;
static jmethodID s_guiOpenEventIsCancelled = nullptr;
static jclass s_guiCloseEventClass = nullptr;
static jmethodID s_guiCloseEventCtor = nullptr;
static jclass s_disconnectEventClass = nullptr;
static jclass s_anticheatFlagEventClass = nullptr;
static jmethodID s_disconnectEventCtor = nullptr;
static jmethodID s_anticheatFlagEventCtor = nullptr;

namespace EventDispatcher {

    void initialize() {
        JNIEnv* env = lc ? lc->getEnv() : nullptr;
        if (!env) return;

        auto initEvent = [&](const char* className, const char* sig, jclass& clsOut, jmethodID& ctorOut) {
            jclass localClass = PluginLoader::loadAPIClass(env, className);
            if (localClass) {
                clsOut = (jclass)env->NewGlobalRef(localClass);
                ctorOut = env->GetMethodID(clsOut, "<init>", sig);
                env->DeleteLocalRef(localClass);
            }
        };

        initEvent("net.ovson.api.event.Render2DEvent", "(F)V", s_render2DEventClass, s_render2DEventCtor);
        initEvent("net.ovson.api.event.render.Render3DEvent", "(F)V", s_render3DEventClass, s_render3DEventCtor);
        initEvent("net.ovson.api.event.TickEvent", "()V", s_tickEventClass, s_tickEventCtor);
        initEvent("net.ovson.api.event.ChatReceivedEvent", "(Ljava/lang/String;)V", s_chatReceivedEventClass, s_chatReceivedEventCtor);
        
        initEvent("net.ovson.api.event.ChatSendEvent", "(Ljava/lang/String;)V", s_chatSendEventClass, s_chatSendEventCtor);
        if (s_chatSendEventClass) s_chatSendEventIsCancelled = env->GetMethodID(s_chatSendEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.PlayerJoinEvent", "(Ljava/lang/String;)V", s_playerJoinEventClass, s_playerJoinEventCtor);
        
        initEvent("net.ovson.api.event.network.PacketSendEvent", "(Ljava/lang/Object;Ljava/lang/String;)V", s_packetSendEventClass, s_packetSendEventCtor);
        if (s_packetSendEventClass) s_packetSendEventIsCancelled = env->GetMethodID(s_packetSendEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.network.PacketReceiveEvent", "(Ljava/lang/Object;Ljava/lang/String;)V", s_packetReceiveEventClass, s_packetReceiveEventCtor);
        if (s_packetReceiveEventClass) s_packetReceiveEventIsCancelled = env->GetMethodID(s_packetReceiveEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.player.AttackEvent", "(I)V", s_attackEventClass, s_attackEventCtor);
        if (s_attackEventClass) s_attackEventIsCancelled = env->GetMethodID(s_attackEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.player.UpdateEvent", "(ZFFZ)V", s_updateEventClass, s_updateEventCtor);
        initEvent("net.ovson.api.event.player.PreMotionEvent", "(DDDFFZZZ)V", s_preMotionEventClass, s_preMotionEventCtor);
        initEvent("net.ovson.api.event.player.PostMotionEvent", "()V", s_postMotionEventClass, s_postMotionEventCtor);
        
        initEvent("net.ovson.api.event.input.KeyPressEvent", "(CI)V", s_keyPressEventClass, s_keyPressEventCtor);
        if (s_keyPressEventClass) s_keyPressEventIsCancelled = env->GetMethodID(s_keyPressEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.input.KeyEvent", "(Ljava/lang/String;IZZ)V", s_keyEventClass, s_keyEventCtor);

        initEvent("net.ovson.api.event.input.MouseEvent", "(IZIII)V", s_mouseEventClass, s_mouseEventCtor);
        if (s_mouseEventClass) s_mouseEventIsCancelled = env->GetMethodID(s_mouseEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.GuiOpenEvent", "(Ljava/lang/String;Z)V", s_guiOpenEventClass, s_guiOpenEventCtor);
        if (s_guiOpenEventClass) s_guiOpenEventIsCancelled = env->GetMethodID(s_guiOpenEventClass, "isCancelled", "()Z");

        initEvent("net.ovson.api.event.GuiCloseEvent", "(Ljava/lang/String;)V", s_guiCloseEventClass, s_guiCloseEventCtor);
        initEvent("net.ovson.api.event.DisconnectEvent", "(Ljava/lang/String;)V", s_disconnectEventClass, s_disconnectEventCtor);

        s_active.store(true);
    }

    void shutdown() {
        s_active.store(false);
        Sleep(50);

        JNIEnv* env = lc ? lc->getEnv() : nullptr;
        if (!env) return;

        auto cleanup = [&](jclass& cls) {
            if (cls) {
                env->DeleteGlobalRef(cls);
                cls = nullptr;
            }
        };

        cleanup(s_render2DEventClass);
        cleanup(s_render3DEventClass);
        cleanup(s_tickEventClass);
        cleanup(s_chatReceivedEventClass);
        cleanup(s_chatSendEventClass);
        cleanup(s_playerJoinEventClass);
        cleanup(s_packetSendEventClass);
        cleanup(s_packetReceiveEventClass);
        cleanup(s_attackEventClass);
        cleanup(s_updateEventClass);
        cleanup(s_preMotionEventClass);
        cleanup(s_postMotionEventClass);
        cleanup(s_keyPressEventClass);
        cleanup(s_keyEventClass);
        cleanup(s_mouseEventClass);
        cleanup(s_guiOpenEventClass);
        cleanup(s_guiCloseEventClass);
        cleanup(s_disconnectEventClass);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    void postRender2DEvent(float partialTicks) {
        try {
            if (!s_active.load() || g_cleaningUp || !PluginLoader::hasPlugins()) return;
            if (!s_render2DEventClass || !s_render2DEventCtor) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_render2DEventClass, s_render2DEventCtor, partialTicks);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postRender3DEvent(float partialTicks) {
        try {
            if (!s_active.load() || g_cleaningUp || !PluginLoader::hasPlugins()) return;
            if (!s_render3DEventClass || !s_render3DEventCtor) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_render3DEventClass, s_render3DEventCtor, partialTicks);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postTickEvent() {
        try {
            if (!s_active.load() || g_cleaningUp || !PluginLoader::hasPlugins()) return;
            if (!s_tickEventClass || !s_tickEventCtor) return;
            
            static auto lastTick = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() < 50) return;
            lastTick = now;

            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_tickEventClass, s_tickEventCtor);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postChatReceivedEvent(const std::string& message) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_chatReceivedEventClass || !s_chatReceivedEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jMsg = Lunar::createSafeJString(env, message);
            if (!jMsg) return;
            jobject event = env->NewObject(s_chatReceivedEventClass, s_chatReceivedEventCtor, jMsg);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            env->DeleteLocalRef(jMsg);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    bool postChatSendEvent(const std::string& message) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_chatSendEventClass || !s_chatSendEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jMsg = Lunar::createSafeJString(env, message);
            if (!jMsg) return false;
            jobject event = env->NewObject(s_chatSendEventClass, s_chatSendEventCtor, jMsg);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_chatSendEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_chatSendEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            env->DeleteLocalRef(jMsg);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    void postPlayerJoinEvent(const std::string& playerName) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_playerJoinEventClass || !s_playerJoinEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, playerName);
            if (!jName) return;
            jobject event = env->NewObject(s_playerJoinEventClass, s_playerJoinEventCtor, jName);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    bool postPacketSendEvent(jobject packet, const std::string& packetName) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_packetSendEventClass || !s_packetSendEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, packetName);
            if (!jName) return false;
            jobject event = env->NewObject(s_packetSendEventClass, s_packetSendEventCtor, packet, jName);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_packetSendEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_packetSendEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    bool postPacketReceiveEvent(jobject packet, const std::string& packetName) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_packetReceiveEventClass || !s_packetReceiveEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, packetName);
            if (!jName) return false;
            jobject event = env->NewObject(s_packetReceiveEventClass, s_packetReceiveEventCtor, packet, jName);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_packetReceiveEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_packetReceiveEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    bool postAttackEvent(int targetEntityId) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_attackEventClass || !s_attackEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_attackEventClass, s_attackEventCtor, targetEntityId);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_attackEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_attackEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    void postUpdateEvent(bool pre, float yaw, float pitch, bool onGround) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_updateEventClass || !s_updateEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_updateEventClass, s_updateEventCtor, pre, yaw, pitch, onGround);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postPreMotionEvent(double x, double y, double z, float yaw, float pitch, bool ground, bool sprint, bool sneak) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_preMotionEventClass || !s_preMotionEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_preMotionEventClass, s_preMotionEventCtor, x, y, z, yaw, pitch, ground, sprint, sneak);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postPostMotionEvent() {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_postMotionEventClass || !s_postMotionEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_postMotionEventClass, s_postMotionEventCtor);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    bool postKeyPressEvent(char character, int keyCode) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_keyPressEventClass || !s_keyPressEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_keyPressEventClass, s_keyPressEventCtor, (jchar)character, keyCode);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_keyPressEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_keyPressEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    void postKeyEvent(const std::string& name, int code, bool pressed, bool inGui) {
        try {
            if (!s_active.load() || g_cleaningUp || !PluginLoader::hasPlugins()) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_keyEventClass || !s_keyEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, name);
            if (!jName) return;
            jobject event = env->NewObject(s_keyEventClass, s_keyEventCtor, jName, code, pressed, inGui);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    bool postMouseEvent(int button, bool pressed, int x, int y, int scroll) {
        try {
            if (!s_active.load() || g_cleaningUp || !PluginLoader::hasPlugins()) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_mouseEventClass || !s_mouseEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jobject event = env->NewObject(s_mouseEventClass, s_mouseEventCtor, button, pressed, x, y, scroll);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_mouseEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_mouseEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    bool postGuiOpenEvent(const std::string& screenName, bool opening) {
        try {
            if (!s_active.load() || g_cleaningUp) return false;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_guiOpenEventClass || !s_guiOpenEventCtor) return false;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, screenName);
            if (!jName) return false;
            jobject event = env->NewObject(s_guiOpenEventClass, s_guiOpenEventCtor, jName, opening);
            bool cancelled = false;
            if (event) {
                PluginLoader::postEvent(event);
                if (s_guiOpenEventIsCancelled) cancelled = env->CallBooleanMethod(event, s_guiOpenEventIsCancelled);
                env->DeleteLocalRef(event);
            }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return cancelled;
        } catch (...) {
            return false;
        }
    }

    void postGuiCloseEvent(const std::string& screenName) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_guiCloseEventClass || !s_guiCloseEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jName = Lunar::createSafeJString(env, screenName);
            if (!jName) return;
            jobject event = env->NewObject(s_guiCloseEventClass, s_guiCloseEventCtor, jName);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postDisconnectEvent(const std::string& reason) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_disconnectEventClass || !s_disconnectEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            jstring jReason = Lunar::createSafeJString(env, reason);
            if (!jReason) return;
            jobject event = env->NewObject(s_disconnectEventClass, s_disconnectEventCtor, jReason);
            if (event) { PluginLoader::postEvent(event); env->DeleteLocalRef(event); }
            env->DeleteLocalRef(jReason);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }

    void postAnticheatFlagEvent(const std::string& playerName) {
        try {
            if (!s_active.load() || g_cleaningUp) return;
            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env || !s_anticheatFlagEventClass || !s_anticheatFlagEventCtor) return;
            if (env->ExceptionCheck()) env->ExceptionClear();

            jstring jName = Lunar::createSafeJString(env, playerName);
            if (!jName) return;
            jobject eventObj = env->NewObject(s_anticheatFlagEventClass, s_anticheatFlagEventCtor, jName);
            if (eventObj) {
                PluginLoader::postEvent(eventObj);
                env->DeleteLocalRef(eventObj);
            }
            env->DeleteLocalRef(jName);
            if (env->ExceptionCheck()) env->ExceptionClear();
        } catch (...) {}
    }
}