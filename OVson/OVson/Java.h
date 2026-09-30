#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
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

  static jstring createSafeJString(JNIEnv *env, const std::string &str) {
    if (!env)
      return nullptr;
    if (str.empty()) {
      static const jchar empty[1] = {0};
      return env->NewString(empty, 0);
    }

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(),
                                   (int)str.size(), nullptr, 0);
    if (wlen > 0) {
      std::wstring wstr(wlen, L'\0');
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0],
                          wlen);
      return env->NewString(reinterpret_cast<const jchar *>(wstr.data()),
                            (jsize)wstr.size());
    }

    wlen = MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), nullptr, 0);
    if (wlen > 0) {
      std::wstring wstr(wlen, L'\0');
      MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.size(), &wstr[0],
                          wlen);
      return env->NewString(reinterpret_cast<const jchar *>(wstr.data()),
                            (jsize)wstr.size());
    }

    std::wstring safeW;
    safeW.reserve(str.size());
    for (unsigned char c : str) {
      safeW.push_back((c < 128) ? (wchar_t)c : L'?');
    }
    return env->NewString(reinterpret_cast<const jchar *>(safeW.data()),
                          (jsize)safeW.size());
  }

  static Lunar *getInstance() {
    static Lunar *instance = new Lunar();
    return instance;
  }

  static void resetGlStateManagerTexture(JNIEnv *env) {
    if (!env) return;
    Lunar *l = getInstance();
    if (!l) return;
    static jclass s_glCls = nullptr;
    static jmethodID s_bindTex = nullptr;
    if (!s_glCls) {
      jclass cls = l->GetClass("net.minecraft.client.renderer.GlStateManager");
      if (!cls) cls = env->FindClass("bfl");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (cls) {
        s_glCls = (jclass)env->NewGlobalRef(cls);
        s_bindTex = l->GetStaticMethodID(s_glCls, "bindTexture", "(I)V", "func_179144_i", "p");
        if (!s_bindTex) {
          s_bindTex = l->FindMethodBySignature(s_glCls, "(I)V", true);
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }
    }
    if (s_glCls && s_bindTex) {
      env->CallStaticVoidMethod(s_glCls, s_bindTex, (jint)0);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }

  static bool isSGAFontRenderer(JNIEnv *env, jobject fr) {
    if (!env || !fr) return false;
    jclass frCls = env->GetObjectClass(fr);
    if (!frCls) return false;

    Lunar *l = getInstance();
    const char *rlFields[] = {"locationFontTexture", "field_111273_g", "f", "g"};
    const char *rlSigs[] = {"Lnet/minecraft/util/ResourceLocation;", "Ljy;"};
    jfieldID f_loc = nullptr;
    for (const char *fn : rlFields) {
      for (const char *fs : rlSigs) {
        f_loc = env->GetFieldID(frCls, fn, fs);
        if (f_loc) break;
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
      if (f_loc) break;
    }
    if (!f_loc && l && l->jvmti) {
      f_loc = l->FindFieldBySignature(frCls, "Ljy;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!f_loc) {
        f_loc = l->FindFieldBySignature(frCls, "Lnet/minecraft/util/ResourceLocation;");
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }

    bool isSga = false;
    if (f_loc) {
      jobject loc = env->GetObjectField(fr, f_loc);
      if (loc) {
        jclass locCls = env->GetObjectClass(loc);
        const char *pathFields[] = {"resourcePath", "field_110626_a", "b"};
        jfieldID f_path = nullptr;
        for (const char *pn : pathFields) {
          f_path = env->GetFieldID(locCls, pn, "Ljava/lang/String;");
          if (f_path) break;
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (!f_path && l) {
          f_path = l->FindFieldBySignature(locCls, "Ljava/lang/String;");
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (f_path) {
          jstring jpath = (jstring)env->GetObjectField(loc, f_path);
          if (jpath) {
            const char *c = env->GetStringUTFChars(jpath, nullptr);
            if (c) {
              std::string p = c;
              for (char &ch : p) ch = (char)::tolower((unsigned char)ch);
              if (p.find("sga") != std::string::npos ||
                  p.find("enchant") != std::string::npos ||
                  p.find("galactic") != std::string::npos) {
                isSga = true;
              }
              env->ReleaseStringUTFChars(jpath, c);
            }
            env->DeleteLocalRef(jpath);
          }
        }
        // Also check toString() as backup
        if (!isSga && !f_path) {
          jmethodID m_toString = env->GetMethodID(locCls, "toString", "()Ljava/lang/String;");
          if (m_toString) {
            jstring jstr = (jstring)env->CallObjectMethod(loc, m_toString);
            if (jstr) {
              const char *c = env->GetStringUTFChars(jstr, nullptr);
              if (c) {
                std::string s = c;
                for (char &ch : s) ch = (char)::tolower((unsigned char)ch);
                if (s.find("sga") != std::string::npos ||
                    s.find("enchant") != std::string::npos ||
                    s.find("galactic") != std::string::npos) {
                  isSga = true;
                }
                env->ReleaseStringUTFChars(jstr, c);
              }
              env->DeleteLocalRef(jstr);
            }
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        env->DeleteLocalRef(locCls);
        env->DeleteLocalRef(loc);
      }
    }
    env->DeleteLocalRef(frCls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return isSga;
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

    const auto& nmap = getNotchMap();
    for (const auto& pair : nmap) {
      auto it = classes.find(pair.second);
      if (it != classes.end() && classes.find(pair.first) == classes.end()) {
        classes[pair.first] = it->second;
      }
    }

    GetClass("java.util.Collection");
    GetClass("java.util.Iterator");
    GetClass("net.minecraft.client.Minecraft");
    GetClass("net.minecraft.client.network.NetworkPlayerInfo");
    GetClass("net.minecraft.util.ChatComponentText");
    GetClass("net.minecraft.client.gui.GuiIngame");
    GetClass("net.minecraft.client.gui.GuiPlayerTabOverlay");
    GetClass("net.minecraft.util.IChatComponent");
    GetClass("net.minecraft.util.IChatComponent$Serializer");
    GetClass("net.minecraft.client.gui.GuiChat");
    GetClass("net.minecraft.client.gui.GuiScreen");
    GetClass("net.minecraft.event.HoverEvent$Action");
    GetClass("net.minecraft.client.gui.GuiTextField");
    GetClass("net.minecraft.client.renderer.ActiveRenderInfo");
    GetClass("net.minecraft.client.renderer.entity.RenderManager");
    GetClass("net.minecraft.entity.Entity");
    GetClass("net.minecraft.client.renderer.EntityRenderer");
    GetClass("net.minecraft.client.settings.GameSettings");
    GetClass("net.minecraft.util.Timer");
  }

  virtual ~Lunar() { Cleanup(); }

  static const std::unordered_map<std::string, std::string>& getNotchMap() {
    static const std::unordered_map<std::string, std::string> s_notchMap = {
        {"net.minecraft.client.Minecraft", "ave"},
        {"net.minecraft.client.entity.EntityPlayerSP", "bew"},
        {"net.minecraft.client.entity.EntityOtherPlayerMP", "bex"},
        {"net.minecraft.client.entity.AbstractClientPlayer", "bet"},
        {"net.minecraft.entity.player.EntityPlayer", "wn"},
        {"net.minecraft.entity.EntityLivingBase", "pr"},
        {"net.minecraft.entity.Entity", "pk"},
        {"net.minecraft.entity.player.InventoryPlayer", "wm"},
        {"net.minecraft.entity.projectile.EntityArrow", "wq"},
        {"net.minecraft.entity.item.EntityItem", "uz"},
        {"net.minecraft.client.gui.GuiIngame", "avo"},
        {"net.minecraft.client.gui.GuiNewChat", "avt"},
        {"net.minecraft.client.gui.GuiChat", "awv"},
        {"net.minecraft.client.gui.GuiScreen", "axu"},
        {"net.minecraft.client.gui.GuiTextField", "avw"},
        {"net.minecraft.client.gui.GuiPlayerTabOverlay", "awh"},
        {"net.minecraft.client.gui.FontRenderer", "avn"},
        {"net.minecraft.client.gui.ScaledResolution", "avr"},
        {"net.minecraft.client.gui.inventory.GuiContainer", "ayl"},
        {"net.minecraft.inventory.Container", "xi"},
        {"net.minecraft.inventory.Slot", "yg"},
        {"net.minecraft.client.multiplayer.WorldClient", "bdb"},
        {"net.minecraft.world.World", "adm"},
        {"net.minecraft.world.chunk.Chunk", "amy"},
        {"net.minecraft.world.chunk.storage.ExtendedBlockStorage", "amz"},
        {"net.minecraft.client.network.NetHandlerPlayClient", "bcy"},
        {"net.minecraft.client.network.NetworkPlayerInfo", "bdc"},
        {"net.minecraft.network.NetworkManager", "ej"},
        {"net.minecraft.client.multiplayer.ServerData", "bde"},
        {"net.minecraft.client.multiplayer.PlayerControllerMP", "bda"},
        {"net.minecraft.scoreboard.Scoreboard", "auo"},
        {"net.minecraft.scoreboard.ScorePlayerTeam", "aul"},
        {"net.minecraft.scoreboard.Score", "aum"},
        {"net.minecraft.scoreboard.ScoreObjective", "auk"},
        {"net.minecraft.scoreboard.Team", "auq"},
        {"net.minecraft.scoreboard.GoalColor", "aur"},
        {"net.minecraft.scoreboard.ScoreDummyCriteria", "aus"},
        {"net.minecraft.scoreboard.ScoreHealthCriteria", "aut"},
        {"net.minecraft.scoreboard.IScoreObjectiveCriteria", "auu"},
        {"net.minecraft.scoreboard.ServerScoreboard", "kk"},
        {"net.minecraft.client.renderer.entity.RenderManager", "biu"},
        {"net.minecraft.client.renderer.entity.RendererLivingEntity", "bjl"},
        {"net.minecraft.client.renderer.RenderGlobal", "bfr"},
        {"net.minecraft.client.renderer.EntityRenderer", "bfb"},
        {"net.minecraft.client.renderer.ActiveRenderInfo", "axs"},
        {"net.minecraft.client.renderer.GlStateManager", "bfl"},
        {"net.minecraft.client.renderer.Tessellator", "bfx"},
        {"net.minecraft.client.renderer.WorldRenderer", "bfd"},
        {"net.minecraft.client.renderer.texture.TextureMap", "bmh"},
        {"net.minecraft.client.renderer.texture.TextureManager", "bmj"},
        {"net.minecraft.client.renderer.texture.TextureAtlasSprite", "bmi"},
        {"net.minecraft.client.renderer.BlockRendererDispatcher", "bgd"},
        {"net.minecraft.client.renderer.BlockModelShapes", "bgc"},
        {"net.minecraft.client.settings.GameSettings", "avh"},
        {"net.minecraft.client.settings.KeyBinding", "avb"},
        {"net.minecraft.util.Timer", "avl"},
        {"net.minecraft.util.IChatComponent", "eu"},
        {"net.minecraft.util.IChatComponent$Serializer", "eu$a"},
        {"net.minecraft.util.ChatComponentText", "fa"},
        {"net.minecraft.util.ChatStyle", "ez"},
        {"net.minecraft.util.MovingObjectPosition", "auh"},
        {"net.minecraft.util.AxisAlignedBB", "aug"},
        {"net.minecraft.util.BlockPos", "cj"},
        {"net.minecraft.util.EnumFacing", "cq"},
        {"net.minecraft.util.ResourceLocation", "jy"},
        {"net.minecraft.util.RegistryNamespacedDefaultedByKey", "co"},
        {"net.minecraft.block.state.IBlockState", "alz"},
        {"net.minecraft.block.Block", "afh"},
        {"net.minecraft.block.BlockBed", "afg"},
        {"net.minecraft.item.ItemStack", "zx"},
        {"net.minecraft.item.ItemArmor", "yv"},
        {"net.minecraft.item.Item", "zw"},
        {"net.minecraft.item.ItemBow", "zp"},
        {"net.minecraft.event.HoverEvent", "ew"},
        {"net.minecraft.event.HoverEvent$Action", "ew$a"},
        {"net.minecraft.network.play.server.S38PacketPlayerListItem", "ja"},
        {"net.minecraft.network.play.server.S0CPacketSpawnPlayer", "ik"},
        {"net.minecraft.network.play.server.S1CPacketEntityMetadata", "id"},
        {"net.minecraft.network.play.server.S3EPacketTeams", "hr"},
    };
    return s_notchMap;
  }

  static std::string translateSigToNotch(const std::string& sig) {
    if (sig.find("net/minecraft") == std::string::npos) return sig;
    std::string result;
    result.reserve(sig.size());
    const auto& nmap = getNotchMap();
    size_t i = 0;
    while (i < sig.size()) {
      if (sig[i] == 'L') {
        size_t semi = sig.find(';', i + 1);
        if (semi != std::string::npos) {
          std::string className = sig.substr(i + 1, semi - (i + 1));
          std::string dotClass = className;
          for (auto& c : dotClass) if (c == '/') c = '.';
          auto it = nmap.find(dotClass);
          if (it != nmap.end()) {
            result += 'L';
            result += it->second;
            result += ';';
            i = semi + 1;
            continue;
          }
        }
      }
      result += sig[i];
      i++;
    }
    return result;
  }

  jclass GetClass(const std::string &className) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    JNIEnv *env = getEnv();
    if (!env)
      return nullptr;
    auto it = classes.find(className);
    if (it != classes.end())
      return it->second;

    const auto& nmap = getNotchMap();

    auto nit = nmap.find(className);
    if (nit != nmap.end()) {
      auto itNotch = classes.find(nit->second);
      if (itNotch != classes.end()) {
        classes[className] = itNotch->second;
        return itNotch->second;
      }
    }

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

    if (nit != nmap.end()) {
      localCls = env->FindClass(nit->second.c_str());
      if (localCls) {
        jclass globalCls = (jclass)env->NewGlobalRef(localCls);
        classes[className] = globalCls;
        classes[nit->second] = globalCls;
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
    std::string autoNotchSig;
    const char* effectiveNotchSig = notchSig;
    if (!effectiveNotchSig && sig) {
      autoNotchSig = translateSigToNotch(sig);
      if (autoNotchSig != sig) {
        effectiveNotchSig = autoNotchSig.c_str();
      }
    }
    if (!fid && srgName && effectiveNotchSig) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetFieldID(cls, srgName, effectiveNotchSig);
    }
    if (!fid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetFieldID(cls, notchName, effectiveNotchSig ? effectiveNotchSig : sig);
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
    std::string autoNotchSig;
    const char* effectiveNotchSig = notchSig;
    if (!effectiveNotchSig && sig) {
      autoNotchSig = translateSigToNotch(sig);
      if (autoNotchSig != sig) {
        effectiveNotchSig = autoNotchSig.c_str();
      }
    }
    if (!fid && srgName && effectiveNotchSig) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetStaticFieldID(cls, srgName, effectiveNotchSig);
    }
    if (!fid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      fid = env->GetStaticFieldID(cls, notchName, effectiveNotchSig ? effectiveNotchSig : sig);
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
    std::string autoNotchSig;
    const char* effectiveNotchSig = notchSig;
    if (!effectiveNotchSig && sig) {
      autoNotchSig = translateSigToNotch(sig);
      if (autoNotchSig != sig) {
        effectiveNotchSig = autoNotchSig.c_str();
      }
    }
    if (!mid && srgName && effectiveNotchSig) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetMethodID(cls, srgName, effectiveNotchSig);
    }
    if (!mid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetMethodID(cls, notchName, effectiveNotchSig ? effectiveNotchSig : sig);
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
    std::string autoNotchSig;
    const char* effectiveNotchSig = notchSig;
    if (!effectiveNotchSig && sig) {
      autoNotchSig = translateSigToNotch(sig);
      if (autoNotchSig != sig) {
        effectiveNotchSig = autoNotchSig.c_str();
      }
    }
    if (!mid && srgName && effectiveNotchSig) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetStaticMethodID(cls, srgName, effectiveNotchSig);
    }
    if (!mid && notchName) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      mid = env->GetStaticMethodID(cls, notchName, effectiveNotchSig ? effectiveNotchSig : sig);
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
    classes.clear();
  }

private:
  std::unordered_map<std::string, jclass> classes;
  std::recursive_mutex m_mutex;
};

#define lc (Lunar::getInstance())
#define g_cleaningUp (Lunar::isCleaningUp())
