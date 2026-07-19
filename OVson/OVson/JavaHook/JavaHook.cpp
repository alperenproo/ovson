#include "JavaHook.h"
#include "../Java.h"
#include "../Utils/Logger.h"
#include <jvmti.h>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>
#include "../Plugins/PluginLoader.h"

namespace JavaHook {

static jvmtiEnv* s_jvmti = nullptr;
static bool s_active = false;
static std::atomic<int> s_transformCount{0};

static jclass s_transformerClass = nullptr;
static jmethodID s_transformMethod = nullptr;

static void JNICALL classFileLoadHookCallback(
    jvmtiEnv* jvmti_env,
    JNIEnv* jni_env,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data)
{
    if (!s_transformerClass || !s_transformMethod || !name) return;

    if (strncmp(name, "java/", 5) == 0 ||
        strncmp(name, "javax/", 6) == 0 ||
        strncmp(name, "sun/", 4) == 0 ||
        strncmp(name, "jdk/", 4) == 0 ||
        strncmp(name, "net/ovson/api/", 14) == 0) {
        return;
    }

    jbyteArray jClassData = jni_env->NewByteArray(class_data_len);
    if (!jClassData) return;
    jni_env->SetByteArrayRegion(jClassData, 0, class_data_len,
                                reinterpret_cast<const jbyte*>(class_data));

    std::string dotName(name);
    for (auto& c : dotName) if (c == '/') c = '.';
    jstring jClassName = jni_env->NewStringUTF(dotName.c_str());

    jbyteArray result = (jbyteArray)jni_env->CallStaticObjectMethod(
        s_transformerClass, s_transformMethod, jClassName, jClassData);

    if (jni_env->ExceptionCheck()) {
        jni_env->ExceptionClear();
        jni_env->DeleteLocalRef(jClassData);
        jni_env->DeleteLocalRef(jClassName);
        return;
    }

    if (result && result != jClassData) {
        jint newLen = jni_env->GetArrayLength(result);
        unsigned char* newData = nullptr;

        jvmtiError err = jvmti_env->Allocate(newLen, &newData);
        if (err == JVMTI_ERROR_NONE && newData) {
            jni_env->GetByteArrayRegion(result, 0, newLen,
                                        reinterpret_cast<jbyte*>(newData));
            *new_class_data = newData;
            *new_class_data_len = newLen;
            s_transformCount.fetch_add(1);
        }
    }

    jni_env->DeleteLocalRef(jClassData);
    jni_env->DeleteLocalRef(jClassName);
    if (result) jni_env->DeleteLocalRef(result);
}

JNIEXPORT void JNICALL Java_net_ovson_api_hook_TransformerRegistry_retransform
  (JNIEnv *env, jclass cls, jstring className)
{
    if (!s_jvmti) return;

    const char* chars = env->GetStringUTFChars(className, nullptr);
    if (!chars) return;

    std::string nameStr(chars);
    env->ReleaseStringUTFChars(className, chars);

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmtiError err = s_jvmti->GetLoadedClasses(&classCount, &classes);
    if (err != JVMTI_ERROR_NONE || !classes) {
        Logger::error("[JavaHook] GetLoadedClasses failed (error %d)", err);
        return;
    }

    jclass targetClass = nullptr;
    for (jint i = 0; i < classCount; i++) {
        char* sig = nullptr;
        err = s_jvmti->GetClassSignature(classes[i], &sig, nullptr);
        if (err == JVMTI_ERROR_NONE && sig) {
            std::string targetSig = "L" + nameStr + ";";
            std::replace(targetSig.begin(), targetSig.end(), '.', '/');

            if (targetSig == sig) {
                targetClass = classes[i];
                s_jvmti->Deallocate((unsigned char*)sig);
                break;
            }
            s_jvmti->Deallocate((unsigned char*)sig);
        }
    }

    if (targetClass) {
        jvmtiError retransErr = s_jvmti->RetransformClasses(1, &targetClass);
        if (retransErr != JVMTI_ERROR_NONE) {
            Logger::error("[JavaHook] Failed to retransform class %s (error %d)", nameStr.c_str(), retransErr);
        } else {
            Logger::info("[JavaHook] Retransformed class %s", nameStr.c_str());
        }
    } else {
        Logger::error("[JavaHook] Could not find loaded class to retransform: %s", nameStr.c_str());
    }

    s_jvmti->Deallocate((unsigned char*)classes);
}

void initialize() {
    if (!lc || !lc->vm) return;

    Logger::info("[JavaHook] Initializing JVMTI bytecode hook system...");

    jint res = lc->vm->GetEnv(reinterpret_cast<void**>(&s_jvmti), JVMTI_VERSION_1_2);
    if (res != JNI_OK || !s_jvmti) {
        Logger::error("[JavaHook] Failed to get JVMTI environment (error %d)", res);
        return;
    }

    jvmtiCapabilities caps = {};
    caps.can_generate_all_class_hook_events = 1;
    caps.can_retransform_classes = 1;
    caps.can_redefine_classes = 1;
    jvmtiError err = s_jvmti->AddCapabilities(&caps);
    if (err != JVMTI_ERROR_NONE) {
        Logger::error("[JavaHook] Failed to add JVMTI capabilities (error %d)", err);
        s_jvmti = nullptr;
        return;
    }

    jvmtiEventCallbacks callbacks = {};
    callbacks.ClassFileLoadHook = classFileLoadHookCallback;
    err = s_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        Logger::error("[JavaHook] Failed to set event callbacks (error %d)", err);
        s_jvmti = nullptr;
        return;
    }

    err = s_jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                             JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    if (err != JVMTI_ERROR_NONE) {
        Logger::error("[JavaHook] Failed to enable ClassFileLoadHook (error %d)", err);
        s_jvmti = nullptr;
        return;
    }

    JNIEnv* env = lc->getEnv();
    if (env) {
        jobject apiLoader = PluginLoader::getAPIClassLoader();
        jclass cls = nullptr;
        if (apiLoader) {
            jclass clsLoaderClass = env->GetObjectClass(apiLoader);
            jmethodID loadClassMethod = env->GetMethodID(clsLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            env->DeleteLocalRef(clsLoaderClass);
            if (loadClassMethod) {
                jstring jName = env->NewStringUTF("net.ovson.api.hook.TransformerRegistry");
                cls = (jclass)env->CallObjectMethod(apiLoader, loadClassMethod, jName);
                env->DeleteLocalRef(jName);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    cls = nullptr;
                }
            }
        }

        if (cls) {
            s_transformerClass = (jclass)env->NewGlobalRef(cls);
            s_transformMethod = env->GetStaticMethodID(cls, "transform",
                "(Ljava/lang/String;[B)[B");
            env->DeleteLocalRef(cls);

            if (!s_transformMethod) {
                Logger::error("[JavaHook] TransformerRegistry.transform() method not found!");
                env->DeleteGlobalRef(s_transformerClass);
                s_transformerClass = nullptr;
            } else {
                JNINativeMethod registryNatives[] = {
                    {(char*)"retransform", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_hook_TransformerRegistry_retransform}
                };
                jint nativeRes = env->RegisterNatives(s_transformerClass, registryNatives, 1);
                if (nativeRes != JNI_OK || env->ExceptionCheck()) {
                    Logger::error("[JavaHook] Failed to register natives for TransformerRegistry!");
                    if (env->ExceptionCheck()) env->ExceptionClear();
                } else {
                    Logger::info("[JavaHook] Registered native retransform method for TransformerRegistry.");
                }
            }
        } else {
            Logger::info("[JavaHook] TransformerRegistry class not found (plugins may register later).");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    s_active = true;
    Logger::info("[JavaHook] JVMTI bytecode hook system active. Transformer: %s",
                 s_transformerClass ? "ready" : "pending");
}

void shutdown() {
    if (!s_jvmti) return;

    Logger::info("[JavaHook] Shutting down... (%d classes transformed)", s_transformCount.load());

    s_jvmti->SetEventNotificationMode(JVMTI_DISABLE,
                                       JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);

    if (s_transformerClass) {
        JNIEnv* env = lc ? lc->getEnv() : nullptr;
        if (env) {
            env->DeleteGlobalRef(s_transformerClass);
        }
        s_transformerClass = nullptr;
        s_transformMethod = nullptr;
    }

    s_active = false;
    s_jvmti = nullptr;
}

bool isActive() {
    return s_active;
}

int getTransformCount() {
    return s_transformCount.load();
}

} // namespace JavaHook
