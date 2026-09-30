#include "InventoryAPI_Bridge.h"
#include "../Java.h"

namespace InventoryAPIBridge {

    jint JNICALL getCurrentSlot(JNIEnv* env, jclass clazz) {
        if (!lc || !lc->getEnv()) return 0;
        
        jclass minecraftClass = env->FindClass("net/minecraft/client/Minecraft");
        if (!minecraftClass) return 0;
        
        jmethodID getMinecraft = env->GetStaticMethodID(minecraftClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
        if (!getMinecraft) return 0;
        
        jobject mc = env->CallStaticObjectMethod(minecraftClass, getMinecraft);
        if (!mc) return 0;
        
        jfieldID thePlayerField = env->GetFieldID(minecraftClass, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;");
        if (!thePlayerField) return 0;
        
        jobject thePlayer = env->GetObjectField(mc, thePlayerField);
        if (!thePlayer) return 0;
        
        jclass playerClass = env->GetObjectClass(thePlayer);
        jfieldID inventoryField = env->GetFieldID(playerClass, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;");
        if (!inventoryField) return 0;
        
        jobject inventory = env->GetObjectField(thePlayer, inventoryField);
        if (!inventory) return 0;
        
        jclass inventoryClass = env->GetObjectClass(inventory);
        jfieldID currentItemField = env->GetFieldID(inventoryClass, "currentItem", "I");
        if (!currentItemField) return 0;
        
        return env->GetIntField(inventory, currentItemField);
    }

    void JNICALL setCurrentSlot(JNIEnv* env, jclass clazz, jint slot) {
        if (!lc || !lc->getEnv()) return;
        
        jclass minecraftClass = env->FindClass("net/minecraft/client/Minecraft");
        if (!minecraftClass) return;
        
        jmethodID getMinecraft = env->GetStaticMethodID(minecraftClass, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
        if (!getMinecraft) return;
        
        jobject mc = env->CallStaticObjectMethod(minecraftClass, getMinecraft);
        if (!mc) return;
        
        jfieldID thePlayerField = env->GetFieldID(minecraftClass, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;");
        if (!thePlayerField) return;
        
        jobject thePlayer = env->GetObjectField(mc, thePlayerField);
        if (!thePlayer) return;
        
        jclass playerClass = env->GetObjectClass(thePlayer);
        jfieldID inventoryField = env->GetFieldID(playerClass, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;");
        if (!inventoryField) return;
        
        jobject inventory = env->GetObjectField(thePlayer, inventoryField);
        if (!inventory) return;
        
        jclass inventoryClass = env->GetObjectClass(inventory);
        jfieldID currentItemField = env->GetFieldID(inventoryClass, "currentItem", "I");
        if (!currentItemField) return;
        
        env->SetIntField(inventory, currentItemField, slot);
    }

    void JNICALL click(JNIEnv* env, jclass clazz, jint windowId, jint slot, jint button, jint mode) {}

    jobject JNICALL getStackInSlot(JNIEnv* env, jclass clazz, jint slot) {
        return nullptr;
    }

    jint JNICALL getSize(JNIEnv* env, jclass clazz) {
        return 36;
    }

    jobject JNICALL getHotbarItem(JNIEnv* env, jclass clazz, jint slot) {
        return nullptr;
    }

    jint JNICALL findItemInHotbar(JNIEnv* env, jclass clazz, jstring name) {
        return -1;
    }

    jobject JNICALL getHelmet(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jobject JNICALL getChestplate(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jobject JNICALL getLeggings(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jobject JNICALL getBoots(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jint JNICALL getChestSize(JNIEnv* env, jclass clazz) {
        return 0;
    }

    jobject JNICALL getStackInChestSlot(JNIEnv* env, jclass clazz, jint slot) {
        return nullptr;
    }

    jstring JNICALL getContainerTitle(JNIEnv* env, jclass clazz) {
        return env->NewStringUTF("");
    }

    jboolean JNICALL isContainerOpen(JNIEnv* env, jclass clazz) {
        return JNI_FALSE;
    }

    jobject JNICALL getCraftResult(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jobject JNICALL getStackInCraftingSlot(JNIEnv* env, jclass clazz, jint slot) {
        return nullptr;
    }

    jint JNICALL countItem(JNIEnv* env, jclass clazz, jstring name) {
        return 0;
    }

    jint JNICALL findItem(JNIEnv* env, jclass clazz, jstring name) {
        return -1;
    }

    void JNICALL dropItem(JNIEnv* env, jclass clazz, jint slot) {}

    void JNICALL dropAllItems(JNIEnv* env, jclass clazz, jstring name) {}

    void JNICALL swapSlots(JNIEnv* env, jclass clazz, jint slot1, jint slot2) {}

    void JNICALL openInventory(JNIEnv* env, jclass clazz) {}

    void JNICALL closeScreen(JNIEnv* env, jclass clazz) {}

            static const JNINativeMethod methods[] = {
        {(char*)"getCurrentSlot", (char*)"()I", (void*)getCurrentSlot},
        {(char*)"setCurrentSlot", (char*)"(I)V", (void*)setCurrentSlot},
        {(char*)"click", (char*)"(IIII)V", (void*)click},
        {(char*)"getStackInSlot", (char*)"(I)Lnet/ovson/api/model/ItemStack;", (void*)getStackInSlot},
        {(char*)"getSize", (char*)"()I", (void*)getSize},
        {(char*)"getHotbarItem", (char*)"(I)Lnet/ovson/api/model/ItemStack;", (void*)getHotbarItem},
        {(char*)"findItemInHotbar", (char*)"(Ljava/lang/String;)I", (void*)findItemInHotbar},
        {(char*)"getHelmet", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getHelmet},
        {(char*)"getChestplate", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getChestplate},
        {(char*)"getLeggings", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getLeggings},
        {(char*)"getBoots", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getBoots},
        {(char*)"getChestSize", (char*)"()I", (void*)getChestSize},
        {(char*)"getStackInChestSlot", (char*)"(I)Lnet/ovson/api/model/ItemStack;", (void*)getStackInChestSlot},
        {(char*)"getContainerTitle", (char*)"()Ljava/lang/String;", (void*)getContainerTitle},
        {(char*)"isContainerOpen", (char*)"()Z", (void*)isContainerOpen},
        {(char*)"getCraftResult", (char*)"()Lnet/ovson/api/model/ItemStack;", (void*)getCraftResult},
        {(char*)"getStackInCraftingSlot", (char*)"(I)Lnet/ovson/api/model/ItemStack;", (void*)getStackInCraftingSlot},
        {(char*)"countItem", (char*)"(Ljava/lang/String;)I", (void*)countItem},
        {(char*)"findItem", (char*)"(Ljava/lang/String;)I", (void*)findItem},
        {(char*)"dropItem", (char*)"(I)V", (void*)dropItem},
        {(char*)"dropAllItems", (char*)"(Ljava/lang/String;)V", (void*)dropAllItems},
        {(char*)"swapSlots", (char*)"(II)V", (void*)swapSlots},
        {(char*)"openInventory", (char*)"()V", (void*)openInventory},
        {(char*)"closeScreen", (char*)"()V", (void*)closeScreen}
    };

    void registerNatives(JNIEnv* env, jclass cls) { env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0])); }
}



