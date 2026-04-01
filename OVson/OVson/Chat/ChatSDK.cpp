#include "ChatSDK.h"
#include "../Utils/Logger.h"
#include "../Java.h"
#include "../Config/Config.h"
#include <cstdarg>
#include <vector>

static bool callSendChatMessage(const std::string &text)
{
	JNIEnv* env = lc->getEnv();
	if (!env) return false;
	CMinecraft mc;
	CPlayer player = mc.GetLocalPlayer();
	if (!player.Get()) return false;

	jclass playerCls = lc->GetClass("net.minecraft.client.entity.EntityPlayerSP");
    if (!playerCls) {
        player.Cleanup();
        return false;
    }

	jmethodID sendChat = lc->GetMethodID(playerCls, "sendChatMessage", "(Ljava/lang/String;)V", "func_71165_d", "e");
    
	if (!sendChat) {
		player.Cleanup();
		return false;
	}

	jstring jtext = env->NewStringUTF(text.c_str());
	env->CallVoidMethod(player.Get(), sendChat, jtext);
	env->DeleteLocalRef(jtext);
	player.Cleanup();
	return true;
}

static bool callAddChatMessage(const std::string &text)
{
	JNIEnv* env = lc->getEnv();
	if (!env)
		return false;
	jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
	if (!mcCls)
		return false;
	
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) return false;

	jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) return false;

	jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI", "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;"); // 1.8.9 obfuscated GUI class
	
    if (!f_ingame) { env->DeleteLocalRef(mcObj); return false; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    if (!ingame) { env->DeleteLocalRef(mcObj); return false; }

	// get chat gui
	jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }

	jmethodID getChatGUI = lc->GetMethodID(igCls, "getChatGUI", "()Lnet/minecraft/client/gui/GuiNewChat;", "func_146158_b", "d", "()Lavt;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");

	if (!getChatGUI) { env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }

    jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
    if (!chatGui) { env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }

	jclass cctCls = lc->GetClass("net.minecraft.util.ChatComponentText");
	if (!cctCls) { env->DeleteLocalRef(chatGui); env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }
	jmethodID cctCtor = env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
	if (!cctCtor) { if (env->ExceptionCheck()) env->ExceptionClear(); env->DeleteLocalRef(chatGui); env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }
	jstring jtext = env->NewStringUTF(text.c_str());
	jobject component = env->NewObject(cctCls, cctCtor, jtext);

	jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
	if (!gncCls) { env->DeleteLocalRef(component); env->DeleteLocalRef(jtext); env->DeleteLocalRef(chatGui); env->DeleteLocalRef(ingame); env->DeleteLocalRef(mcObj); return false; }
	jmethodID print = lc->GetMethodID(gncCls, "printChatMessage", "(Lnet/minecraft/util/IChatComponent;)V", "func_146227_a", "a", "(Leu;)V");
    if (!print) print = lc->FindMethodBySignature(gncCls, "(Lnet/minecraft/util/IChatComponent;)V");
    if (!print) print = lc->FindMethodBySignature(gncCls, "(Leu;)V");

    if (print) {
	    env->CallVoidMethod(chatGui, print, component);
    }

	env->DeleteLocalRef(jtext);
	env->DeleteLocalRef(component);
	env->DeleteLocalRef(chatGui);
	env->DeleteLocalRef(ingame);
	env->DeleteLocalRef(mcObj);
	return true;
}

bool ChatSDK::sendClientChat(const std::string &text)
{
	return callSendChatMessage(text);
}

bool ChatSDK::showClientMessage(const std::string &text)
{
	return callAddChatMessage(text);
}

std::string ChatSDK::formatPrefix()
{
	const char *S = "\xC2\xA7";
	return std::string(S) + "0[" + S + "r" + S + "cO" + S + "6V" + S + "es" + S + "ao" + S + "bn" + S + "0]" + S + "r ";
}

bool ChatSDK::showPrefixed(const std::string &message)
{
    if (message.find("FAILED") != std::string::npos && 
        message.find("CRITICAL") == std::string::npos && 
        !Config::isGlobalDebugEnabled()) {
        return false;
    }
	return showClientMessage(formatPrefix() + "§f" + message);
}

void ChatSDK::initialize()
{
	Lunar::reporter = ChatSDK::showPrefixed;
}

bool ChatSDK::showPrefixedf(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	return showPrefixed(buf);
}
