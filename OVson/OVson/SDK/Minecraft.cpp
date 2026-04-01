#include "Minecraft.h"

jclass CMinecraft::GetClass() {
	return lc->GetClass("net.minecraft.client.Minecraft");

}

jobject CMinecraft::GetInstance() {
	jclass minecraftClass = this->GetClass();
	JNIEnv* env = lc->getEnv();
	if (!env || !minecraftClass) return nullptr;

	jfieldID getMinecraft = env->GetStaticFieldID(minecraftClass, "theMinecraft", "Lnet/minecraft/client/Minecraft;");
    if (!getMinecraft) { if (env->ExceptionCheck()) env->ExceptionClear(); getMinecraft = env->GetStaticFieldID(minecraftClass, "field_71432_P", "Lnet/minecraft/client/Minecraft;"); }
    if (!getMinecraft) { if (env->ExceptionCheck()) env->ExceptionClear(); getMinecraft = env->GetStaticFieldID(minecraftClass, "S", "Lave;"); }
    
    if (!getMinecraft) { if (env->ExceptionCheck()) env->ExceptionClear(); return nullptr; }

	jobject rtrn = env->GetStaticObjectField(minecraftClass, getMinecraft);

	return rtrn;
}

CPlayer CMinecraft::GetLocalPlayer() {
	jclass minecraftClass = this->GetClass();
	jobject minecraftObject = this->GetInstance();
	JNIEnv* env = lc->getEnv();
	if (!env || !minecraftClass || !minecraftObject) {
        if (minecraftObject) env->DeleteLocalRef(minecraftObject);
        return CPlayer(nullptr);
    }

	jfieldID getPlayer = env->GetFieldID(minecraftClass, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;");
    if (!getPlayer) { if (env->ExceptionCheck()) env->ExceptionClear(); getPlayer = env->GetFieldID(minecraftClass, "field_71439_g", "Lnet/minecraft/client/entity/EntityPlayerSP;"); }
    if (!getPlayer) { if (env->ExceptionCheck()) env->ExceptionClear(); getPlayer = env->GetFieldID(minecraftClass, "h", "Lbew;"); }

	jobject rtrn = nullptr;
    if (getPlayer) {
        rtrn = env->GetObjectField(minecraftObject, getPlayer);
    }

	env->DeleteLocalRef(minecraftObject);
	return CPlayer(rtrn);
}