#include "ChatHook.h"
#include "Commands.h"
#include "../Utils/Logger.h"
#include "../Java.h"
#include "../Plugins/PluginLoader.h"
#include "../Config/Config.h"
#include "ChatSDK.h"

// who-related capture removed; no interceptor linkage needed here

static jmethodID g_sendChatMessage = nullptr;

static void JNICALL onMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jmethodID method)
{
	if (method != g_sendChatMessage)
		return;
}

// removed onMethodEntryChat

bool ChatHook::install()
{
	JNIEnv* env = lc->getEnv();
	if (!lc->jvmti || !env)
		return false;
	Logger::info("Installing chat hook");

	jclass playerCls = lc->GetClass("net.minecraft.client.entity.EntityPlayerSP");
	if (!playerCls){
		Logger::error("EntityPlayerSP not found");
		return false;
	}
	g_sendChatMessage = lc->GetMethodID(playerCls, "sendChatMessage", "(Ljava/lang/String;)V", "func_71165_d", "e");
	if (!g_sendChatMessage){
		Logger::error("sendChatMessage method not found");
		return false;
	}

	jvmtiCapabilities caps{};
	caps.can_generate_method_entry_events = 1;
	jvmtiError capRes = lc->jvmti->AddCapabilities(&caps);
	if (capRes != JVMTI_ERROR_NONE){
		Logger::error("JVMTI AddCapabilities failed: %d", (int)capRes);
	}
	jvmtiEventCallbacks cbs{};

	jvmtiError cbRes = lc->jvmti->SetEventCallbacks(&cbs, sizeof(cbs));
	if (cbRes != JVMTI_ERROR_NONE){
		Logger::error("JVMTI SetEventCallbacks failed: %d", (int)cbRes);
		return false;
	}

	return true;
}

void ChatHook::uninstall()
{
	if (!lc->jvmti)
		return;
	lc->jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, nullptr);
	jvmtiEventCallbacks cbs{};
	lc->jvmti->SetEventCallbacks(&cbs, sizeof(cbs));
}

bool ChatHook::onClientSendMessage(const std::string &message)
{
	if (CommandRegistry::instance().tryDispatch(message))
	{
		// command handled; block original send
		return true;
	}

    JNIEnv* env = lc->getEnv();
    if (env) {
        jclass eventCls = PluginLoader::loadAPIClass(env, "net.ovson.api.event.ChatSendEvent");
        if (eventCls) {
            jmethodID ctor = env->GetMethodID(eventCls, "<init>", "(Ljava/lang/String;)V");
            if (ctor) {
                jstring jmsg = env->NewStringUTF(message.c_str());
                jobject eventObj = env->NewObject(eventCls, ctor, jmsg);
                if (eventObj) {
                    PluginLoader::postEvent(eventObj);
                    
                    jmethodID isCancelled = env->GetMethodID(eventCls, "isCancelled", "()Z");
                    if (isCancelled && env->CallBooleanMethod(eventObj, isCancelled)) {
                        env->DeleteLocalRef(eventObj);
                        env->DeleteLocalRef(jmsg);
                        env->DeleteLocalRef(eventCls);
                        return true;
                    }
                    env->DeleteLocalRef(eventObj);
                }
                env->DeleteLocalRef(jmsg);
            }
            env->DeleteLocalRef(eventCls);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    const std::string& prefix = Config::getCommandPrefix();
    if (!message.empty() && message.substr(0, prefix.length()) == prefix) {
        if (!(message.length() >= prefix.length() * 2 && message.substr(prefix.length(), prefix.length()) == prefix)) {
            ChatSDK::showPrefixed("§cUnknown command: §f" + message);
            return true;
        }
    }

	return false;
}
