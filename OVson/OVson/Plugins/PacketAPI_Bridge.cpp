#include "PacketAPI_Bridge.h"
#include "../Java.h"

namespace PacketAPIBridge {

    // PacketFactory methods
    jobject JNICALL createChatPacket(JNIEnv* env, jclass clazz, jstring msg) {
        return nullptr;
    }

    jobject JNICALL createUseEntityPacket(JNIEnv* env, jclass clazz, jint entityId, jstring action) {
        return nullptr;
    }

    jobject JNICALL createPlayerPacket(JNIEnv* env, jclass clazz, jboolean onGround) {
        return nullptr;
    }

    jobject JNICALL createPositionPacket(JNIEnv* env, jclass clazz, jdouble x, jdouble y, jdouble z, jboolean onGround) {
        return nullptr;
    }

    jobject JNICALL createRotationPacket(JNIEnv* env, jclass clazz, jfloat yaw, jfloat pitch, jboolean onGround) {
        return nullptr;
    }

    jobject JNICALL createPosRotPacket(JNIEnv* env, jclass clazz, jdouble x, jdouble y, jdouble z, jfloat yaw, jfloat pitch, jboolean onGround) {
        return nullptr;
    }

    jobject JNICALL createDiggingPacket(JNIEnv* env, jclass clazz, jint status, jint x, jint y, jint z, jint facing) {
        return nullptr;
    }

    jobject JNICALL createBlockPlacePacket(JNIEnv* env, jclass clazz, jint x, jint y, jint z, jint direction) {
        return nullptr;
    }

    jobject JNICALL createHeldItemPacket(JNIEnv* env, jclass clazz, jint slot) {
        return nullptr;
    }

    jobject JNICALL createAnimationPacket(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    jobject JNICALL createEntityActionPacket(JNIEnv* env, jclass clazz, jint action) {
        return nullptr;
    }

    jobject JNICALL createCloseWindowPacket(JNIEnv* env, jclass clazz, jint windowId) {
        return nullptr;
    }

    jobject JNICALL createAbilitiesPacket(JNIEnv* env, jclass clazz) {
        return nullptr;
    }

    // PacketHelper methods
    jstring JNICALL getPacketType(JNIEnv* env, jclass clazz, jobject obj) {
        if (obj == nullptr) return env->NewStringUTF("");
        jclass c = env->GetObjectClass(obj);
        jmethodID mid = env->GetMethodID(c, "getClass", "()Ljava/lang/Class;");
        if (mid == nullptr) {
            env->DeleteLocalRef(c);
            return env->NewStringUTF("");
        }
        jobject classObj = env->CallObjectMethod(obj, mid);
        if (classObj == nullptr) {
            env->DeleteLocalRef(c);
            return env->NewStringUTF("");
        }
        
        jclass classClass = env->GetObjectClass(classObj);
        jmethodID getSimpleNameMid = env->GetMethodID(classClass, "getSimpleName", "()Ljava/lang/String;");
        if (getSimpleNameMid == nullptr) {
            env->DeleteLocalRef(c);
            env->DeleteLocalRef(classObj);
            env->DeleteLocalRef(classClass);
            return env->NewStringUTF("");
        }
        
        jstring name = (jstring)env->CallObjectMethod(classObj, getSimpleNameMid);
        
        env->DeleteLocalRef(c);
        env->DeleteLocalRef(classObj);
        env->DeleteLocalRef(classClass);
        return name;
    }

    jdouble JNICALL getPacketX(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getPacketY(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getPacketZ(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jfloat JNICALL getPacketYaw(JNIEnv* env, jclass clazz, jobject obj) { return 0.0f; }
    jfloat JNICALL getPacketPitch(JNIEnv* env, jclass clazz, jobject obj) { return 0.0f; }
    jboolean JNICALL getPacketOnGround(JNIEnv* env, jclass clazz, jobject obj) { return JNI_FALSE; }
    
    jint JNICALL getVelocityEntityId(JNIEnv* env, jclass clazz, jobject obj) { return 0; }
    jdouble JNICALL getVelocityX(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getVelocityY(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getVelocityZ(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    
    jdouble JNICALL getPosLookX(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getPosLookY(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jdouble JNICALL getPosLookZ(JNIEnv* env, jclass clazz, jobject obj) { return 0.0; }
    jfloat JNICALL getPosLookYaw(JNIEnv* env, jclass clazz, jobject obj) { return 0.0f; }
    jfloat JNICALL getPosLookPitch(JNIEnv* env, jclass clazz, jobject obj) { return 0.0f; }
    
    jstring JNICALL getChatMessage(JNIEnv* env, jclass clazz, jobject obj) { return env->NewStringUTF(""); }
    jint JNICALL getChatType(JNIEnv* env, jclass clazz, jobject obj) { return 0; }
    
    jobject JNICALL getPacketField(JNIEnv* env, jclass clazz, jobject obj, jstring fieldName) { return nullptr; }
    void JNICALL setPacketField(JNIEnv* env, jclass clazz, jobject obj, jstring fieldName, jobject val) { }


    JNINativeMethod factoryMethods[] = {
        {(char*)"createChatPacket", (char*)"(Ljava/lang/String;)Ljava/lang/Object;", (void*)createChatPacket},
        {(char*)"createUseEntityPacket", (char*)"(ILjava/lang/String;)Ljava/lang/Object;", (void*)createUseEntityPacket},
        {(char*)"createPlayerPacket", (char*)"(Z)Ljava/lang/Object;", (void*)createPlayerPacket},
        {(char*)"createPositionPacket", (char*)"(DDDZ)Ljava/lang/Object;", (void*)createPositionPacket},
        {(char*)"createRotationPacket", (char*)"(FFZ)Ljava/lang/Object;", (void*)createRotationPacket},
        {(char*)"createPosRotPacket", (char*)"(DDDFFZ)Ljava/lang/Object;", (void*)createPosRotPacket},
        {(char*)"createDiggingPacket", (char*)"(IIIII)Ljava/lang/Object;", (void*)createDiggingPacket},
        {(char*)"createBlockPlacePacket", (char*)"(IIII)Ljava/lang/Object;", (void*)createBlockPlacePacket},
        {(char*)"createHeldItemPacket", (char*)"(I)Ljava/lang/Object;", (void*)createHeldItemPacket},
        {(char*)"createAnimationPacket", (char*)"()Ljava/lang/Object;", (void*)createAnimationPacket},
        {(char*)"createEntityActionPacket", (char*)"(I)Ljava/lang/Object;", (void*)createEntityActionPacket},
        {(char*)"createCloseWindowPacket", (char*)"(I)Ljava/lang/Object;", (void*)createCloseWindowPacket},
        {(char*)"createAbilitiesPacket", (char*)"()Ljava/lang/Object;", (void*)createAbilitiesPacket}
    };

    JNINativeMethod helperMethods[] = {
        {(char*)"getPacketType", (char*)"(Ljava/lang/Object;)Ljava/lang/String;", (void*)getPacketType},
        {(char*)"getPacketX", (char*)"(Ljava/lang/Object;)D", (void*)getPacketX},
        {(char*)"getPacketY", (char*)"(Ljava/lang/Object;)D", (void*)getPacketY},
        {(char*)"getPacketZ", (char*)"(Ljava/lang/Object;)D", (void*)getPacketZ},
        {(char*)"getPacketYaw", (char*)"(Ljava/lang/Object;)F", (void*)getPacketYaw},
        {(char*)"getPacketPitch", (char*)"(Ljava/lang/Object;)F", (void*)getPacketPitch},
        {(char*)"getPacketOnGround", (char*)"(Ljava/lang/Object;)Z", (void*)getPacketOnGround},
        {(char*)"getVelocityEntityId", (char*)"(Ljava/lang/Object;)I", (void*)getVelocityEntityId},
        {(char*)"getVelocityX", (char*)"(Ljava/lang/Object;)D", (void*)getVelocityX},
        {(char*)"getVelocityY", (char*)"(Ljava/lang/Object;)D", (void*)getVelocityY},
        {(char*)"getVelocityZ", (char*)"(Ljava/lang/Object;)D", (void*)getVelocityZ},
        {(char*)"getPosLookX", (char*)"(Ljava/lang/Object;)D", (void*)getPosLookX},
        {(char*)"getPosLookY", (char*)"(Ljava/lang/Object;)D", (void*)getPosLookY},
        {(char*)"getPosLookZ", (char*)"(Ljava/lang/Object;)D", (void*)getPosLookZ},
        {(char*)"getPosLookYaw", (char*)"(Ljava/lang/Object;)F", (void*)getPosLookYaw},
        {(char*)"getPosLookPitch", (char*)"(Ljava/lang/Object;)F", (void*)getPosLookPitch},
        {(char*)"getChatMessage", (char*)"(Ljava/lang/Object;)Ljava/lang/String;", (void*)getChatMessage},
        {(char*)"getChatType", (char*)"(Ljava/lang/Object;)I", (void*)getChatType},
        {(char*)"getPacketField", (char*)"(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;", (void*)getPacketField},
        {(char*)"setPacketField", (char*)"(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)V", (void*)setPacketField}
    };

    void registerFactoryNatives(JNIEnv* env, jclass cls) { env->RegisterNatives(cls, factoryMethods, sizeof(factoryMethods)/sizeof(factoryMethods[0])); }
    void registerHelperNatives(JNIEnv* env, jclass cls) { env->RegisterNatives(cls, helperMethods, sizeof(helperMethods)/sizeof(helperMethods[0])); }
}

