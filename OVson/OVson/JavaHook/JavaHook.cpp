#include <windows.h>
#include <GL/gl.h>
#include "JavaHook.h"
#include "../Java.h"
#include "../Config/Config.h"
#include "../Utils/ColoredHitboxes.h"
#include "../Utils/HitboxDebug.h"
#include "../Utils/Logger.h"
#include <jvmti.h>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>
#include <filesystem>
#include "../Plugins/PluginLoader.h"
#include "../Logic/StatsTracker.h"
#include "HitboxHook_bytes.h"
#include "RenderManagerTransformer_bytes.h"

namespace fs = std::filesystem;

namespace JavaHook {

static jvmtiEnv* s_jvmti = nullptr;
static bool s_active = false;
static std::atomic<int> s_transformCount{0};

static jclass s_transformerClass = nullptr;
static jmethodID s_transformMethod = nullptr;

static bool redefineLoadedClass(jvmtiEnv* jvmti, JNIEnv* env, const char* slashName, const unsigned char* bytes, jint len) {
    if (!jvmti || !env || !bytes || len <= 0) return false;

    std::string targetSig = "L" + std::string(slashName) + ";";

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmtiError err = jvmti->GetLoadedClasses(&classCount, &classes);
    if (err != JVMTI_ERROR_NONE || !classes) return false;

    bool redefined = false;
    for (jint i = 0; i < classCount; ++i) {
        char* sig = nullptr;
        if (jvmti->GetClassSignature(classes[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig) {
            if (targetSig == sig) {
                jvmtiClassDefinition def;
                def.klass = classes[i];
                def.class_byte_count = len;
                def.class_bytes = bytes;
                jvmtiError rErr = jvmti->RedefineClasses(1, &def);
                if (rErr == JVMTI_ERROR_NONE) {
                    HitboxDebug::log("RedefineClasses for %s SUCCESS (%d bytes)", slashName, len);
                    redefined = true;
                } else {
                    HitboxDebug::log("RedefineClasses for %s returned error %d", slashName, rErr);
                }
            }
            jvmti->Deallocate((unsigned char*)sig);
        }
        env->DeleteLocalRef(classes[i]);
    }
    jvmti->Deallocate((unsigned char*)classes);
    return redefined;
}

extern "C" {
JNIEXPORT jboolean JNICALL Java_net_ovson_api_hook_HitboxHook_isTeamColoredHitboxesEnabled(JNIEnv* env, jclass clazz);
JNIEXPORT jint JNICALL Java_net_ovson_api_hook_HitboxHook_resolveBoxColor(JNIEnv* env, jclass clazz, jobject bb, jint r, jint g, jint b);
JNIEXPORT jint JNICALL Java_net_ovson_api_hook_HitboxHook_resolveEntityColor(JNIEnv* env, jclass clazz, jobject entity);
JNIEXPORT void JNICALL Java_net_ovson_api_hook_HitboxHook_debugLog(JNIEnv* env, jclass clazz, jstring jmsg);
}

static bool defineAndRegisterHitboxHook(JNIEnv* env, jobject classLoader, const char* loaderDesc) {
    if (!env) return false;

    jclass hookCls = nullptr;

    if (classLoader) {
        hookCls = env->DefineClass("net/ovson/api/hook/HitboxHook", classLoader,
                                   (const jbyte*)HitboxHook_class, (jsize)HitboxHook_class_len);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            hookCls = nullptr;
        }
    }

    if (!hookCls && classLoader) {
        jclass clsCls = env->FindClass("java/lang/Class");
        if (clsCls) {
            jmethodID m_forName = env->GetStaticMethodID(clsCls, "forName",
                "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;");
            if (m_forName) {
                jstring className = env->NewStringUTF("net.ovson.api.hook.HitboxHook");
                hookCls = (jclass)env->CallStaticObjectMethod(clsCls, m_forName, className, JNI_TRUE, classLoader);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    hookCls = nullptr;
                }
                env->DeleteLocalRef(className);
            }
            env->DeleteLocalRef(clsCls);
        }
    }

    if (!hookCls) {
        hookCls = env->FindClass("net/ovson/api/hook/HitboxHook");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            hookCls = nullptr;
        }
    }

    if (hookCls) {
        JNINativeMethod hitboxNatives[] = {
            {(char*)"isTeamColoredHitboxesEnabled", (char*)"()Z", (void*)&Java_net_ovson_api_hook_HitboxHook_isTeamColoredHitboxesEnabled},
            {(char*)"resolveBoxColor", (char*)"(Ljava/lang/Object;III)I", (void*)&Java_net_ovson_api_hook_HitboxHook_resolveBoxColor},
            {(char*)"resolveEntityColor", (char*)"(Ljava/lang/Object;)I", (void*)&Java_net_ovson_api_hook_HitboxHook_resolveEntityColor},
            {(char*)"debugLog", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_hook_HitboxHook_debugLog}
        };
        jint res = env->RegisterNatives(hookCls, hitboxNatives, 4);
        if (res == JNI_OK && !env->ExceptionCheck()) {
            Logger::info("[JavaHook] Registered HitboxHook natives on %s (loader=%p)", loaderDesc, classLoader);
            HitboxDebug::log("Registered HitboxHook natives on %s (loader=%p)", loaderDesc, classLoader);
        } else {
            if (env->ExceptionCheck()) env->ExceptionClear();
            Logger::error("[JavaHook] Failed to register HitboxHook natives on %s (loader=%p)", loaderDesc, classLoader);
            HitboxDebug::log("Failed to register HitboxHook natives on %s (loader=%p)", loaderDesc, classLoader);
        }
        env->DeleteLocalRef(hookCls);
        return true;
    } else {
        Logger::error("[JavaHook] Could not define HitboxHook on %s (loader=%p)", loaderDesc, classLoader);
        HitboxDebug::log("Could not define HitboxHook on %s (loader=%p)", loaderDesc, classLoader);
        return false;
    }
}

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

    std::string dotName(name);
    for (auto& c : dotName) if (c == '/') c = '.';

    bool isTarget = (dotName == "net.minecraft.client.renderer.RenderGlobal" || 
                     dotName == "bfr" ||
                     dotName == "net.minecraft.client.renderer.entity.RenderManager" ||
                     dotName == "biu" ||
                     dotName == "net.badlion.client.mods.render.Hitboxes");
    if (!isTarget) return;

    HitboxDebug::log("ClassFileLoadHook: name='%s' len=%d", name, class_data_len);
        std::wstring dumpDir = PluginLoader::getAppDataDir() + L"\\dumps";
        fs::create_directories(dumpDir);
        std::string inPath = fs::path(dumpDir).string() + "\\target_in_" + dotName + ".class";
        FILE* fIn = nullptr;
        if (fopen_s(&fIn, inPath.c_str(), "wb") == 0 && fIn) {
            fwrite(class_data, 1, class_data_len, fIn);
            fclose(fIn);
        }

    jbyteArray jClassData = jni_env->NewByteArray(class_data_len);
    if (!jClassData) return;
    jni_env->SetByteArrayRegion(jClassData, 0, class_data_len,
                                reinterpret_cast<const jbyte*>(class_data));

    jstring jClassName = jni_env->NewStringUTF(dotName.c_str());

    jbyteArray result = (jbyteArray)jni_env->CallStaticObjectMethod(
        s_transformerClass, s_transformMethod, jClassName, jClassData);

    if (jni_env->ExceptionCheck()) {
        if (isTarget) HitboxDebug::log("Exception during transform of %s", name);
        jni_env->ExceptionDescribe();
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
            HitboxDebug::log("Bytecode transformed for %s: %d -> %d bytes", name, class_data_len, newLen);

            if (isTarget) {
                std::wstring dumpDir = PluginLoader::getAppDataDir() + L"\\dumps";
                std::string outPath = fs::path(dumpDir).string() + "\\target_out_" + dotName + ".class";
                FILE* fOut = nullptr;
                if (fopen_s(&fOut, outPath.c_str(), "wb") == 0 && fOut) {
                    fwrite(newData, 1, newLen, fOut);
                    fclose(fOut);
                }
            }
        }
    } else if (isTarget) {
        HitboxDebug::log("Transformer returned null/same for %s", name);
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

    jclass classCls = env->FindClass("java/lang/Class");
    jmethodID getClassLoaderMethod = classCls ? env->GetMethodID(classCls, "getClassLoader", "()Ljava/lang/ClassLoader;") : nullptr;
    if (classCls) env->DeleteLocalRef(classCls);

    jclass cached = lc->GetClass(nameStr);
    if (cached) {
        if (getClassLoaderMethod) {
            jobject loader = env->CallObjectMethod(cached, getClassLoaderMethod);
            defineAndRegisterHitboxHook(env, loader, nameStr.c_str());
            if (loader) env->DeleteLocalRef(loader);
        }

        jvmtiError retransErr = s_jvmti->RetransformClasses(1, &cached);
        if (retransErr == JVMTI_ERROR_NONE) {
            Logger::info("[JavaHook] Retransformed class %s directly via lc->GetClass", nameStr.c_str());
            HitboxDebug::logTransform(nameStr, true);
            return;
        } else {
            Logger::error("[JavaHook] Direct retransform failed for %s (error %d)", nameStr.c_str(), retransErr);
            HitboxDebug::log("Direct retransform failed for %s (error %d)", nameStr.c_str(), retransErr);
        }
    }

    jint classCount = 0;
    jclass* classes = nullptr;
    jvmtiError err = s_jvmti->GetLoadedClasses(&classCount, &classes);
    if (err != JVMTI_ERROR_NONE || !classes) {
        Logger::error("[JavaHook] GetLoadedClasses failed (error %d)", err);
        HitboxDebug::logTransform(nameStr, false);
        return;
    }

    std::string targetSig = "L" + nameStr + ";";
    std::replace(targetSig.begin(), targetSig.end(), '.', '/');

    std::string notchSig = "";
    if (nameStr == "net.minecraft.client.renderer.entity.RenderManager") {
        notchSig = "Lbiu;";
    } else if (nameStr == "net.minecraft.client.renderer.RenderGlobal") {
        notchSig = "Lbfr;";
    }

    jclass targetClass = nullptr;
    for (jint i = 0; i < classCount; i++) {
        char* sig = nullptr;
        err = s_jvmti->GetClassSignature(classes[i], &sig, nullptr);
        if (err == JVMTI_ERROR_NONE && sig) {
            if (targetSig == sig || (!notchSig.empty() && notchSig == sig)) {
                targetClass = classes[i];
                s_jvmti->Deallocate((unsigned char*)sig);
                break;
            }
            s_jvmti->Deallocate((unsigned char*)sig);
        }
    }

    if (targetClass) {
        if (getClassLoaderMethod) {
            jobject loader = env->CallObjectMethod(targetClass, getClassLoaderMethod);
            defineAndRegisterHitboxHook(env, loader, nameStr.c_str());
            if (loader) env->DeleteLocalRef(loader);
        }

        jvmtiError retransErr = s_jvmti->RetransformClasses(1, &targetClass);
        if (retransErr != JVMTI_ERROR_NONE) {
            Logger::error("[JavaHook] Failed to retransform class %s (error %d)", nameStr.c_str(), retransErr);
            HitboxDebug::logTransform(nameStr, false);
            HitboxDebug::log("Fallback retransform failed for %s (error %d)", nameStr.c_str(), retransErr);
        } else {
            Logger::info("[JavaHook] Retransformed class %s", nameStr.c_str());
            HitboxDebug::logTransform(nameStr, true);
        }
    } else {
        Logger::error("[JavaHook] Could not find loaded class to retransform: %s", nameStr.c_str());
        HitboxDebug::logTransform(nameStr, false);
    }

    s_jvmti->Deallocate((unsigned char*)classes);
}

extern "C" {
JNIEXPORT jboolean JNICALL Java_net_ovson_api_hook_HitboxHook_isTeamColoredHitboxesEnabled(JNIEnv* env, jclass clazz) {
    return Config::isTeamColoredHitboxesEnabled() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL Java_net_ovson_api_hook_HitboxHook_resolveBoxColor(JNIEnv* env, jclass clazz, jobject bb, jint r, jint g, jint b) {
    if (!env || !bb) return -1;
    uint32_t col = OVson::Utils::resolveBoxColor(env, bb, (uint32_t)-1);
    return (col != (uint32_t)-1) ? static_cast<jint>(col) : -1;
}

JNIEXPORT jint JNICALL Java_net_ovson_api_hook_HitboxHook_resolveEntityColor(JNIEnv* env, jclass clazz, jobject entity) {
    if (!env || !entity) return -1;
    uint32_t col = OVson::Utils::resolvePlayerTeamColor(env, entity);
    return (col != (uint32_t)-1) ? static_cast<jint>(col & 0xFFFFFF) : -1;
}

JNIEXPORT void JNICALL Java_net_ovson_api_hook_HitboxHook_debugLog(JNIEnv* env, jclass clazz, jstring jmsg) {
    if (!env || !jmsg) return;
    const char* chars = env->GetStringUTFChars(jmsg, nullptr);
    if (chars) {
        HitboxDebug::log("%s", chars);
        env->ReleaseStringUTFChars(jmsg, chars);
    }
}
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

    std::wstring appDataDir = PluginLoader::getAppDataDir();
    const wchar_t* jarsToSearch[] = { L"\\OVsonAPI.jar", L"\\asm.jar", L"\\asm-tree.jar" };
    for (const wchar_t* jarName : jarsToSearch) {
        std::wstring jarPath = appDataDir + jarName;
        if (fs::exists(jarPath)) {
            std::string utf8Path = fs::path(jarPath).string();
            jvmtiError bErr = s_jvmti->AddToBootstrapClassLoaderSearch(utf8Path.c_str());
            HitboxDebug::log("AddToBootstrapClassLoaderSearch result: %d (%s)", bErr, utf8Path.c_str());
            jvmtiError sErr = s_jvmti->AddToSystemClassLoaderSearch(utf8Path.c_str());
            HitboxDebug::log("AddToSystemClassLoaderSearch result: %d (%s)", sErr, utf8Path.c_str());
        }
    }

    JNIEnv* env = lc->getEnv();
    if (env) {
        defineAndRegisterHitboxHook(env, nullptr, "Bootstrap");

        jobject apiLoader = PluginLoader::getAPIClassLoader();
        if (apiLoader) {
            defineAndRegisterHitboxHook(env, apiLoader, "apiLoader");
        }

        jclass classClsInit = env->FindClass("java/lang/Class");
        jmethodID getLoaderInit = classClsInit ? env->GetMethodID(classClsInit, "getClassLoader", "()Ljava/lang/ClassLoader;") : nullptr;
        if (classClsInit) env->DeleteLocalRef(classClsInit);
        if (getLoaderInit) {
            const char* probeClasses[] = {
                "net.minecraft.client.Minecraft", "ave",
                "net.minecraft.client.renderer.entity.RenderManager", "biu"
            };
            for (const char* pc : probeClasses) {
                jclass clsProbe = lc->GetClass(pc);
                if (clsProbe) {
                    jobject pLoader = env->CallObjectMethod(clsProbe, getLoaderInit);
                    if (pLoader) {
                        defineAndRegisterHitboxHook(env, pLoader, pc);
                        env->DeleteLocalRef(pLoader);
                    }
                }
            }
        }
        jclass cls = nullptr;
        jmethodID loadClassMethod = nullptr;
        if (apiLoader) {
            jclass clsLoaderClass = env->GetObjectClass(apiLoader);
            loadClassMethod = env->GetMethodID(clsLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
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

                jmethodID clearMethod = env->GetStaticMethodID(s_transformerClass, "clear", "()V");
                if (clearMethod) {
                    env->CallStaticVoidMethod(s_transformerClass, clearMethod);
                    HitboxDebug::log("Cleared previous transformers from TransformerRegistry");
                }

                redefineLoadedClass(s_jvmti, env, "net/ovson/api/hook/HitboxHook", HitboxHook_class, (jint)HitboxHook_class_len);
                redefineLoadedClass(s_jvmti, env, "net/ovson/api/hook/RenderManagerTransformer", RenderManagerTransformer_class, (jint)RenderManagerTransformer_class_len);

                // Register RenderManagerTransformer
                jmethodID regMethod = env->GetStaticMethodID(s_transformerClass, "register", "(Lnet/ovson/api/hook/ClassTransformer;)V");
                if (regMethod && apiLoader && loadClassMethod) {
                    jstring jTransName = env->NewStringUTF("net.ovson.api.hook.RenderManagerTransformer");
                    jclass rTransCls = (jclass)env->CallObjectMethod(apiLoader, loadClassMethod, jTransName);
                    env->DeleteLocalRef(jTransName);
                    if (rTransCls) {
                        jmethodID rTransCtor = env->GetMethodID(rTransCls, "<init>", "()V");
                        if (rTransCtor) {
                            jobject rTransObj = env->NewObject(rTransCls, rTransCtor);
                            if (rTransObj) {
                                env->CallStaticVoidMethod(s_transformerClass, regMethod, rTransObj);
                                env->DeleteLocalRef(rTransObj);
                                Logger::info("[JavaHook] Registered RenderManagerTransformer with TransformerRegistry.");
                            }
                        }
                        env->DeleteLocalRef(rTransCls);
                    }
                }


                // Retransform targets (Hitboxes, RenderManager, biu, RenderGlobal, bfr)
                const char* targetsToRetransform[] = {
                    "net.badlion.client.mods.render.Hitboxes",
                    "net.minecraft.client.renderer.entity.RenderManager",
                    "biu",
                    "net.minecraft.client.renderer.RenderGlobal",
                    "bfr"
                };
                for (const char* tName : targetsToRetransform) {
                    jstring jName = env->NewStringUTF(tName);
                    Java_net_ovson_api_hook_TransformerRegistry_retransform(env, nullptr, jName);
                    env->DeleteLocalRef(jName);
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

    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    OVson::Utils::resetBoxColorState(env);

    if (s_transformerClass) {
        if (env) {
            env->DeleteGlobalRef(s_transformerClass);
        }
        s_transformerClass = nullptr;
        s_transformMethod = nullptr;
    }

    s_jvmti->DisposeEnvironment();

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
