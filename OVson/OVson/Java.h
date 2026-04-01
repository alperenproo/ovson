#pragma once
#include <algorithm>
#include <atomic>
#include <iostream>
#include <jni.h>
#include <jvmti.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>


class Lunar {
public:
  static std::atomic<bool> &isCleaningUp() {
    static std::atomic<bool> cleaning{false};
    return cleaning;
  }

  static Lunar *getInstance() {
    static Lunar *instance = new Lunar();
    return instance;
  }

  jfieldID FindFieldBySignature(jclass cls, const char *sig,
                                bool isStatic = false) {
    JNIEnv *env = getEnv();
    if (!env || !cls || !jvmti)
      return nullptr;

    jint fieldCount = 0;
    jfieldID *fields = nullptr;
    if (jvmti->GetClassFields(cls, &fieldCount, &fields) != JVMTI_ERROR_NONE)
      return nullptr;

    jfieldID result = nullptr;
    for (int i = 0; i < fieldCount; i++) {
      char *fName = nullptr;
      char *fSig = nullptr;
      if (jvmti->GetFieldName(cls, fields[i], &fName, &fSig, nullptr) ==
          JVMTI_ERROR_NONE) {
        if (fSig && std::string(fSig) == sig) {
          jint modifiers = 0;
          jvmti->GetFieldModifiers(cls, fields[i], &modifiers);
          bool actualStatic = (modifiers & 0x0008) != 0;
          if (actualStatic == isStatic) {
            result = fields[i];
            jvmti->Deallocate((unsigned char *)fName);
            jvmti->Deallocate((unsigned char *)fSig);
            break;
          }
        }
        if (fName)
          jvmti->Deallocate((unsigned char *)fName);
        if (fSig)
          jvmti->Deallocate((unsigned char *)fSig);
      }
    }
    if (fields)
      jvmti->Deallocate((unsigned char *)fields);
    return result;
  }

  jmethodID FindMethodBySignature(jclass cls, const char *sig,
                                  bool isStatic = false) {
    JNIEnv *env = getEnv();
    if (!env || !cls || !jvmti)
      return nullptr;

    jint methodCount = 0;
    jmethodID *methods = nullptr;
    if (jvmti->GetClassMethods(cls, &methodCount, &methods) != JVMTI_ERROR_NONE)
      return nullptr;

    jmethodID result = nullptr;
    for (int i = 0; i < methodCount; i++) {
      char *mName = nullptr;
      char *mSig = nullptr;
      if (jvmti->GetMethodName(methods[i], &mName, &mSig, nullptr) ==
          JVMTI_ERROR_NONE) {
        if (mSig && std::string(mSig) == sig) {
          jint modifiers = 0;
          jvmti->GetMethodModifiers(methods[i], &modifiers);
          bool actualStatic = (modifiers & 0x0008) != 0;
          if (actualStatic == isStatic) {
            result = methods[i];
            jvmti->Deallocate((unsigned char *)mName);
            jvmti->Deallocate((unsigned char *)mSig);
            break;
          }
        }
        if (mName)
          jvmti->Deallocate((unsigned char *)mName);
        if (mSig)
          jvmti->Deallocate((unsigned char *)mSig);
      }
    }
    if (methods)
      jvmti->Deallocate((unsigned char *)methods);
    return result;
  }

  typedef bool (*DiagnosticReporter)(const std::string &);
  static DiagnosticReporter reporter;
  JavaVM *vm;
  jvmtiEnv *jvmti;

  Lunar() : vm(nullptr), jvmti(nullptr) {}

  JNIEnv *getEnv() {
    if (isCleaningUp() || !vm)
      return nullptr;
    JNIEnv *env = nullptr;
    jint res = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
      JavaVMAttachArgs args;
      args.version = JNI_VERSION_1_6;
      args.name = (char *)"OVson-Thread";
      args.group = NULL;
      if (vm->AttachCurrentThread((void **)&env, &args) != JNI_OK) {
        return nullptr;
      }
    }
    if (env && !jvmti) {
      vm->GetEnv((void **)&jvmti, 0x30010001); // JVMTI_VERSION_1_1
    }
    return env;
  }

  void GetLoadedClasses() {
    JNIEnv *env = getEnv();
    if (!vm || !env)
      return;
    if (vm->GetEnv((void **)&jvmti, 0x30010001) != 0)
      return; // JVMTI_VERSION_1_1, 0 is JNI_OK

    jclass lang = env->FindClass("java/lang/Class");
    if (!lang)
      return;
    jmethodID getName =
        env->GetMethodID(lang, "getName", "()Ljava/lang/String;");

    jclass *classesPtr = nullptr;
    jint amount = 0;
    if (jvmti->GetLoadedClasses(&amount, &classesPtr) != JVMTI_ERROR_NONE) {
      env->DeleteLocalRef(lang);
      return;
    }

    Cleanup();

    for (int i = 0; i < amount; i++) {
      jstring name = (jstring)env->CallObjectMethod(classesPtr[i], getName);
      if (name) {
        const char *classNameUtf = env->GetStringUTFChars(name, 0);
        if (classNameUtf) {
          std::string className(classNameUtf);
          std::replace(className.begin(), className.end(), '/', '.');

          jclass globalCls = (jclass)env->NewGlobalRef(classesPtr[i]);
          classes[className] = globalCls;

          env->ReleaseStringUTFChars(name, classNameUtf);
        }
        env->DeleteLocalRef(name);
      }
    }

    if (classesPtr)
      jvmti->Deallocate((unsigned char *)classesPtr);
    env->DeleteLocalRef(lang);

    GetClass("java.util.Collection");
    GetClass("java.util.Iterator");
    GetClass("net.minecraft.client.Minecraft");
    GetClass("net.minecraft.client.network.NetworkPlayerInfo");
    GetClass("net.minecraft.util.ChatComponentText");
    GetClass("net.minecraft.client.gui.GuiIngame");
    GetClass("net.minecraft.client.gui.GuiPlayerTabOverlay");
    GetClass("net.minecraft.util.IChatComponent");
    GetClass("net.minecraft.client.gui.GuiChat");
    GetClass("net.minecraft.client.gui.GuiScreen");
    GetClass("net.minecraft.client.gui.GuiTextField");
  }

  virtual ~Lunar() { Cleanup(); }

  jclass GetClass(const std::string &className) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    JNIEnv *env = getEnv();
    if (!env)
      return nullptr;
    auto it = classes.find(className);
    if (it != classes.end())
      return it->second;

    std::string internalName = className;
    std::replace(internalName.begin(), internalName.end(), '.', '/');
    jclass localCls = env->FindClass(internalName.c_str());
    if (localCls) {
      jclass globalCls = (jclass)env->NewGlobalRef(localCls);
      classes[className] = globalCls;
      env->DeleteLocalRef(localCls);
      if (env->ExceptionCheck())
        env->ExceptionClear();
      return globalCls;
    }

    if (env->ExceptionCheck())
      env->ExceptionClear();

    // 3rd
    static const std::unordered_map<std::string, std::string> notchMap = {
        {"net.minecraft.client.Minecraft", "ave"},
        {"net.minecraft.client.entity.EntityPlayerSP", "bew"},
        {"net.minecraft.client.gui.GuiIngame", "avo"},
        {"net.minecraft.client.gui.GuiNewChat", "avt"},
        {"net.minecraft.client.gui.GuiChat", "awv"},
        {"net.minecraft.client.gui.GuiScreen", "axu"},
        {"net.minecraft.client.gui.GuiTextField", "avw"},
        {"net.minecraft.client.gui.GuiPlayerTabOverlay", "awh"},
        {"net.minecraft.client.multiplayer.WorldClient", "bdb"},
        {"net.minecraft.client.network.NetHandlerPlayClient", "bcy"},
        {"net.minecraft.client.network.NetworkPlayerInfo", "bdc"},
        {"net.minecraft.scoreboard.Scoreboard", "auo"},
        {"net.minecraft.scoreboard.ScorePlayerTeam", "aul"},
        {"net.minecraft.scoreboard.Score", "aum"},
        {"net.minecraft.scoreboard.ScoreObjective", "auk"},
        {"net.minecraft.client.multiplayer.ServerData", "bde"},
        {"net.minecraft.client.multiplayer.PlayerControllerMP", "bda"},
        {"net.minecraft.util.IChatComponent", "eu"},
        {"net.minecraft.util.ChatComponentText", "fa"},
        {"net.minecraft.client.gui.inventory.GuiContainer", "ayl"},
        {"net.minecraft.inventory.Container", "xi"},
        {"net.minecraft.inventory.Slot", "yg"},
        {"net.minecraft.item.ItemStack", "zx"},
        {"net.minecraft.world.World", "adm"},
        {"net.minecraft.entity.Entity", "pk"},
        {"net.minecraft.util.MovingObjectPosition", "auh"},
        {"net.minecraft.util.BlockPos", "cj"},
        {"net.minecraft.block.state.IBlockState", "alz"},
        {"net.minecraft.block.Block", "afh"},
        {"net.minecraft.block.BlockBed", "afg"},
        {"net.minecraft.client.settings.GameSettings", "avh"},
        {"net.minecraft.util.Timer", "avl"},
        {"net.minecraft.client.renderer.entity.RenderManager", "biu"},
        {"net.minecraft.client.renderer.texture.TextureMap", "bmh"},
        {"net.minecraft.client.renderer.texture.TextureManager", "bmj"},
        {"net.minecraft.client.renderer.texture.TextureAtlasSprite", "bmi"},
        {"net.minecraft.world.chunk.Chunk", "amy"},
        {"net.minecraft.world.chunk.storage.ExtendedBlockStorage", "amz"},
        {"net.minecraft.util.ResourceLocation", "jy"},
        {"net.minecraft.util.RegistryNamespacedDefaultedByKey", "co"},
        {"net.minecraft.client.renderer.BlockRendererDispatcher", "bgd"},
        {"net.minecraft.client.renderer.BlockModelShapes", "bgc"},
        {"net.minecraft.client.multiplayer.WorldClient", "bdb"},
        {"net.minecraft.scoreboard.Scoreboard", "auo"},
        {"net.minecraft.scoreboard.ScorePlayerTeam", "aul"},
        {"net.minecraft.scoreboard.ScoreObjective", "auk"},
        {"net.minecraft.scoreboard.Score", "aum"},
    };
    auto nit = notchMap.find(className);
    if (nit != notchMap.end()) {
      localCls = env->FindClass(nit->second.c_str());
      if (localCls) {
        jclass globalCls = (jclass)env->NewGlobalRef(localCls);
        classes[className] = globalCls;
        env->DeleteLocalRef(localCls);
        if (env->ExceptionCheck())
          env->ExceptionClear();
        return globalCls;
      }
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    return nullptr;
  }

  jfieldID GetFieldID(jclass cls, const char *name, const char *sig,
                      const char *srgName = nullptr,
                      const char *notchName = nullptr,
                      const char *notchSig = nullptr) {
    JNIEnv *env = getEnv();
    if (!env || !cls)
      return nullptr;
    jfieldID fid = env->GetFieldID(cls, name, sig);
    if (!fid && srgName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetFieldID(cls, srgName, sig);
    }
    if (!fid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetFieldID(cls, notchName, notchSig ? notchSig : sig);
    }
    if (!fid) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (reporter)
        reporter("§cFAILED: §fField " + std::string(name));
    }
    return fid;
  }

  jfieldID GetStaticFieldID(jclass cls, const char *name, const char *sig,
                            const char *srgName = nullptr,
                            const char *notchName = nullptr,
                            const char *notchSig = nullptr) {
    JNIEnv *env = getEnv();
    if (!env || !cls)
      return nullptr;
    jfieldID fid = env->GetStaticFieldID(cls, name, sig);
    if (!fid && srgName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetStaticFieldID(cls, srgName, sig);
    }
    if (!fid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetStaticFieldID(cls, notchName, notchSig ? notchSig : sig);
    }
    if (!fid) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (reporter)
        reporter("§cFAILED: §fStaticField " + std::string(name));
    }
    return fid;
  }

  jmethodID GetMethodID(jclass cls, const char *name, const char *sig,
                        const char *srgName = nullptr,
                        const char *notchName = nullptr,
                        const char *notchSig = nullptr) {
    JNIEnv *env = getEnv();
    if (!env || !cls)
      return nullptr;
    jmethodID mid = env->GetMethodID(cls, name, sig);
    if (!mid && srgName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetMethodID(cls, srgName, sig);
    }
    if (!mid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetMethodID(cls, notchName, notchSig ? notchSig : sig);
    }
    if (!mid) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (reporter)
        reporter("§cFAILED: §fMethod " + std::string(name));
    }
    return mid;
  }

  jmethodID GetStaticMethodID(jclass cls, const char *name, const char *sig,
                              const char *srgName = nullptr,
                              const char *notchName = nullptr,
                              const char *notchSig = nullptr) {
    JNIEnv *env = getEnv();
    if (!env || !cls)
      return nullptr;
    jmethodID mid = env->GetStaticMethodID(cls, name, sig);
    if (!mid && srgName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetStaticMethodID(cls, srgName, sig);
    }
    if (!mid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetStaticMethodID(cls, notchName, notchSig ? notchSig : sig);
    }
    if (!mid) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (reporter)
        reporter("§cFAILED: §fStaticMethod " + std::string(name));
    }
    return mid;
  }

  jobject GetStaticObjectField(jclass cls, const char *name, const char *sig,
                               const char *srgName = nullptr,
                               const char *notchName = nullptr,
                               const char *notchSig = nullptr) {
    if (!cls)
      return nullptr;
    jfieldID fid =
        GetStaticFieldID(cls, name, sig, srgName, notchName, notchSig);
    if (!fid)
      return nullptr;
    JNIEnv *env = getEnv();
    return env ? env->GetStaticObjectField(cls, fid) : nullptr;
  }

  jobject GetObjectField(jobject obj, const char *name, const char *sig,
                         const char *srgName = nullptr,
                         const char *notchName = nullptr,
                         const char *notchSig = nullptr) {
    if (!obj)
      return nullptr;
    JNIEnv *env = getEnv();
    if (!env)
      return nullptr;
    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = GetFieldID(cls, name, sig, srgName, notchName, notchSig);
    env->DeleteLocalRef(cls);
    if (!fid)
      return nullptr;
    return env->GetObjectField(obj, fid);
  }

  double GetDoubleField(jobject obj, const char *name,
                        const char *srgName = nullptr,
                        const char *notchName = nullptr) {
    if (!obj)
      return 0.0;
    JNIEnv *env = getEnv();
    if (!env)
      return 0.0;
    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = GetFieldID(cls, name, "D", srgName, notchName);
    env->DeleteLocalRef(cls);
    return fid ? env->GetDoubleField(obj, fid) : 0.0;
  }

  int GetIntField(jobject obj, const char *name, const char *srgName = nullptr,
                  const char *notchName = nullptr) {
    if (!obj)
      return 0;
    JNIEnv *env = getEnv();
    if (!env)
      return 0;
    jclass cls = env->GetObjectClass(obj);
    jfieldID fid = GetFieldID(cls, name, "I", srgName, notchName);
    env->DeleteLocalRef(cls);
    return fid ? env->GetIntField(obj, fid) : 0;
  }

  int CallStaticIntMethod(jclass cls, jmethodID mid, ...) {
    if (!cls || !mid)
      return 0;
    JNIEnv *env = getEnv();
    if (!env)
      return 0;
    va_list args;
    va_start(args, mid);
    int res = env->CallStaticIntMethodV(cls, mid, args);
    va_end(args);
    return res;
  }

  int CallIntMethod(jobject obj, jmethodID mid, ...) {
    if (!obj || !mid)
      return 0;
    JNIEnv *env = getEnv();
    if (!env)
      return 0;
    va_list args;
    va_start(args, mid);
    int res = env->CallIntMethodV(obj, mid, args);
    va_end(args);
    return res;
  }

  bool CallBooleanMethod(jobject obj, jmethodID mid, ...) {
    if (!obj || !mid)
      return false;
    JNIEnv *env = getEnv();
    if (!env)
      return false;
    va_list args;
    va_start(args, mid);
    bool res = env->CallBooleanMethodV(obj, mid, args);
    va_end(args);
    return res;
  }

  bool CheckException() {
    JNIEnv *env = getEnv();
    if (env && env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return true;
    }
    return false;
  }

  void Cleanup() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (classes.empty())
      return;

    JNIEnv *env = getEnv();
    if (!env) {
      classes.clear();
      return;
    }

    for (auto &pair : classes) {
      if (pair.second)
        env->DeleteGlobalRef(pair.second);
    }
    classes.clear();
  }

private:
  std::unordered_map<std::string, jclass> classes;
  std::recursive_mutex m_mutex;
};

#define lc (Lunar::getInstance())
#define g_cleaningUp (Lunar::isCleaningUp())