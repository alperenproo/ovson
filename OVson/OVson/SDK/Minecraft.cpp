#include "Minecraft.h"
#include "McAccess.h"

jclass CMinecraft::GetClass() {
	return Mc::minecraftClass();
}

jobject CMinecraft::GetInstance() {
	return Mc::theMinecraft(lc ? lc->getEnv() : nullptr);
}

CPlayer CMinecraft::GetLocalPlayer() {
	return CPlayer(Mc::thePlayer(lc ? lc->getEnv() : nullptr));
}
