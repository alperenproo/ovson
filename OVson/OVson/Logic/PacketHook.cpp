#include "PacketHook.h"

#include <set>
#include <string>
#include "BlockHitSound.h"
#include "PacketFilterHook_bytes.h"
#include "../Java.h"
#include "../Utils/Logger.h"
#include "../Chat/ChatHook.h"
#include "../Plugins/PluginLoader.h"

#include <atomic>
#include <string>

namespace {

std::atomic<bool> g_acceptNativeCallbacks{false};
std::atomic<unsigned int> g_activeNativeCallbacks{0};

class NativeCallbackGuard {
public:
    NativeCallbackGuard() {
        if (!g_acceptNativeCallbacks.load(std::memory_order_acquire)) return;
        g_activeNativeCallbacks.fetch_add(1, std::memory_order_acq_rel);
        counted_ = true;
        if (g_acceptNativeCallbacks.load(std::memory_order_acquire)) {
            active_ = true;
            return;
        }
        release();
    }

    ~NativeCallbackGuard() { release(); }
    explicit operator bool() const { return active_; }

private:
    void release() {
        if (!active_ && !counted_) return;
        active_ = false;
        counted_ = false;
        g_activeNativeCallbacks.fetch_sub(1, std::memory_order_acq_rel);
        g_activeNativeCallbacks.notify_all();
    }

    bool active_ = false;
    bool counted_ = false;
};

void disableAndDrainNativeCallbacks() {
    g_acceptNativeCallbacks.store(false, std::memory_order_release);
    unsigned int active = g_activeNativeCallbacks.load(std::memory_order_acquire);
    while (active != 0) {
        g_activeNativeCallbacks.wait(active, std::memory_order_acquire);
        active = g_activeNativeCallbacks.load(std::memory_order_acquire);
    }
}

jstring unchangedChatResult(JNIEnv *env, jstring rawJson) noexcept {
    return rawJson ? static_cast<jstring>(env->NewLocalRef(rawJson))
                   : env->NewStringUTF("");
}

void copyJavaString(JNIEnv *env, jstring source, std::string &destination) {
    if (!source) return;
    const char *characters = env->GetStringUTFChars(source, nullptr);
    if (!characters) return;
    try {
        destination.assign(characters);
    } catch (...) {
        env->ReleaseStringUTFChars(source, characters);
        throw;
    }
    env->ReleaseStringUTFChars(source, characters);
}

} // namespace

extern "C" JNIEXPORT void JNICALL Java_net_ovson_api_hook_PacketFilterHook_logDebug
  (JNIEnv *env, jclass, jstring jmsg)
{
    NativeCallbackGuard callback;
    if (!callback) return;
    if (jmsg) {
        const char* msg = env->GetStringUTFChars(jmsg, nullptr);
        if (msg) {
            try {
                Logger::info("%s", msg);
            } catch (...) {
            }
            env->ReleaseStringUTFChars(jmsg, msg);
        }
    }
}

extern "C" JNIEXPORT jstring JNICALL Java_net_ovson_api_hook_PacketFilterHook_processIncomingChat
  (JNIEnv *env, jclass, jstring jUnformatted, jstring jRawJson)
{
    NativeCallbackGuard callback;
    if (!callback) {
        return unchangedChatResult(env, jRawJson);
    }
    try {
        std::string unformatted;
        std::string rawJson;
        copyJavaString(env, jUnformatted, unformatted);
        copyJavaString(env, jRawJson, rawJson);
        const std::string result =
            ChatHook::processIncomingChat(unformatted, rawJson);
        return env->NewStringUTF(result.c_str());
    } catch (...) {
        return unchangedChatResult(env, jRawJson);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_net_ovson_api_hook_PacketFilterHook_onServerPacket(
    JNIEnv *, jclass, jint kind, jint entityId, jint data1, jint data2,
    jint data3, jfloat value1, jfloat value2, jfloat value3) {
    NativeCallbackGuard callback;
    if (!callback) return;
    // This callback runs on Netty's inbound channel thread. Keep it bounded and
    // JNI-free: the render thread performs all game-state inspection later.
    try {
        BlockHitSound::enqueueServerSignal(
            static_cast<BlockHitSound::ServerSignalKind>(kind), entityId,
            data1, data2, data3, value1, value2, value3, GetTickCount64());
    } catch (...) {
        // Never unwind through a JNI/native boundary.
    }
}

static bool s_injected = false;
static jclass s_hookCls = nullptr;
static jobject s_hookObj = nullptr;
// The Netty channel we actually installed the handler on. Hypixel moves you
// between backends and the client builds a NEW NetworkManager and a NEW
// channel when it does; the handler stays behind on the dead one. Keeping a
// global ref lets us notice that and re-install instead of going quiet.
static jobject s_hookedChannel = nullptr;
static ULONGLONG s_nextAttempt = 0;
static bool s_addFailureLogged = false;

// Discovery walks a chain of client-specific lookups and every failure along
// it is a silent return, which is fine while it eventually succeeds and
// useless when it never does: the feature is simply absent with nothing in
// the log to say why. Each stage reports itself once, so a client that
// remaps one link in the chain names that link instead of leaving the whole
// feature looking dead.
static void reportStall(const char *stage) {
    static std::set<std::string> reported;
    if (reported.insert(stage).second)
        Logger::error("[PacketHook] discovery stalled at: %s", stage);
}

void PacketHook::update() {
    JNIEnv* env = lc->getEnv();
    if (!env) return;

    // Run the correlator every render update, BEFORE the s_injected early-out
    // below. Packet-hook discovery is a one-shot job that stops once the hook
    // is installed, but the correlator has to keep ticking for the entire
    // session: it is what turns the queued server signals into sounds, and the
    // signals only start arriving after injection succeeds. Returning early
    // here would silence block-hit sound permanently the moment the hook went
    // in. Discovery itself is still throttled further down; the correlator is
    // deliberately not, because a 2-second tick would lose the short
    // swing/hurt/health ordering windows.
    BlockHitSound::update(env);

    // Being injected is not a terminal state. It used to be -- this was a
    // bare `if (s_injected) return;` -- and that is why block-hit sound went
    // silent the moment the connection was replaced: the handler sat on a
    // channel nobody was reading from any more, every counter stayed at
    // zero, and nothing in the log said why. Keep looking, just cheaply:
    // once a second costs far less than a frame and is far more often than
    // a player changes servers.
    if (s_injected) {
        static ULONGLONG nextChannelCheck = 0;
        const ULONGLONG checkNow = GetTickCount64();
        if (checkNow < nextChannelCheck) return;
        nextChannelCheck = checkNow + 1000;
    }

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) { reportStall("Minecraft class"); return; }

    jmethodID getMcMethod = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A", "()Lave;");
    if (!getMcMethod) { reportStall("Minecraft.getMinecraft()"); return; }

    jobject mcObj = env->CallStaticObjectMethod(mcCls, getMcMethod);
    if (!mcObj) { reportStall("Minecraft instance"); return; }

    static ULONGLONG lastAttempt = 0;
    ULONGLONG now = GetTickCount64();
    if (now < s_nextAttempt || now - lastAttempt < 2000) {
        env->DeleteLocalRef(mcObj);
        return;
    }
    lastAttempt = now;

    static jfieldID theWorldField = nullptr;
    static jmethodID getNetHandler = nullptr;
    static jmethodID getNetworkManager = nullptr;
    static jfieldID nmField = nullptr;
    static jfieldID channelField = nullptr;

    if (!theWorldField) {
        theWorldField = lc->GetFieldID(mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;", "field_71441_e", "f", "Lbdb;");
        if (!theWorldField)
            theWorldField = lc->FindFieldBySignature(mcCls, "Lbdb;");
        if (!theWorldField)
            reportStall("Minecraft.theWorld field");
    }
    jobject theWorld = theWorldField ? env->GetObjectField(mcObj, theWorldField) : nullptr;
    if (env->ExceptionCheck()) env->ExceptionClear();

    if (!theWorld) {
        env->DeleteLocalRef(mcObj);
        return;
    }
    env->DeleteLocalRef(theWorld);

    if (!getNetHandler) {
        getNetHandler = lc->GetMethodID(mcCls, "getNetHandler", "()Lnet/minecraft/client/network/NetHandlerPlayClient;", "func_147114_u", "ay", "()Lbcy;");
        if (!getNetHandler) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getNetHandler = lc->FindMethodBySignature(mcCls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
        }
        if (!getNetHandler) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getNetHandler = lc->FindMethodBySignature(mcCls, "()Lbcy;");
        }
    }
    
    if (!getNetHandler) {
        reportStall("Minecraft.getNetHandler()");
        env->DeleteLocalRef(mcObj);
        return;
    }

    jobject nh = env->CallObjectMethod(mcObj, getNetHandler);
    if (!nh) {
        reportStall("NetHandlerPlayClient instance");
        env->DeleteLocalRef(mcObj);
        return;
    }

    jobject networkManager = nullptr;
    jclass nhCls = env->GetObjectClass(nh);
    
    if (!getNetworkManager && !nmField) {
        getNetworkManager = lc->FindMethodBySignature(nhCls, "()Lnet/minecraft/network/NetworkManager;");
        if (!getNetworkManager) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            getNetworkManager = lc->FindMethodBySignature(nhCls, "()Lej;");
        }
        if (!getNetworkManager) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            nmField = lc->FindFieldBySignature(nhCls, "Lnet/minecraft/network/NetworkManager;");
            if (!nmField) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                nmField = lc->FindFieldBySignature(nhCls, "Lej;");
            }
        }

        // Everything above searches for NetworkManager by name -- either the
        // deobfuscated one or the single obfuscated name this Minecraft
        // version happens to use. Both are guesses about how a particular
        // client shipped the jar, and a client that remaps it to anything
        // else leaves the whole packet hook dead with no way to tell why.
        //
        // So identify it by what it demonstrably IS instead: the object held
        // by NetHandlerPlayClient that owns an io.netty.channel.Channel.
        // Netty is a third-party library, so its type name survives any
        // Minecraft remapping, which makes it the one stable anchor in this
        // chain. That is also exactly the field the code below goes on to
        // read, so anything this finds is by definition the right object.
        if (!getNetworkManager && !nmField && lc->jvmti) {
            // Walk the hierarchy: GetClassFields only reports fields declared
            // by the class itself, and a client that subclasses
            // NetHandlerPlayClient would otherwise hide the one field we want.
            jclass walk = nhCls;
            bool ownsWalkRef = false;
            while (walk && !nmField) {
                jint fieldCount = 0;
                jfieldID *fields = nullptr;
                if (lc->jvmti->GetClassFields(walk, &fieldCount, &fields) ==
                    JVMTI_ERROR_NONE) {
                    for (jint i = 0; i < fieldCount && !nmField; ++i) {
                        char *fieldName = nullptr;
                        char *fieldSig = nullptr;
                        if (lc->jvmti->GetFieldName(walk, fields[i], &fieldName,
                                                    &fieldSig, nullptr) !=
                            JVMTI_ERROR_NONE) {
                            continue;
                        }
                        // GetClassFields returns statics too, and reading one
                        // with GetObjectField (instead of
                        // GetStaticObjectField) is undefined -- on HotSpot it
                        // dereferences garbage and takes the whole poll down
                        // with an access violation. NetworkManager is an
                        // instance field, so skip statics outright.
                        jint modifiers = 0;
                        constexpr jint kAccStatic = 0x0008;
                        const bool isStatic =
                            lc->jvmti->GetFieldModifiers(walk, fields[i],
                                                         &modifiers) !=
                                JVMTI_ERROR_NONE ||
                            (modifiers & kAccStatic) != 0;

                        // Object-typed fields only; a primitive or an array
                        // cannot own a Channel.
                        if (!isStatic && fieldSig && fieldSig[0] == 'L') {
                            jobject candidate =
                                env->GetObjectField(nh, fields[i]);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (candidate) {
                                jclass candidateCls =
                                    env->GetObjectClass(candidate);
                                if (candidateCls &&
                                    lc->FindFieldBySignature(
                                        candidateCls,
                                        "Lio/netty/channel/Channel;")) {
                                    nmField = fields[i];
                                    Logger::info(
                                        "[PacketHook] NetworkManager found "
                                        "structurally: field '%s' of type %s",
                                        fieldName ? fieldName : "?",
                                        fieldSig ? fieldSig : "?");
                                }
                                if (candidateCls)
                                    env->DeleteLocalRef(candidateCls);
                                env->DeleteLocalRef(candidate);
                            }
                            if (env->ExceptionCheck()) env->ExceptionClear();
                        }
                        if (fieldName)
                            lc->jvmti->Deallocate((unsigned char *)fieldName);
                        if (fieldSig)
                            lc->jvmti->Deallocate((unsigned char *)fieldSig);
                    }
                    if (fields)
                        lc->jvmti->Deallocate((unsigned char *)fields);
                }
                if (nmField)
                    break;
                jclass parent = env->GetSuperclass(walk);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (ownsWalkRef)
                    env->DeleteLocalRef(walk);
                walk = parent;
                ownsWalkRef = true;
            }
            if (ownsWalkRef && walk)
                env->DeleteLocalRef(walk);
            if (!nmField)
                reportStall("NetworkManager field (structural scan)");
        }
    }
    
    if (getNetworkManager) {
        networkManager = env->CallObjectMethod(nh, getNetworkManager);
    } else if (nmField) {
        networkManager = env->GetObjectField(nh, nmField);
    }

    if (!networkManager) {
        reportStall("NetworkManager on NetHandlerPlayClient");
        env->DeleteLocalRef(nhCls);
        env->DeleteLocalRef(nh);
        env->DeleteLocalRef(mcObj);
        return;
    }

    jclass nmCls = env->GetObjectClass(networkManager);
    if (!channelField) {
        channelField = lc->FindFieldBySignature(nmCls, "Lio/netty/channel/Channel;");
    }
    
    if (!channelField) {
        reportStall("io.netty.channel.Channel field on NetworkManager");
        env->DeleteLocalRef(nmCls);
        env->DeleteLocalRef(nhCls);
        env->DeleteLocalRef(networkManager);
        env->DeleteLocalRef(nh);
        env->DeleteLocalRef(mcObj);
        return;
    }

    jobject channel = env->GetObjectField(networkManager, channelField);
    if (!channel) {
        Logger::error("[PacketHook] Failed: Channel field on NetworkManager is null");
        env->DeleteLocalRef(nmCls);
        env->DeleteLocalRef(networkManager);
        env->DeleteLocalRef(nh);
        env->DeleteLocalRef(mcObj);
        return;
    }

    // Still the same connection we hooked? Then there is nothing to do. A
    // different channel object means the old one is gone and our handler
    // went with it.
    if (s_injected) {
        const bool sameChannel =
            s_hookedChannel && env->IsSameObject(s_hookedChannel, channel);
        if (sameChannel) {
            env->DeleteLocalRef(channel);
            env->DeleteLocalRef(nmCls);
            env->DeleteLocalRef(nhCls);
            env->DeleteLocalRef(networkManager);
            env->DeleteLocalRef(nh);
            env->DeleteLocalRef(mcObj);
            return;
        }
        Logger::info("[PacketHook] Connection replaced; re-injecting "
                     "PacketFilterHook into the new Netty pipeline");
        if (s_hookedChannel) {
            env->DeleteGlobalRef(s_hookedChannel);
            s_hookedChannel = nullptr;
        }
        s_injected = false;
        s_addFailureLogged = false;
        s_nextAttempt = 0;
    }

    if (!s_hookCls) {
        jmethodID getClassLoader = env->GetMethodID(env->FindClass("java/lang/Class"), "getClassLoader", "()Ljava/lang/ClassLoader;");
        jclass channelCls = env->GetObjectClass(channel);
        jobject channelLoader = env->CallObjectMethod(channelCls, getClassLoader);
        jobject nmLoader = env->CallObjectMethod(nmCls, getClassLoader);
        env->DeleteLocalRef(channelCls);

        Logger::info("[PacketHook] channelLoader=%p, nmLoader=%p", channelLoader, nmLoader);

        jobject loaders[] = { channelLoader, nmLoader, nullptr };
        const char* loaderNames[] = { "channelLoader (Netty)", "nmLoader (NetworkManager)", "null (bootstrap)" };
        jclass hookClsLocal = nullptr;

        for (int i = 0; i < 3 && !hookClsLocal; i++) {
            hookClsLocal = env->DefineClass("net/ovson/api/hook/PacketFilterHook", loaders[i], (const jbyte*)PacketFilterHook_class, PacketFilterHook_class_len);
            if (env->ExceptionCheck()) {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                std::string exStr = "Unknown";
                if (ex) {
                    jmethodID toStr = env->GetMethodID(env->GetObjectClass(ex), "toString", "()Ljava/lang/String;");
                    if (toStr) {
                        jstring js = (jstring)env->CallObjectMethod(ex, toStr);
                        if (js) {
                            const char* str = env->GetStringUTFChars(js, nullptr);
                            if (str) { exStr = str; env->ReleaseStringUTFChars(js, str); }
                            env->DeleteLocalRef(js);
                        }
                    }
                    env->DeleteLocalRef(ex);
                }
                Logger::error("[PacketHook] DefineClass with %s failed: %s", loaderNames[i], exStr.c_str());
                hookClsLocal = nullptr;
            } else if (hookClsLocal) {
                Logger::info("[PacketHook] DefineClass succeeded with %s!", loaderNames[i]);
            }
        }

        if (!hookClsLocal) {
            jclass clsCls = env->FindClass("java/lang/Class");
            jmethodID m_forName = clsCls ? env->GetStaticMethodID(clsCls, "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;") : nullptr;
            jobject tryLoaders[] = { channelLoader, nmLoader };
            for (int i = 0; i < 2 && !hookClsLocal; i++) {
                if (m_forName && tryLoaders[i]) {
                    jstring className = env->NewStringUTF("net.ovson.api.hook.PacketFilterHook");
                    hookClsLocal = (jclass)env->CallStaticObjectMethod(clsCls, m_forName, className, JNI_TRUE, tryLoaders[i]);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                        hookClsLocal = nullptr;
                    }
                    env->DeleteLocalRef(className);
                }
            }
            if (hookClsLocal) {
                Logger::info("[PacketHook] Class.forName succeeded (re-inject scenario)");
            } else {
                Logger::error("[PacketHook] All DefineClass and Class.forName attempts failed!");
            }
            if (clsCls) env->DeleteLocalRef(clsCls);
        }

        if (channelLoader) env->DeleteLocalRef(channelLoader);
        if (nmLoader) env->DeleteLocalRef(nmLoader);

        if (hookClsLocal) {
            s_hookCls = (jclass)env->NewGlobalRef(hookClsLocal);
            env->DeleteLocalRef(hookClsLocal);

            if (!s_hookCls) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                Logger::error("[PacketHook] Failed to retain PacketFilterHook class");
                BlockHitSound::setCallbackAcceptance(false);
                g_acceptNativeCallbacks.store(false, std::memory_order_release);
            } else {
                JNINativeMethod methods[] = {
                    {(char*)"processIncomingChat", (char*)"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*)&Java_net_ovson_api_hook_PacketFilterHook_processIncomingChat},
                    {(char*)"logDebug", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_hook_PacketFilterHook_logDebug},
                    {(char*)"onServerPacket", (char*)"(IIIIIFFF)V", (void*)&Java_net_ovson_api_hook_PacketFilterHook_onServerPacket}
                };
                const jint nativeResult = env->RegisterNatives(s_hookCls, methods, 3);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (nativeResult != JNI_OK) {
                    Logger::error("[PacketHook] Failed to register packet natives (%d); "
                                  "restart Minecraft if an older hook class is already loaded",
                                  nativeResult);
                    BlockHitSound::setCallbackAcceptance(false);
                    g_acceptNativeCallbacks.store(false, std::memory_order_release);
                    env->DeleteGlobalRef(s_hookCls);
                    s_hookCls = nullptr;
                } else {
                    g_acceptNativeCallbacks.store(true, std::memory_order_release);
                    BlockHitSound::setCallbackAcceptance(true);
                }
            }
        } else {
            Logger::error("[PacketHook] Failed to define or find PacketFilterHook class");
        }
    }

    env->DeleteLocalRef(nmCls);

    if (s_hookCls) {
        if (!s_hookObj) {
            jmethodID init = env->GetMethodID(s_hookCls, "<init>", "()V");
            jobject hookObjLocal = init ? env->NewObject(s_hookCls, init) : nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (hookObjLocal) {
                s_hookObj = env->NewGlobalRef(hookObjLocal);
                env->DeleteLocalRef(hookObjLocal);
            }
        }

        jmethodID pipelineMethod = s_hookObj ? env->GetMethodID(env->GetObjectClass(channel), "pipeline", "()Lio/netty/channel/ChannelPipeline;") : nullptr;
        if (pipelineMethod) {
            jobject pipeline = env->CallObjectMethod(channel, pipelineMethod);
            if (pipeline) {
                jmethodID addBefore = env->GetMethodID(env->GetObjectClass(pipeline), "addBefore", "(Ljava/lang/String;Ljava/lang/String;Lio/netty/channel/ChannelHandler;)Lio/netty/channel/ChannelPipeline;");
                jmethodID getHandler = env->GetMethodID(env->GetObjectClass(pipeline), "get", "(Ljava/lang/String;)Lio/netty/channel/ChannelHandler;");
                
                if (addBefore && getHandler) {
                    jstring baseName = env->NewStringUTF("packet_handler");
                    jstring hookName = env->NewStringUTF("ovson_packet_filter");
                    
                    jobject existing = env->CallObjectMethod(pipeline, getHandler, hookName);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    
                    if (existing) {
                        s_injected = true;
                        if (!s_hookedChannel)
                            s_hookedChannel = env->NewGlobalRef(channel);
                        env->DeleteLocalRef(existing);
                    } else {
                        env->CallObjectMethod(pipeline, addBefore, baseName, hookName, s_hookObj);
                        
                        if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                            if (!s_addFailureLogged)
                                Logger::error("[PacketHook] addBefore failed; retrying in 30 seconds");
                            s_addFailureLogged = true;
                            s_nextAttempt = now + 30000;
                            s_injected = false;
                        } else {
                            Logger::info("[PacketHook] Successfully injected PacketFilterHook into Netty pipeline!");
                            s_injected = true;
                            if (s_hookedChannel)
                                env->DeleteGlobalRef(s_hookedChannel);
                            s_hookedChannel = env->NewGlobalRef(channel);
                            s_addFailureLogged = false;
                            s_nextAttempt = 0;
                        }
                    }
                    env->DeleteLocalRef(baseName);
                    env->DeleteLocalRef(hookName);
                } else {
                    if (!s_addFailureLogged)
                        Logger::error("[PacketHook] Could not resolve pipeline methods; retrying in 30 seconds");
                    s_addFailureLogged = true;
                    s_nextAttempt = now + 30000;
                }
                env->DeleteLocalRef(pipeline);
            }
        }
    }

    env->DeleteLocalRef(channel);
    env->DeleteLocalRef(nhCls);
    env->DeleteLocalRef(networkManager);
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
}

void PacketHook::uninstall() {
    g_acceptNativeCallbacks.store(false, std::memory_order_release);
    BlockHitSound::setCallbackAcceptance(false);
    JNIEnv* env = (lc ? lc->getEnv() : nullptr);
    BlockHitSound::shutdown(env);
    if (!lc || !lc->getEnv()) {
        disableAndDrainNativeCallbacks();
        return;
    }
    if (!env) return;

    const auto finalizeNativeState = [env]() {
        if (s_hookCls) {
            env->UnregisterNatives(s_hookCls);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        disableAndDrainNativeCallbacks();
        if (s_hookObj) {
            env->DeleteGlobalRef(s_hookObj);
            s_hookObj = nullptr;
        }
        if (s_hookCls) {
            env->DeleteGlobalRef(s_hookCls);
            s_hookCls = nullptr;
        }
        if (s_hookedChannel) {
            env->DeleteGlobalRef(s_hookedChannel);
            s_hookedChannel = nullptr;
        }
        s_injected = false;
    };

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) {
        finalizeNativeState();
        return;
    }

    jmethodID getMc = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A", "()Lave;");
    if (!getMc) {
        env->DeleteLocalRef(mcCls);
        finalizeNativeState();
        return;
    }

    jobject mcObj = env->CallStaticObjectMethod(mcCls, getMc);
    if (!mcObj) {
        env->DeleteLocalRef(mcCls);
        finalizeNativeState();
        return;
    }

    jmethodID getNet = lc->GetMethodID(mcCls, "getNetHandler", "()Lnet/minecraft/client/network/NetHandlerPlayClient;", "func_147114_u", "ay", "()Lbcy;");
    if (!getNet) {
        env->DeleteLocalRef(mcObj);
        env->DeleteLocalRef(mcCls);
        finalizeNativeState();
        return;
    }

    jobject nh = env->CallObjectMethod(mcObj, getNet);
    if (!nh) {
        env->DeleteLocalRef(mcObj);
        env->DeleteLocalRef(mcCls);
        finalizeNativeState();
        return;
    }

    jobject networkManager = nullptr;
    jclass nhCls = env->GetObjectClass(nh);
    jfieldID nmField = lc->FindFieldBySignature(nhCls, "Lnet/minecraft/network/NetworkManager;");
    if (!nmField) nmField = lc->FindFieldBySignature(nhCls, "Lej;");
    if (nmField) networkManager = env->GetObjectField(nh, nmField);

    if (networkManager) {
        jclass nmCls = env->GetObjectClass(networkManager);
        jfieldID channelField = lc->FindFieldBySignature(nmCls, "Lio/netty/channel/Channel;");
        if (channelField) {
            jobject channel = env->GetObjectField(networkManager, channelField);
            if (channel) {
                jmethodID pipelineMethod = env->GetMethodID(env->GetObjectClass(channel), "pipeline", "()Lio/netty/channel/ChannelPipeline;");
                if (pipelineMethod) {
                    jobject pipeline = env->CallObjectMethod(channel, pipelineMethod);
                    if (pipeline) {
                        jmethodID removeMethod = env->GetMethodID(env->GetObjectClass(pipeline), "remove", "(Ljava/lang/String;)Lio/netty/channel/ChannelHandler;");
                        if (removeMethod) {
                            jstring hookName = env->NewStringUTF("ovson_packet_filter");
                            env->CallObjectMethod(pipeline, removeMethod, hookName);
                            if (env->ExceptionCheck()) {
                                env->ExceptionClear();
                            } else {
                                Logger::info("[PacketHook] Successfully removed PacketFilterHook from pipeline.");
                            }
                            env->DeleteLocalRef(hookName);
                        }
                        env->DeleteLocalRef(pipeline);
                    }
                }
                env->DeleteLocalRef(channel);
            }
        }
        env->DeleteLocalRef(nmCls);
        env->DeleteLocalRef(networkManager);
    }

    env->DeleteLocalRef(nhCls);
    env->DeleteLocalRef(nh);
    env->DeleteLocalRef(mcObj);
    env->DeleteLocalRef(mcCls);

    // finalizeNativeState() also unregisters the natives and drains any
    // in-flight native callbacks before dropping the global refs, which
    // the plain "just delete the refs" version here used to skip.
    finalizeNativeState();
    s_addFailureLogged = false;
    s_nextAttempt = 0;
}

