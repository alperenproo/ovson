#include "BetterTab.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Java.h"
#include "../Logic/StatsTracker.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/GlGuard.h"
#include "../Utils/Logger.h"
#include "../Utils/Timer.h"
#include "../Utils/Anticheat/Anticheat.h"
#include <GL/gl.h>
#include <algorithm>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace BetterTab {

struct JCache {
  bool initialized = false;
  bool fontMethodsReady = false;
  jclass mcCls = nullptr;
  jfieldID f_theMc = nullptr;
  jfieldID f_gameSettings = nullptr;
  jfieldID f_guiScale = nullptr;
  jfieldID f_fontRenderer = nullptr;
  jmethodID m_drawString = nullptr;
  jmethodID m_getStringWidth = nullptr;
  jfieldID f_theWorld = nullptr;
  jmethodID m_getScoreboard = nullptr;
  jmethodID m_getObjectiveInDisplaySlot = nullptr;
  jmethodID m_getValueFromObjective = nullptr;
  jmethodID m_getScorePoints = nullptr;
  jmethodID m_getNet = nullptr;
  jmethodID m_getPlayerInfoMap = nullptr;
  jmethodID m_getGameProfile = nullptr;
  jmethodID m_getName = nullptr;
  jfieldID f_responseTime = nullptr;
  jmethodID m_getResponseTime = nullptr;
  jclass rlCls = nullptr;
  jfieldID f_locationSkin = nullptr;
  jmethodID m_getLocationSkin = nullptr;
  jfieldID f_renderEngine = nullptr;
  jmethodID m_bindTexture = nullptr;
  jmethodID m_tm_getTexture = nullptr;
  jmethodID m_tex_getGlTextureId = nullptr;
  jmethodID m_object_toString = nullptr;

  jclass collCls = nullptr;
  jmethodID m_iterator = nullptr;
  jclass iterCls = nullptr;
  jmethodID m_hasNext = nullptr;
  jmethodID m_next = nullptr;
};
static JCache g_jc;

static std::string resolveLogPath() {
  char tmp[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", tmp, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    return std::string(tmp) + "\\ov_bettertab_debug.txt";
  }
  n = GetEnvironmentVariableA("USERPROFILE", tmp, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    return std::string(tmp) + "\\ov_bettertab_debug.txt";
  }
  return "ov_bettertab_debug.txt";
}

static void logDiagnostic(const char *fmt, ...) {
  (void)fmt;
  return;
  static FILE *f = nullptr;
  static std::mutex logMtx;
  static std::string s_path;
  std::lock_guard<std::mutex> lock(logMtx);
  if (!f) {
    s_path = resolveLogPath();
    fopen_s(&f, s_path.c_str(), "a");
    if (f) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      fprintf(f,
              "\n[%04d-%02d-%02d %02d:%02d:%02d] ===== Better Tab session "
              "start (pid=%lu, path=%s) =====\n",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
              GetCurrentProcessId(), s_path.c_str());
      fflush(f);
    }
  }
  if (!f)
    return;

  SYSTEMTIME st;
  GetLocalTime(&st);
  fprintf(f, "[%02d:%02d:%02d.%03d t=%lu] ", st.wHour, st.wMinute, st.wSecond,
          st.wMilliseconds, GetCurrentThreadId());

  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
  fprintf(f, "\n");
  fflush(f);
}

static void ensureFontMethods(JNIEnv *env) {
  if (g_jc.fontMethodsReady)
    return;
  jclass fontCls = lc->GetClass("net.minecraft.client.gui.FontRenderer");
  if (!fontCls) {
    fontCls = env->FindClass("avn");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (fontCls) {
    g_jc.m_drawString =
        lc->GetMethodID(fontCls, "drawStringWithShadow",
                        "(Ljava/lang/String;FFI)I", "func_175063_a", "a");
    if (!g_jc.m_drawString) {
      g_jc.m_drawString =
          lc->FindMethodBySignature(fontCls, "(Ljava/lang/String;FFI)I");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    g_jc.m_getStringWidth =
        lc->GetMethodID(fontCls, "getStringWidth", "(Ljava/lang/String;)I",
                        "func_78256_a", "a");
    if (!g_jc.m_getStringWidth) {
      g_jc.m_getStringWidth =
          lc->FindMethodBySignature(fontCls, "(Ljava/lang/String;)I");
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }
    if (g_jc.m_drawString)
      logDiagnostic("Found drawStringWithShadow");
    else
      logDiagnostic("drawStringWithShadow NOT found!");
    if (g_jc.m_getStringWidth)
      logDiagnostic("Found getStringWidth");
    else
      logDiagnostic("getStringWidth NOT found!");
  }
  g_jc.fontMethodsReady = true;
}

static void ensureMcFields(JNIEnv *env) {
  if (g_jc.initialized)
    return;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) {
    mcCls = env->FindClass("ave");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (!mcCls)
    return;

  g_jc.mcCls = (jclass)env->NewGlobalRef(mcCls);
  g_jc.f_theMc = lc->GetStaticFieldID(g_jc.mcCls, "theMinecraft",
                                      "Lnet/minecraft/client/Minecraft;",
                                      "field_71432_P", "S", "Lave;");
  g_jc.f_gameSettings =
      lc->GetFieldID(g_jc.mcCls, "gameSettings",
                     "Lnet/minecraft/client/settings/GameSettings;",
                     "field_71474_y", "t", "Lavh;");
  g_jc.f_fontRenderer = lc->GetFieldID(
      g_jc.mcCls, "fontRendererObj", "Lnet/minecraft/client/gui/FontRenderer;",
      "field_71466_p", "q", "Lavn;");
  if (g_jc.f_fontRenderer)
    logDiagnostic("Found fontRendererObj");
  else
    logDiagnostic("fontRendererObj NOT found, trying signature...");
  if (!g_jc.f_fontRenderer)
    g_jc.f_fontRenderer = lc->FindFieldBySignature(
        g_jc.mcCls, "Lnet/minecraft/client/gui/FontRenderer;");
  if (!g_jc.f_fontRenderer)
    g_jc.f_fontRenderer = lc->FindFieldBySignature(g_jc.mcCls, "Lavn;");
  if (g_jc.f_fontRenderer)
    logDiagnostic("fontRendererObj resolved via signature");

  g_jc.f_theWorld = lc->GetFieldID(
      g_jc.mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
      "field_71441_e", "f", "Lbdb;");

  jclass worldCls =
      lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
  if (!worldCls) {
    worldCls = env->FindClass("bdb");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (worldCls) {
    g_jc.m_getScoreboard = lc->GetMethodID(
        worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;",
        "func_96441_U", "Z", "()Lauo;");
  }

  jclass sbCls = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
  if (!sbCls) {
    sbCls = env->FindClass("auo");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (sbCls) {
    g_jc.m_getObjectiveInDisplaySlot =
        lc->GetMethodID(sbCls, "getObjectiveInDisplaySlot",
                        "(I)Lnet/minecraft/scoreboard/ScoreObjective;",
                        "func_96539_a", "a", "(I)Lauk;");
    g_jc.m_getValueFromObjective =
        lc->GetMethodID(sbCls, "getValueFromObjective",
                        "(Ljava/lang/String;Lnet/minecraft/scoreboard/"
                        "ScoreObjective;)Lnet/minecraft/scoreboard/Score;",
                        "func_96529_a", "c", "(Ljava/lang/String;Lauk;)Laum;");
  }

  jclass scoreCls = lc->GetClass("net.minecraft.scoreboard.Score");
  if (!scoreCls) {
    scoreCls = env->FindClass("aum");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (scoreCls) {
    g_jc.m_getScorePoints =
        lc->GetMethodID(scoreCls, "getScorePoints", "()I", "func_96652_c", "c");
  }

  g_jc.m_getNet =
      lc->GetMethodID(g_jc.mcCls, "getNetHandler",
                      "()Lnet/minecraft/client/network/NetHandlerPlayClient;",
                      "func_147114_u", "ay", "()Lbcy;");
  if (!g_jc.m_getNet) {
    env->ExceptionClear();
    g_jc.m_getNet = lc->FindMethodBySignature(
        g_jc.mcCls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
  }
  if (!g_jc.m_getNet) {
    env->ExceptionClear();
    g_jc.m_getNet = lc->FindMethodBySignature(g_jc.mcCls, "()Lbcy;");
  }
  if (g_jc.m_getNet)
    logDiagnostic("Found getNetHandler");
  else
    logDiagnostic("getNetHandler NOT found!");

  jclass nhCls =
      lc->GetClass("net.minecraft.client.network.NetHandlerPlayClient");
  if (!nhCls) {
    nhCls = env->FindClass("bcy");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (nhCls) {
    g_jc.m_getPlayerInfoMap =
        lc->GetMethodID(nhCls, "getPlayerInfoMap", "()Ljava/util/Collection;",
                        "func_175106_d", "d", "()Ljava/util/Collection;");
    if (g_jc.m_getPlayerInfoMap)
      logDiagnostic("Found getPlayerInfoMap");
    else
      logDiagnostic("getPlayerInfoMap NOT found!");
  }

  if (g_jc.f_gameSettings) {
    jobject dummyMc = env->GetStaticObjectField(g_jc.mcCls, g_jc.f_theMc);
    if (dummyMc) {
      jobject dummyGs = env->GetObjectField(dummyMc, g_jc.f_gameSettings);
      if (dummyGs) {
        jclass gsCls = env->GetObjectClass(dummyGs);
        g_jc.f_guiScale =
            lc->GetFieldID(gsCls, "guiScale", "I", "field_71454_cg", "aB");
        if (!g_jc.f_guiScale) {
          env->ExceptionClear();
          g_jc.f_guiScale = env->GetFieldID(gsCls, "aN", "I");
        }
        if (!g_jc.f_guiScale) {
          env->ExceptionClear();
          g_jc.f_guiScale = env->GetFieldID(gsCls, "aM", "I");
        }
        if (!g_jc.f_guiScale) {
          env->ExceptionClear();
          g_jc.f_guiScale = env->GetFieldID(gsCls, "bc", "I");
        }

        if (g_jc.f_guiScale)
          logDiagnostic("Found guiScale field");
        else
          logDiagnostic("guiScale field NOT found!");

        env->DeleteLocalRef(gsCls);
        env->DeleteLocalRef(dummyGs);
      }
      env->DeleteLocalRef(dummyMc);
    }
  }

  if (env->ExceptionCheck())
    env->ExceptionClear();
  logDiagnostic("Partial mapping complete, continuing...");
  jclass npiCls =
      lc->GetClass("net.minecraft.client.network.NetworkPlayerInfo");
  if (!npiCls) {
    npiCls = env->FindClass("bdc");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (npiCls) {
    g_jc.m_getGameProfile = lc->GetMethodID(
        npiCls, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
        "func_178845_a", "a", "()Lcom/mojang/authlib/GameProfile;");
    g_jc.m_getLocationSkin = lc->GetMethodID(
        npiCls, "getLocationSkin", "()Lnet/minecraft/util/ResourceLocation;",
        "func_178837_g", "g", "()Ljy;");
    g_jc.f_locationSkin = lc->GetFieldID(
        npiCls, "locationSkin", "Lnet/minecraft/util/ResourceLocation;",
        "field_178862_f", "a", "Ljy;");
    if (!g_jc.f_locationSkin) {
      env->ExceptionClear();
      g_jc.f_locationSkin = lc->FindFieldBySignature(
          npiCls, "Lnet/minecraft/util/ResourceLocation;");
    }
    if (!g_jc.f_locationSkin) {
      env->ExceptionClear();
      g_jc.f_locationSkin = lc->FindFieldBySignature(npiCls, "Ljy;");
    }
    if (!g_jc.f_locationSkin) {
      env->ExceptionClear();
      g_jc.f_locationSkin = lc->GetFieldID(npiCls, "l", "Ljy;");
    }

    g_jc.f_responseTime =
        lc->GetFieldID(npiCls, "responseTime", "I", "field_178867_h", "h", "I");
    g_jc.m_getResponseTime = lc->GetMethodID(npiCls, "getResponseTime", "()I",
                                             "func_178835_l", nullptr);
    if (env->ExceptionCheck())
      env->ExceptionClear();

    if (g_jc.m_getGameProfile)
      logDiagnostic("Found getGameProfile");
    else
      logDiagnostic("getGameProfile NOT found!");
    if (g_jc.f_responseTime)
      logDiagnostic("Found responseTime field");
    else
      logDiagnostic("responseTime field NOT found!");
  }

  jclass gpCls = lc->GetClass("com.mojang.authlib.GameProfile");
  if (!gpCls) {
    gpCls = env->FindClass("com/mojang/authlib/GameProfile");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (gpCls) {
    g_jc.m_getName = lc->GetMethodID(gpCls, "getName", "()Ljava/lang/String;");
    if (g_jc.m_getName)
      logDiagnostic("Found GameProfile.getName");
    else
      logDiagnostic("GameProfile.getName NOT found!");
  }

  jclass rlClass = lc->GetClass("net.minecraft.util.ResourceLocation");
  if (!rlClass) {
    rlClass = env->FindClass("jy");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (rlClass) {
    g_jc.rlCls = (jclass)env->NewGlobalRef(rlClass);
  }

  g_jc.f_renderEngine =
      lc->GetFieldID(g_jc.mcCls, "renderEngine",
                     "Lnet/minecraft/client/renderer/texture/TextureManager;",
                     "field_71446_z", "P", "Lbmj;");
  if (!g_jc.f_renderEngine) {
    env->ExceptionClear();
    g_jc.f_renderEngine = lc->FindFieldBySignature(
        g_jc.mcCls, "Lnet/minecraft/client/renderer/texture/TextureManager;");
  }
  if (!g_jc.f_renderEngine) {
    env->ExceptionClear();
    g_jc.f_renderEngine = lc->FindFieldBySignature(g_jc.mcCls, "Lbmj;");
  }

  jclass tmCls =
      lc->GetClass("net.minecraft.client.renderer.texture.TextureManager");
  if (!tmCls) {
    tmCls = env->FindClass("bmj");
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  if (tmCls) {
    g_jc.m_bindTexture = lc->GetMethodID(
        tmCls, "bindTexture", "(Lnet/minecraft/util/ResourceLocation;)V",
        "func_110577_a", "a", "(Ljy;)V");
    g_jc.m_tm_getTexture =
        lc->GetMethodID(tmCls, "getTexture",
                        "(Lnet/minecraft/util/ResourceLocation;)Lnet/minecraft/"
                        "client/renderer/texture/ITextureObject;",
                        "func_110581_b", "b");
    if (!g_jc.m_tm_getTexture) {
      env->ExceptionClear();
      g_jc.m_tm_getTexture = lc->FindMethodBySignature(
          tmCls, "(Lnet/minecraft/util/ResourceLocation;)Lnet/minecraft/client/"
                 "renderer/texture/ITextureObject;");
    }
    if (!g_jc.m_tm_getTexture) {
      env->ExceptionClear();
      g_jc.m_tm_getTexture = lc->FindMethodBySignature(tmCls, "(Ljy;)Lblz;");
    }
  }

  if (!g_jc.m_object_toString) {
    jclass objCls = env->FindClass("java/lang/Object");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (objCls) {
      g_jc.m_object_toString =
          env->GetMethodID(objCls, "toString", "()Ljava/lang/String;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      env->DeleteLocalRef(objCls);
    }
  }

  logDiagnostic("SKIN init: npiCls=%p tmCls=%p rlCls=%p", (void *)npiCls,
                (void *)tmCls, (void *)rlClass);
  logDiagnostic("SKIN init: f_locationSkin=%p m_getLocationSkin=%p "
                "f_renderEngine=%p m_bindTexture=%p m_tm_getTexture=%p",
                (void *)g_jc.f_locationSkin, (void *)g_jc.m_getLocationSkin,
                (void *)g_jc.f_renderEngine, (void *)g_jc.m_bindTexture,
                (void *)g_jc.m_tm_getTexture);

  jclass collCls = lc->GetClass("java.util.Collection");
  if (!collCls)
    collCls = env->FindClass("java/util/Collection");
  if (collCls) {
    g_jc.collCls = (jclass)env->NewGlobalRef(collCls);
    g_jc.m_iterator =
        env->GetMethodID(g_jc.collCls, "iterator", "()Ljava/util/Iterator;");
    if (env->ExceptionCheck())
      env->ExceptionClear();
    if (g_jc.m_iterator)
      logDiagnostic("Found Collection.iterator");
    else
      logDiagnostic("Collection.iterator NOT found!");
  }

  jclass iterCls = lc->GetClass("java.util.Iterator");
  if (!iterCls)
    iterCls = env->FindClass("java/util/Iterator");
  if (iterCls) {
    g_jc.iterCls = (jclass)env->NewGlobalRef(iterCls);
    g_jc.m_hasNext = env->GetMethodID(g_jc.iterCls, "hasNext", "()Z");
    if (env->ExceptionCheck())
      env->ExceptionClear();
    g_jc.m_next =
        env->GetMethodID(g_jc.iterCls, "next", "()Ljava/lang/Object;");
    if (env->ExceptionCheck())
      env->ExceptionClear();

    if (g_jc.m_hasNext)
      logDiagnostic("Found Iterator.hasNext");
    else
      logDiagnostic("Iterator.hasNext NOT found!");
    if (g_jc.m_next)
      logDiagnostic("Found Iterator.next");
    else
      logDiagnostic("Iterator.next NOT found!");
  }

  if (env->ExceptionCheck())
    env->ExceptionClear();
  g_jc.initialized = true;
  logDiagnostic("All mappings initialized successfully.");
}

void init() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  ensureFontMethods(env);
  ensureMcFields(env);
}

struct RenderCtx {
  JNIEnv *env = nullptr;
  jobject mc = nullptr;           // local ref
  jobject fontRenderer = nullptr; // local ref
  int guiScale = 0;
};

static void drawString(RenderCtx &ctx, const std::string &text, float x,
                       float y, uint32_t color) {
  if (!ctx.env || !ctx.fontRenderer || !g_jc.m_drawString || text.empty())
    return;
  jstring jt = ctx.env->NewStringUTF(text.c_str());
  if (!jt)
    return;
  ctx.env->CallIntMethod(ctx.fontRenderer, g_jc.m_drawString, jt, x, y,
                         (jint)color);
  if (ctx.env->ExceptionCheck())
    ctx.env->ExceptionClear();
  ctx.env->DeleteLocalRef(jt);
}

namespace {
constexpr size_t kWidthCacheMax = 1024;
std::list<std::string> g_widthLru;
std::unordered_map<std::string,
                   std::pair<int, std::list<std::string>::iterator>>
    g_widthCache;
} // namespace

static int measure(RenderCtx &ctx, const std::string &text) {
  if (!ctx.env || !ctx.fontRenderer || !g_jc.m_getStringWidth || text.empty())
    return 0;

  auto cached = g_widthCache.find(text);
  if (cached != g_widthCache.end()) {
    g_widthLru.splice(g_widthLru.begin(), g_widthLru, cached->second.second);
    return cached->second.first;
  }

  jstring jt = ctx.env->NewStringUTF(text.c_str());
  if (!jt)
    return 0;
  int w = ctx.env->CallIntMethod(ctx.fontRenderer, g_jc.m_getStringWidth, jt);
  if (ctx.env->ExceptionCheck())
    ctx.env->ExceptionClear();
  ctx.env->DeleteLocalRef(jt);

  if (g_widthCache.size() >= kWidthCacheMax) {
    while (g_widthCache.size() >= kWidthCacheMax && !g_widthLru.empty()) {
      g_widthCache.erase(g_widthLru.back());
      g_widthLru.pop_back();
    }
  }
  g_widthLru.push_front(text);
  g_widthCache.emplace(text, std::make_pair(w, g_widthLru.begin()));
  return w;
}

static std::string fmt2(double v) {
  int i = (int)(v * 100.0 + 0.5);
  return std::to_string(i / 100) + "." + (i % 100 < 10 ? "0" : "") + std::to_string(i % 100);
}

static std::string fmtCommas(long long n) {
  std::string s = std::to_string(n);
  int res = (int)s.length() - 3;
  while (res > 0) {
    s.insert(res, ",");
    res -= 3;
  }
  return s;
}

static uint32_t teamColorToRGB(const std::string &team) {
  if (team == "Red")
    return 0xFFFF5555;
  if (team == "Blue")
    return 0xFF5555FF;
  if (team == "Green")
    return 0xFF55FF55;
  if (team == "Yellow")
    return 0xFFFFFF55;
  if (team == "Aqua")
    return 0xFF55FFFF;
  if (team == "Pink")
    return 0xFFFF55FF;
  if (team == "Gray" || team == "Grey")
    return 0xFF555555;
  return 0xFFFFFFFF;
}

// skin cache
namespace {
struct SkinEntry {
  GLuint glId = 0;
  ULONGLONG lastTry = 0;
  bool tentative = false;
  std::list<std::string>::iterator lruIt;
};
constexpr size_t kSkinCacheMax = 256;
std::list<std::string> g_skinLru;
std::unordered_map<std::string, SkinEntry> g_skinTexCache;
constexpr ULONGLONG SKIN_RETRY_MS = 1000;

struct LocalFrameGuard {
  JNIEnv *env;
  bool ok;
  LocalFrameGuard(JNIEnv *e, jint capacity) : env(e), ok(false) {
    if (env && env->PushLocalFrame(capacity) == 0) {
      ok = true;
    } else if (env && env->ExceptionCheck()) {
      env->ExceptionClear();
    }
  }
  ~LocalFrameGuard() {
    if (ok && env)
      env->PopLocalFrame(nullptr);
  }
  LocalFrameGuard(const LocalFrameGuard &) = delete;
  LocalFrameGuard &operator=(const LocalFrameGuard &) = delete;
};

static std::atomic<int> g_skinDiagSeenMask{0};
static inline void skinDiagOnce(int bit, const char *msg) {
  int mask = 1 << bit;
  int prev = g_skinDiagSeenMask.fetch_or(mask);
  if (!(prev & mask))
    logDiagnostic("SKIN: %s", msg);
}

GLuint resolveSkinTexId(JNIEnv *env, jobject tm, jobject npi,
                        bool *outIsDefault = nullptr) {
  if (outIsDefault) *outIsDefault = false;
  if (!env || !tm || !npi || !g_jc.m_tm_getTexture ||
      (!g_jc.m_getLocationSkin && !g_jc.f_locationSkin)) {
    skinDiagOnce(0, "resolveSkinTexId: preconditions failed "
                    "(env/tm/npi/m_tm_getTexture/getLocationSkin missing)");
    return 0;
  }
  jobject rl = nullptr;
  if (g_jc.m_getLocationSkin) {
    rl = env->CallObjectMethod(npi, g_jc.m_getLocationSkin);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      rl = nullptr;
    }
  }
  if (!rl && g_jc.f_locationSkin) {
    rl = env->GetObjectField(npi, g_jc.f_locationSkin);
  }
  if (!rl) {
    skinDiagOnce(
        1,
        "resolveSkinTexId: locationSkin is null (skin not loaded yet)");
    return 0;
  }

  if (outIsDefault && g_jc.m_object_toString) {
    jstring jpath =
        (jstring)env->CallObjectMethod(rl, g_jc.m_object_toString);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    } else if (jpath) {
      const char *cpath = env->GetStringUTFChars(jpath, nullptr);
      if (cpath) {
        *outIsDefault = (strstr(cpath, "/skins/") == nullptr) &&
                        (strstr(cpath, ":skins/") == nullptr);
        env->ReleaseStringUTFChars(jpath, cpath);
      }
      env->DeleteLocalRef(jpath);
    }
  }

  jobject texObj = env->CallObjectMethod(tm, g_jc.m_tm_getTexture, rl);
  env->DeleteLocalRef(rl);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    skinDiagOnce(3, "resolveSkinTexId: getTexture threw");
    return 0;
  }
  if (!texObj) {
    skinDiagOnce(2, "resolveSkinTexId: getTexture returned null");
    return 0;
  }

  if (!g_jc.m_tex_getGlTextureId) {
    jclass realCls = env->GetObjectClass(texObj);
    if (realCls) {
      g_jc.m_tex_getGlTextureId =
          env->GetMethodID(realCls, "getGlTextureId", "()I");
      if (!g_jc.m_tex_getGlTextureId) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        g_jc.m_tex_getGlTextureId =
            env->GetMethodID(realCls, "func_110552_b", "()I");
      }
      if (!g_jc.m_tex_getGlTextureId) {
        if (env->ExceptionCheck())
          env->ExceptionClear();
        g_jc.m_tex_getGlTextureId = env->GetMethodID(realCls, "b", "()I");
      }
      static std::atomic<bool> s_loggedCls{false};
      if (!s_loggedCls.exchange(true)) {
        jclass jcls = env->FindClass("java/lang/Class");
        if (env->ExceptionCheck())
          env->ExceptionClear();
        if (jcls) {
          jmethodID mName =
              env->GetMethodID(jcls, "getName", "()Ljava/lang/String;");
          if (env->ExceptionCheck())
            env->ExceptionClear();
          if (mName) {
            jstring jn = (jstring)env->CallObjectMethod(realCls, mName);
            if (env->ExceptionCheck())
              env->ExceptionClear();
            if (jn) {
              const char *u = env->GetStringUTFChars(jn, nullptr);
              if (u) {
                logDiagnostic(
                    "SKIN: texture concrete class=%s, m_tex_getGlTextureId=%p",
                    u, (void *)g_jc.m_tex_getGlTextureId);
                env->ReleaseStringUTFChars(jn, u);
              }
              env->DeleteLocalRef(jn);
            }
          }
          env->DeleteLocalRef(jcls);
        }
      }
      env->DeleteLocalRef(realCls);
    }
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  if (!g_jc.m_tex_getGlTextureId) {
    skinDiagOnce(4, "resolveSkinTexId: getGlTextureId method not resolved on "
                    "concrete class");
    env->DeleteLocalRef(texObj);
    return 0;
  }

  GLuint id = 0;
  jint v = env->CallIntMethod(texObj, g_jc.m_tex_getGlTextureId);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    skinDiagOnce(3, "resolveSkinTexId: getGlTextureId call threw");
  } else if (v > 0) {
    id = (GLuint)v;
  } else {
    skinDiagOnce(5, "resolveSkinTexId: getGlTextureId returned <=0 (texture "
                    "not uploaded yet)");
  }
  env->DeleteLocalRef(texObj);
  return id;
}

static void touchLru(SkinEntry &entry) {
  g_skinLru.splice(g_skinLru.begin(), g_skinLru, entry.lruIt);
}

static SkinEntry &upsertEntry(const std::string &name, GLuint id,
                              ULONGLONG now, bool tentative) {
  auto it = g_skinTexCache.find(name);
  if (it != g_skinTexCache.end()) {
    bool wasConfirmed = !it->second.tentative && it->second.glId != 0;
    it->second.glId = id;
    it->second.lastTry = now;
    if (!wasConfirmed) {
      it->second.tentative = tentative;
    }
    touchLru(it->second);
    return it->second;
  }
  if (g_skinTexCache.size() >= kSkinCacheMax && !g_skinLru.empty()) {
    g_skinTexCache.erase(g_skinLru.back());
    g_skinLru.pop_back();
  }
  g_skinLru.push_front(name);
  SkinEntry e{id, now, tentative, g_skinLru.begin()};
  auto [insIt, _] = g_skinTexCache.emplace(name, e);
  return insIt->second;
}

GLuint cacheLookupOrFetch(JNIEnv *env, jobject tm, jobject npi,
                          const std::string &name) {
  auto it = g_skinTexCache.find(name);
  ULONGLONG now = GetTickCount64();
  if (it != g_skinTexCache.end()) {
    touchLru(it->second);
    if (it->second.glId != 0 && !it->second.tentative)
      return it->second.glId;
    if (now - it->second.lastTry < SKIN_RETRY_MS)
      return it->second.glId;
  }
  bool isDefault = false;
  GLuint id = resolveSkinTexId(env, tm, npi, &isDefault);
  upsertEntry(name, id, now, isDefault);
  return id;
}
} // namespace

static void drawHead(RenderCtx &ctx, jobject tm, jobject npi, GLuint glTexId,
                     float x, float y, float size) {
  if (!ctx.env)
    return;

  GLint prevTex = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

  static std::atomic<int> s_drawDiagSeen{0};
  auto drawOnce = [](int bit, const char *msg) {
    int mask = 1 << bit;
    int prev = s_drawDiagSeen.fetch_or(mask);
    if (!(prev & mask))
      logDiagnostic("SKIN drawHead: %s", msg);
  };
  static std::atomic<long> s_directHits{0}, s_fallbackHits{0};
  static std::atomic<ULONGLONG> s_lastSummary{0};

  if (glTexId != 0) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, glTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_directHits.fetch_add(1);
  } else {
    if (!npi || !tm || !g_jc.m_getLocationSkin || !g_jc.m_bindTexture) {
      drawOnce(
          0,
          "fallback preconds missing — npi/tm/m_getLocationSkin/m_bindTexture");
      return;
    }
    jobject rl = ctx.env->CallObjectMethod(npi, g_jc.m_getLocationSkin);
    if (ctx.env->ExceptionCheck()) {
      ctx.env->ExceptionClear();
      drawOnce(1, "getLocationSkin threw");
      return;
    }
    if (!rl) {
      drawOnce(2, "getLocationSkin returned null");
      return;
    }
    ctx.env->CallVoidMethod(tm, g_jc.m_bindTexture, rl);
    if (ctx.env->ExceptionCheck()) {
      ctx.env->ExceptionClear();
      drawOnce(3, "bindTexture threw");
    }
    ctx.env->DeleteLocalRef(rl);
    glEnable(GL_TEXTURE_2D);
    s_fallbackHits.fetch_add(1);
  }
  ULONGLONG now = GetTickCount64();
  if (now - s_lastSummary.load() > 5000) {
    s_lastSummary.store(now);
    logDiagnostic("SKIN drawHead: direct=%ld fallback=%ld (5s rollup)",
                  s_directHits.exchange(0), s_fallbackHits.exchange(0));
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1, 1, 1, 1);
  glBegin(GL_QUADS);
  glTexCoord2f(0.125f, 0.125f);
  glVertex2f(x, y);
  glTexCoord2f(0.125f, 0.25f);
  glVertex2f(x, y + size);
  glTexCoord2f(0.25f, 0.25f);
  glVertex2f(x + size, y + size);
  glTexCoord2f(0.25f, 0.125f);
  glVertex2f(x + size, y);
  glTexCoord2f(0.625f, 0.125f);
  glVertex2f(x, y);
  glTexCoord2f(0.625f, 0.25f);
  glVertex2f(x, y + size);
  glTexCoord2f(0.75f, 0.25f);
  glVertex2f(x + size, y + size);
  glTexCoord2f(0.75f, 0.125f);
  glVertex2f(x + size, y);
  glEnd();

  if (prevTex > 0) {
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
  }
}

static uint32_t getRainbow(float speed = 1.0f, float offset = 0.0f) {
  float t = (float)GetTickCount64() / 1000.0f * speed + offset;
  float r = (sin(t) + 1.0f) / 2.0f;
  float g = (sin(t + 2.0f) + 1.0f) / 2.0f;
  float b = (sin(t + 4.0f) + 1.0f) / 2.0f;
  return 0xFF000000 | ((int)(r * 255) << 16) | ((int)(g * 255) << 8) |
         (int)(b * 255);
}

static uint32_t hpToColor(int hp) {
  if (hp <= 5)
    return 0xFFFF5555;
  if (hp <= 10)
    return 0xFFFFAA00;
  if (hp <= 15)
    return 0xFFFFFF55;
  return 0xFF55FF55;
}

static double sortKeyFor(const Hypixel::PlayerStats &s,
                         const std::string &mode) {
  if (mode == "Team") {
    if (s.teamColor == "Red")
      return 1.0;
    if (s.teamColor == "Blue")
      return 2.0;
    if (s.teamColor == "Green")
      return 3.0;
    if (s.teamColor == "Yellow")
      return 4.0;
    if (s.teamColor == "Aqua")
      return 5.0;
    if (s.teamColor == "Pink")
      return 6.0;
    if (s.teamColor == "White")
      return 7.0;
    if (s.teamColor == "Gray" || s.teamColor == "Grey")
      return 8.0;
    return 9.0;
  }
  if (mode == "Star")
    return (double)s.bedwarsStar;
  if (mode == "FK")
    return (double)s.bedwarsFinalKills;
  if (mode == "Finals")
    return (double)s.bedwarsFinalKills;
  if (mode == "FKDR") {
    return (s.bedwarsFinalDeaths == 0)
               ? (double)s.bedwarsFinalKills
               : (double)s.bedwarsFinalKills / s.bedwarsFinalDeaths;
  }
  if (mode == "Wins")
    return (double)s.bedwarsWins;
  if (mode == "WLR") {
    return (s.bedwarsLosses == 0) ? (double)s.bedwarsWins
                                  : (double)s.bedwarsWins / s.bedwarsLosses;
  }
  if (mode == "WS")
    return (double)s.winstreak;
  return 0.0;
}

enum ColKey {
  COL_HP,
  COL_STAR,
  COL_NAME,
  COL_FINALS,
  COL_FKDR,
  COL_KILLS,
  COL_KDR,
  COL_BEDS,
  COL_BLR,
  COL_WINS,
  COL_WLR,
  COL_WS,
  COL_TAGS,
  COL_PING,
  COL_COUNT
};

struct Column {
  ColKey key;
  std::string title;
  float width;
  float align;
  bool enabled;
};

struct Cell {
  std::string text;
  uint32_t color;
};

static bool s_rendering = false;

void render(void *hdcPtr) {
  if (s_rendering)
    return;
  s_rendering = true;

  init();

  RenderCtx ctx;
  ctx.env = lc->getEnv();
  if (!ctx.env || !g_jc.initialized)
    return;

  LocalFrameGuard localFrame(ctx.env, 256);
  if (!localFrame.ok)
    return;

  ctx.mc = ctx.env->GetStaticObjectField(g_jc.mcCls, g_jc.f_theMc);
  if (!ctx.mc)
    return;
  ctx.fontRenderer = g_jc.f_fontRenderer
                         ? ctx.env->GetObjectField(ctx.mc, g_jc.f_fontRenderer)
                         : nullptr;

  static int s_cachedGuiScale = -1;
  if (s_cachedGuiScale < 0 && ctx.mc && g_jc.f_gameSettings &&
      g_jc.f_guiScale) {
    jobject gs = ctx.env->GetObjectField(ctx.mc, g_jc.f_gameSettings);
    if (gs) {
      s_cachedGuiScale = ctx.env->GetIntField(gs, g_jc.f_guiScale);
      ctx.env->DeleteLocalRef(gs);
    }
  }
  if (s_cachedGuiScale >= 0)
    ctx.guiScale = s_cachedGuiScale;

  float screenWidth = 0.0f, screenHeight = 0.0f;
  if (hdcPtr) {
    HWND hwnd = WindowFromDC((HDC)hdcPtr);
    RECT rc{};
    if (hwnd && GetClientRect(hwnd, &rc)) {
      screenWidth = (float)(rc.right - rc.left);
      screenHeight = (float)(rc.bottom - rc.top);
    }
  }
  if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    screenWidth = (float)viewport[2];
    screenHeight = (float)viewport[3];
  }

  int scaleFactor = 1;
  int targetScale = (ctx.guiScale > 0 && ctx.guiScale <= 4) ? ctx.guiScale : 2;
  while (scaleFactor < targetScale && screenWidth / (scaleFactor + 1) >= 320 &&
         screenHeight / (scaleFactor + 1) >= 240) {
    scaleFactor++;
  }
  float scaledWidth = screenWidth / scaleFactor;
  float scaledHeight = screenHeight / scaleFactor;

  static bool loggedOnce = false;
  if (!loggedOnce) {
    logDiagnostic("Render Context: Screen=%fx%f, guiScale=%d, scaleFactor=%d, "
                  "scaledRes=%fx%f",
                  screenWidth, screenHeight, ctx.guiScale, scaleFactor,
                  scaledWidth, scaledHeight);
    loggedOnce = true;
  }

  std::string sortMode = Config::getSortMode();
  bool desc = Config::isTabSortDescending();

  struct Decorated {
    double key;
    std::string name;
    Hypixel::PlayerStats stats;
    jobject npi;
    GLuint glTexId;
    int ping;
    std::string lookupName;
    bool hasRealName;
  };
  std::vector<Decorated> rows;
  bool activeMatch = OVson::isInHypixelGame() && !OVson::isInPreGameLobby();

  jobject scoreboard = nullptr;
  jobject healthObj = nullptr;
  jobject world = ctx.env->GetObjectField(ctx.mc, g_jc.f_theWorld);
  if (world) {
    scoreboard = ctx.env->CallObjectMethod(world, g_jc.m_getScoreboard);
    if (scoreboard) {
      healthObj = ctx.env->CallObjectMethod(
          scoreboard, g_jc.m_getObjectiveInDisplaySlot, 0);
    }
    ctx.env->DeleteLocalRef(world);
  }

  std::vector<std::pair<std::string, const Hypixel::PlayerStats *>>
      fuzzyCandidates;
  {
    std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
    if (!OVson::g_playerStatsMap.empty()) {
      fuzzyCandidates.reserve(OVson::g_playerStatsMap.size());
      for (auto const &[mapKey, mapStats] : OVson::g_playerStatsMap) {
        if (mapKey.length() >= 3) {
          fuzzyCandidates.push_back({mapKey, &mapStats});
        }
      }
    }
  }

  jobject tmForRows = nullptr;
  if (g_jc.f_renderEngine) {
    tmForRows = ctx.env->GetObjectField(ctx.mc, g_jc.f_renderEngine);
    if (ctx.env->ExceptionCheck())
      ctx.env->ExceptionClear();
  }

  if (g_jc.m_getNet) {
    jobject nh = ctx.env->CallObjectMethod(ctx.mc, g_jc.m_getNet);
    if (ctx.env->ExceptionCheck())
      ctx.env->ExceptionClear();
    if (nh) {
      if (g_jc.m_getPlayerInfoMap) {
        jobject coll = ctx.env->CallObjectMethod(nh, g_jc.m_getPlayerInfoMap);
        if (ctx.env->ExceptionCheck())
          ctx.env->ExceptionClear();
        if (coll) {
          jclass cCls = ctx.env->GetObjectClass(coll);
          jmethodID mSize = ctx.env->GetMethodID(cCls, "size", "()I");
          if (mSize) {
            int sz = ctx.env->CallIntMethod(coll, mSize);
            static int lastSz = -1;
            if (sz != lastSz) {
              logDiagnostic("Collection size: %d", sz);
              lastSz = sz;
            }
          }
          ctx.env->DeleteLocalRef(cCls);

          if (g_jc.m_iterator && g_jc.m_hasNext && g_jc.m_next) {
            jobject iter = ctx.env->CallObjectMethod(coll, g_jc.m_iterator);
            if (ctx.env->ExceptionCheck())
              ctx.env->ExceptionClear();
            if (iter) {
              std::unordered_set<std::string> seenNames;
              while (g_jc.m_hasNext &&
                     ctx.env->CallBooleanMethod(iter, g_jc.m_hasNext)) {
                if (ctx.env->ExceptionCheck()) {
                  ctx.env->ExceptionClear();
                  break;
                }
                if (!g_jc.m_next)
                  break;
                jobject npi = ctx.env->CallObjectMethod(iter, g_jc.m_next);
                if (ctx.env->ExceptionCheck()) {
                  ctx.env->ExceptionClear();
                  continue;
                }
                if (npi) {
                  jobject gp = g_jc.m_getGameProfile
                                   ? ctx.env->CallObjectMethod(
                                         npi, g_jc.m_getGameProfile)
                                   : nullptr;
                  if (ctx.env->ExceptionCheck()) {
                    ctx.env->ExceptionClear();
                    ctx.env->DeleteLocalRef(npi);
                    continue;
                  }
                  if (gp) {
                    jstring jName = g_jc.m_getName
                                        ? (jstring)ctx.env->CallObjectMethod(
                                              gp, g_jc.m_getName)
                                        : nullptr;
                    if (ctx.env->ExceptionCheck()) {
                      ctx.env->ExceptionClear();
                      ctx.env->DeleteLocalRef(gp);
                      ctx.env->DeleteLocalRef(npi);
                      continue;
                    }
                    if (jName) {
                      const char *utf = ctx.env->GetStringUTFChars(jName, 0);
                      std::string name = utf ? utf : "";
                      if (utf)
                        ctx.env->ReleaseStringUTFChars(jName, utf);

                      std::string cleanNameForMatching;
                      {
                        std::string stripped;
                        for (size_t i = 0; i < name.length();) {
                          if (i + 2 <= name.length() &&
                              (unsigned char)name[i] == 0xC2 &&
                              (unsigned char)name[i + 1] == 0xA7) {
                            i += 3; // skip § + color code
                          } else if ((unsigned char)name[i] == 0xA7) {
                            i += 2; // alternative § encoding
                          } else {
                            unsigned char c = (unsigned char)name[i];
                            if (c > 32 && c < 127) { // only printable ASCII
                              stripped += name[i];
                              char low = name[i];
                              if (low >= 'A' && low <= 'Z')
                                low += 32;
                              cleanNameForMatching += low;
                            }
                            i++;
                          }
                        }
                        name = stripped;
                      }

                      if (cleanNameForMatching.empty() ||
                          seenNames.count(cleanNameForMatching)) {
                        ctx.env->DeleteLocalRef(gp);
                        ctx.env->DeleteLocalRef(npi);
                        ctx.env->DeleteLocalRef(jName);
                        continue;
                      }
                      seenNames.insert(cleanNameForMatching);

                      std::string cleanName;
                      for (size_t i = 0; i < name.length(); ++i) {
                        char c = name[i];
                        if (c >= 'A' && c <= 'Z') {
                          cleanName += (char)(c + 32);
                        } else if ((c >= 'a' && c <= 'z') ||
                                   (c >= '0' && c <= '9') || c == '_') {
                          cleanName += c;
                        }
                      }

                      bool foundInMap = false;
                      {
                        std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
                        auto it = OVson::g_playerStatsMap.find(cleanName);
                        if (it == OVson::g_playerStatsMap.end())
                          it = OVson::g_playerStatsMap.find(name);
                        if (it == OVson::g_playerStatsMap.end()) {
                          std::string lowerName = name;
                          for (auto &c : lowerName) {
                            if (c >= 'A' && c <= 'Z')
                              c += 32;
                          }
                          it = OVson::g_playerStatsMap.find(lowerName);
                        }
                        if (it != OVson::g_playerStatsMap.end()) {
                          foundInMap = true;
                        }
                      }

                      if (!foundInMap) {
                        constexpr size_t kMaxFuzzyVisits = 64;
                        size_t visited = 0;
                        for (auto &cand : fuzzyCandidates) {
                          if (++visited > kMaxFuzzyVisits)
                            break;
                          const std::string &mapKey = cand.first;
                          if (name.find(mapKey) != std::string::npos ||
                              cleanName.find(mapKey) != std::string::npos ||
                              mapKey.find(name) != std::string::npos ||
                              mapKey.find(cleanName) != std::string::npos) {
                            foundInMap = true;
                            break;
                          }
                        }
                      }

                      std::string lookupName = name;
                      std::string lookupClean = cleanName;
                      bool hasRealName = false;
                      {
                          std::lock_guard<std::mutex> nLock(OVson::g_nickMapMutex);
                          auto nit = OVson::g_nickToRealMap.find(name);
                          if (nit != OVson::g_nickToRealMap.end()) {
                              lookupName = nit->second;
                              lookupClean = lookupName; // simplified
                              hasRealName = true;
                          }
                      }

                      Hypixel::PlayerStats stats;
                      bool foundInMapStat = false;
                      {
                        std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
                        auto it = OVson::g_playerStatsMap.find(lookupName);
                        if (it == OVson::g_playerStatsMap.end())
                          it = OVson::g_playerStatsMap.find(lookupClean);
                        if (it != OVson::g_playerStatsMap.end()) {
                          stats = it->second;
                          foundInMapStat = true;
                        }
                      }

                      if (!foundInMapStat) {
                        std::lock_guard<std::mutex> lock(
                            OVson::g_pendingStatsMutex);
                        auto pit = OVson::g_pendingStatsMap.find(lookupName);
                        if (pit == OVson::g_pendingStatsMap.end())
                          pit = OVson::g_pendingStatsMap.find(lookupClean);
                        if (pit != OVson::g_pendingStatsMap.end()) {
                          stats = pit->second;
                          foundInMapStat = true;
                        }
                      }

                      if (hasRealName && !foundInMapStat) {
                        OVson::requestStatsForVisiblePlayer(lookupName);
                      }

                      if (hasRealName) {
                        std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
                        auto it = OVson::g_playerStatsMap.find(name);
                        if (it == OVson::g_playerStatsMap.end())
                          it = OVson::g_playerStatsMap.find(cleanName);
                        if (it != OVson::g_playerStatsMap.end()) {
                          if (!it->second.teamColor.empty()) {
                            stats.teamColor = it->second.teamColor;
                          }
                          if (it->second.healthKnown) {
                            stats.inGameHealth = it->second.inGameHealth;
                            stats.healthKnown = true;
                          }
                        }
                      }

                      if (scoreboard && healthObj &&
                          g_jc.m_getValueFromObjective) {
                        jstring jn = ctx.env->NewStringUTF(name.c_str());
                        if (jn) {
                          jobject score = ctx.env->CallObjectMethod(
                              scoreboard, g_jc.m_getValueFromObjective, jn,
                              healthObj);
                          if (ctx.env->ExceptionCheck())
                            ctx.env->ExceptionClear();
                          if (score) {
                            if (g_jc.m_getScorePoints) {
                              stats.inGameHealth = ctx.env->CallIntMethod(
                                  score, g_jc.m_getScorePoints);
                              if (ctx.env->ExceptionCheck())
                                ctx.env->ExceptionClear();

                              std::lock_guard<std::mutex> lock(
                                  OVson::g_statsMutex);
                              auto mit = OVson::g_playerStatsMap.find(lookupClean);
                              if (mit == OVson::g_playerStatsMap.end())
                                mit = OVson::g_playerStatsMap.find(lookupName);
                              if (mit != OVson::g_playerStatsMap.end()) {
                                mit->second.inGameHealth = stats.inGameHealth;
                                mit->second.healthKnown = true;
                              }
                              if (hasRealName) {
                                auto fit = OVson::g_playerStatsMap.find(cleanName);
                                if (fit == OVson::g_playerStatsMap.end())
                                  fit = OVson::g_playerStatsMap.find(name);
                                if (fit != OVson::g_playerStatsMap.end()) {
                                  fit->second.inGameHealth = stats.inGameHealth;
                                  fit->second.healthKnown = true;
                                }
                              }
                            }
                            ctx.env->DeleteLocalRef(score);
                          }
                          ctx.env->DeleteLocalRef(jn);
                        }
                      }

                      if (!(activeMatch && stats.inGameHealth <= 0)) {
                        GLuint glTex =
                            cacheLookupOrFetch(ctx.env, tmForRows, npi, name);

                        int ping = -1;
                        if (g_jc.f_responseTime) {
                          jint v =
                              ctx.env->GetIntField(npi, g_jc.f_responseTime);
                          if (ctx.env->ExceptionCheck())
                            ctx.env->ExceptionClear();
                          else
                            ping = (int)v;
                        } else if (g_jc.m_getResponseTime) {
                          jint v = ctx.env->CallIntMethod(
                              npi, g_jc.m_getResponseTime);
                          if (ctx.env->ExceptionCheck())
                            ctx.env->ExceptionClear();
                          else
                            ping = (int)v;
                        }

                        std::string finalDisplayName = name;
                        if (hasRealName) {
                            finalDisplayName = lookupName + " (" + name + ")";
                        }

                        rows.push_back({sortKeyFor(stats, sortMode), finalDisplayName,
                                        stats, ctx.env->NewLocalRef(npi), glTex,
                                        ping, lookupName, hasRealName});

                        {
                          std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
                          auto it = OVson::g_playerStatsMap.find(name);
                          if (it != OVson::g_playerStatsMap.end()) {
                            it->second.lastPing = ping;
                          } else {
                            std::string cleanName;
                            for (char c : name) {
                              if (c >= 'A' && c <= 'Z')
                                cleanName += (char)(c + 32);
                              else if ((c >= 'a' && c <= 'z') ||
                                       (c >= '0' && c <= '9') || c == '_')
                                cleanName += c;
                            }
                            it = OVson::g_playerStatsMap.find(cleanName);
                            if (it != OVson::g_playerStatsMap.end()) {
                              it->second.lastPing = ping;
                            }
                          }
                        }
                      }
                      ctx.env->DeleteLocalRef(jName);
                    }
                    ctx.env->DeleteLocalRef(gp);
                  }
                  ctx.env->DeleteLocalRef(npi);
                }
              }
              ctx.env->DeleteLocalRef(iter);
            }
          }
          ctx.env->DeleteLocalRef(coll);
        } else {
          static bool loggedCollNull = false;
          if (!loggedCollNull) {
            logDiagnostic("ERROR: Player Collection is NULL!");
            loggedCollNull = true;
          }
        }
      } else {
        static bool loggedMapNull = false;
        if (!loggedMapNull) {
          logDiagnostic("ERROR: getPlayerInfoMap method is MISSING!");
          loggedMapNull = true;
        }
      }
      ctx.env->DeleteLocalRef(nh);
    } else {
      static bool loggedNhNull = false;
      if (!loggedNhNull) {
        logDiagnostic("ERROR: NetHandler (nh) is NULL!");
        loggedNhNull = true;
      }
    }
  }

  if (healthObj)
    ctx.env->DeleteLocalRef(healthObj);
  if (scoreboard)
    ctx.env->DeleteLocalRef(scoreboard);

  if (rows.empty() && Config::isGlobalDebugEnabled()) {
    auto mkDemo = [](const std::string &n, int star, int fk, int fd, int k,
                     int d, int bb, int bl, int w, int l, int hp,
                     const std::string &team) {
      Hypixel::PlayerStats s;
      s.displayName = n;
      s.bedwarsStar = star;
      s.bedwarsFinalKills = fk;
      s.bedwarsFinalDeaths = fd;
      s.bedwarsKills = k;
      s.bedwarsDeaths = d;
      s.bedwarsBedsBroken = bb;
      s.bedwarsBedsLost = bl;
      s.bedwarsWins = w;
      s.bedwarsLosses = l;
      s.inGameHealth = hp;
      s.teamColor = team;
      return s;
    };
    rows.push_back({1.0, "SweatyPro",
                    mkDemo("SweatyPro", 1250, 15000, 1000, 25000, 5000, 2500,
                           500, 1500, 200, 20, "Red"),
                    nullptr, 0, 42, "SweatyPro", false});
    rows.push_back({0.5, "CasualPlayer",
                    mkDemo("CasualPlayer", 45, 200, 150, 1200, 1000, 85, 100,
                           45, 120, 12, "Blue"),
                    nullptr, 0, 87, "CasualPlayer", false});
  }

  if (!rows.empty()) {
    static int lastCount = -1;
    if (rows.size() != lastCount) {
      logDiagnostic("Player list size: %zu", rows.size());
      lastCount = (int)rows.size();
    }
  } else {
    static bool warnedEmpty = false;
    if (!warnedEmpty) {
      logDiagnostic("WARNING: Player list (rows) is empty!");
      warnedEmpty = true;
    }
  }

  std::sort(rows.begin(), rows.end(),
            [desc](const Decorated &a, const Decorated &b) {
              if (a.key == b.key)
                return a.name < b.name;
              return desc ? (a.key > b.key) : (a.key < b.key);
            });

  Column cols[COL_COUNT] = {
      {COL_HP, "HP", 0, 0.0f, Config::isProShowHp()},
      {COL_STAR, "Star", 0, 0.0f, Config::isProShowStar()},
      {COL_NAME, "Name", 0, 0.0f, true},
      {COL_FINALS, "Finals", 0, 0.0f, Config::isProShowFk()},
      {COL_FKDR, "FKDR", 0, 0.0f, Config::isProShowFkdr()},
      {COL_KILLS, "Kills", 0, 0.0f, Config::isProShowKills()},
      {COL_KDR, "KDR", 0, 0.0f, Config::isProShowKdr()},
      {COL_BEDS, "Beds", 0, 0.0f, Config::isProShowBeds()},
      {COL_BLR, "BLR", 0, 0.0f, Config::isProShowBlr()},
      {COL_WINS, "Wins", 0, 0.0f, Config::isProShowWins()},
      {COL_WLR, "WLR", 0, 0.0f, Config::isProShowWlr()},
      {COL_WS, "WS", 0, 0.0f, Config::isProShowWs()},
      {COL_TAGS, "Tags", 0, 0.0f, Config::isProShowTags()},
      {COL_PING, "Ping", 0, 0.0f, Config::isProShowPing()},
  };
  std::vector<int> activeIdx;
  for (int i = 0; i < COL_COUNT; ++i)
    if (cols[i].enabled)
      activeIdx.push_back(i);

  auto cellFor = [](ColKey k, const Hypixel::PlayerStats &s,
                    const std::string &name, int ping, const std::string &lookupName, bool hasRealName) -> Cell {
    double fkdr = (s.bedwarsFinalDeaths == 0)
                      ? (double)s.bedwarsFinalKills
                      : (double)s.bedwarsFinalKills / s.bedwarsFinalDeaths;
    double kd = (s.bedwarsDeaths == 0)
                    ? (double)s.bedwarsKills
                    : (double)s.bedwarsKills / s.bedwarsDeaths;
    double blr = (s.bedwarsBedsLost == 0)
                     ? (double)s.bedwarsBedsBroken
                     : (double)s.bedwarsBedsBroken / s.bedwarsBedsLost;
    double wlr = (s.bedwarsLosses == 0)
                     ? (double)s.bedwarsWins
                     : (double)s.bedwarsWins / s.bedwarsLosses;

    bool hasStats = s.isFetched;
    auto fmtStat = [&](int val) -> std::string {
      return (hasStats && (!s.isNicked || hasRealName)) ? fmtCommas(val) : "-";
    };
    auto fmtVal2 = [&](double val) -> std::string {
      return (hasStats && (!s.isNicked || hasRealName)) ? fmt2(val) : "-";
    };
    uint32_t defaultColor = hasStats ? 0xFFFFFFFF : 0xFFAAAAAA;
    switch (k) {
    case COL_HP:
      return {std::to_string(s.inGameHealth), hpToColor(s.inGameHealth)};
    case COL_STAR: {
      if (s.isNicked && !hasRealName)
        return {"\xC2\xA7"
                "4[NICKED]",
                0xFFFFFFFF};
      return {hasStats ? BedwarsStars::GetFormattedLevel(s.bedwarsStar) : "-",
              hasStats ? StatColors::getColor(StatColors::StatType::Star,
                                              s.bedwarsStar)
                       : defaultColor};
    }
    case COL_NAME: {
      std::string displayName = name;
      if (Anticheat::isPlayerFlagged(lookupName)) {
        displayName += " \xC2\xA7" "c\xE2\x9A\xA0"; // " §c⚠"
      }
      return {displayName, teamColorToRGB(s.teamColor)};
    }
    case COL_FINALS:
      return {fmtStat(s.bedwarsFinalKills),
              hasStats ? StatColors::getColor(StatColors::StatType::FinalKills,
                                              s.bedwarsFinalKills)
                       : defaultColor};
    case COL_FKDR:
      return {fmtVal2(fkdr),
              hasStats ? StatColors::getColor(StatColors::StatType::FKDR, fkdr)
                       : defaultColor};
    case COL_KILLS:
      return {fmtStat(s.bedwarsKills),
              hasStats ? StatColors::getColor(StatColors::StatType::Kills,
                                              s.bedwarsKills)
                       : defaultColor};
    case COL_KDR:
      return {fmtVal2(kd),
              hasStats ? StatColors::getColor(StatColors::StatType::KDR, kd)
                       : defaultColor};
    case COL_BEDS:
      return {fmtStat(s.bedwarsBedsBroken),
              hasStats ? StatColors::getColor(StatColors::StatType::Beds,
                                              s.bedwarsBedsBroken)
                       : defaultColor};
    case COL_BLR:
      return {fmtVal2(blr),
              hasStats ? StatColors::getColor(StatColors::StatType::BLR, blr)
                       : defaultColor};
    case COL_WINS:
      return {fmtStat(s.bedwarsWins),
              hasStats ? StatColors::getColor(StatColors::StatType::Wins,
                                              s.bedwarsWins)
                       : defaultColor};
    case COL_WLR:
      return {fmtVal2(wlr),
              hasStats ? StatColors::getColor(StatColors::StatType::WLR, wlr)
                       : defaultColor};
    case COL_WS:
      return {fmtStat(s.winstreak),
              hasStats
                  ? StatColors::getColor(StatColors::StatType::WS, s.winstreak)
                  : defaultColor};
    case COL_TAGS:
      if (!s.areTagsFetched)
        return {"-", 0xFFAAAAAA};
      return {s.tagsDisplay.empty() ? "-" : s.tagsDisplay, 0xFFFFFFFF};
    case COL_PING: {
      int p = ping;
      int mode = Config::getPingDisplayMode();

      if (s.auroraPing != -1) {
        if (mode == 2 && s.auroraPingRecentAvg != -1)
          p = s.auroraPingRecentAvg;
        else
          p = s.auroraPing;
      } else if (mode == 2 && s.auroraPingRecentAvg != -1) {
        p = s.auroraPingRecentAvg;
      }

      if (p < 0)
        return {"N/A", 0xFFAAAAAA};

      uint32_t color =
          StatColors::getColor(StatColors::StatType::Ping, (double)p);
      return {std::to_string(p), color};
    }
    default:
      return {"", 0xFFFFFFFF};
    }
  };

  std::vector<std::vector<Cell>> rowCells(rows.size());
  for (size_t r = 0; r < rows.size(); ++r) {
    rowCells[r].reserve(activeIdx.size());
    for (int idx : activeIdx)
      rowCells[r].push_back(
          cellFor(cols[idx].key, rows[r].stats, rows[r].name, rows[r].ping, rows[r].lookupName, rows[r].hasRealName));
  }

  for (size_t ci = 0; ci < activeIdx.size(); ++ci) {
    int idx = activeIdx[ci];
    float maxW = (float)measure(ctx, cols[idx].title);
    for (size_t r = 0; r < rows.size(); ++r) {
      float cellW = (float)measure(ctx, rowCells[r][ci].text);
      if (cols[idx].key == COL_NAME)
        cellW += 12.0f;
      if (cellW > maxW)
        maxW = cellW;
    }
    cols[idx].width = std::floor(maxW + 8.0f);
  }

  std::vector<std::string> footerLines;
  {
    std::lock_guard<std::mutex> lock(OVson::g_footerMutex);
    if (!OVson::g_tabFooterText.empty()) {
      std::string raw = OVson::g_tabFooterText;
      std::vector<std::pair<std::string, std::string>> keywords = {
          {"Final Kills", "Final Kills"},
          {"Kills", "Kills"},
          {"Beds Broken", "Beds Broken"},
          {"Beds B", "Beds Broken"},
          {"Wins", "Wins"},
          {"Losses", "Losses"}};

      std::string currentStyledLine;
      for (const auto &kw : keywords) {
        size_t pos = raw.find(kw.first);
        if (pos != std::string::npos) {
          size_t colon = raw.find(':', pos);
          if (colon != std::string::npos) {
            size_t valStart = raw.find_first_not_of(" :", colon);
            if (valStart != std::string::npos) {
              size_t valEnd = raw.find_first_of(" \n\r", valStart);
              std::string value =
                  raw.substr(valStart, (valEnd == std::string::npos)
                                           ? std::string::npos
                                           : (valEnd - valStart));

              if (!currentStyledLine.empty())
                currentStyledLine += "  ";
              currentStyledLine += "\xC2\xA7"
                                   "b" +
                                   kw.second +
                                   ": \xC2\xA7"
                                   "e" +
                                   value;

              raw.erase(pos, (valEnd == std::string::npos) ? std::string::npos
                                                           : (valEnd - pos));
            }
          }
        }
      }
      if (!currentStyledLine.empty())
        footerLines.push_back(currentStyledLine);
      else
        footerLines.push_back(OVson::g_tabFooterText);
    }
  }

  float totalWidth = 0;
  for (int idx : activeIdx)
    totalWidth += cols[idx].width;
  float spacing = 4.0f;
  if (!activeIdx.empty())
    totalWidth += spacing * (activeIdx.size() - 1);
  float padding = 8.0f;
  float boxWidth = std::floor(totalWidth + padding * 2.0f);
  for (const std::string &fl : footerLines) {
    float fw = (float)measure(ctx, fl);
    if (std::floor(fw + padding * 2.0f) > boxWidth)
      boxWidth = std::floor(fw + padding * 2.0f);
  }

  float rowHeight = 10.0f;
  float headerHeight = 12.0f;
  float titleHeight = 14.0f;
  float boxHeight = std::floor(titleHeight + headerHeight +
                               (rows.size() * rowHeight) + padding * 1.5f);
  if (!footerLines.empty())
    boxHeight += std::floor((footerLines.size() * rowHeight) + 6.0f);

  float startX = std::floor((scaledWidth - boxWidth) / 2.0f);
  float startY = 15.0f;

  GlGuard::GlAttribGuard _gAttrib(GL_ALL_ATTRIB_BITS);
  glViewport(0, 0, (GLint)screenWidth, (GLint)screenHeight);
  GlGuard::GlMatrixGuard _gPr(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, scaledWidth, scaledHeight, 0, -1, 1);
  GlGuard::GlMatrixGuard _gMv(GL_MODELVIEW);
  glLoadIdentity();

  GLboolean wasTexture2D = glIsEnabled(GL_TEXTURE_2D);
  GLboolean wasBlend = glIsEnabled(GL_BLEND);
  GLboolean wasDepthTest = glIsEnabled(GL_DEPTH_TEST);
  GLboolean wasLighting = glIsEnabled(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_TEXTURE_2D);
  glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
  glBegin(GL_QUADS);
  glVertex2f(startX, startY);
  glVertex2f(startX, startY + boxHeight);
  glVertex2f(startX + boxWidth, startY + boxHeight);
  glVertex2f(startX + boxWidth, startY);
  glEnd();
  glEnable(GL_TEXTURE_2D);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  float currentY = startY + padding;

  std::string titleText = "OVSON OVERLAY - PLAYING BEDWARS ON HYPIXEL.NET";
  int tw = measure(ctx, titleText);
  uint32_t rainbow = getRainbow(2.0f);
  float titleX = std::floor(startX + (boxWidth - tw) / 2.0f);
  drawString(ctx, titleText, titleX, currentY, rainbow);
  drawString(ctx, titleText, titleX + 0.5f, currentY, rainbow);
  currentY += titleHeight;

  float currentX = startX + padding;
  for (size_t ci = 0; ci < activeIdx.size(); ++ci) {
    int idx = activeIdx[ci];
    float cx = std::floor(currentX);
    int titleW = measure(ctx, cols[idx].title);
    if (cols[idx].align == 0.5f)
      cx = std::floor(currentX + (cols[idx].width - titleW) / 2.0f);
    else if (cols[idx].align == 1.0f)
      cx = std::floor(currentX + cols[idx].width - titleW);
    drawString(ctx, cols[idx].title, cx, currentY, 0xFFFFFFFF);
    currentX += cols[idx].width + spacing;
  }
  currentY += headerHeight;

  for (size_t r = 0; r < rows.size(); ++r) {
    currentX = startX + padding;
    for (size_t ci = 0; ci < activeIdx.size(); ++ci) {
      int idx = activeIdx[ci];
      const Cell &c = rowCells[r][ci];
      float cx = std::floor(currentX);
      int w = measure(ctx, c.text);
      if (cols[idx].align == 0.5f)
        cx = std::floor(currentX + cols[idx].width / 2.0f - w / 2.0f);
      else if (cols[idx].align == 1.0f)
        cx = std::floor(currentX + cols[idx].width - w);

      if (cols[idx].key == COL_NAME) {
        drawHead(ctx, tmForRows, rows[r].npi, rows[r].glTexId, cx,
                 std::floor(currentY + 0.5f), 8.0f);
        drawString(ctx, c.text, cx + 10.0f, currentY, c.color);
      } else {
        drawString(ctx, c.text, cx, currentY, c.color);
      }
      currentX += cols[idx].width + spacing;
    }
    currentY += rowHeight;
  }

  if (tmForRows)
    ctx.env->DeleteLocalRef(tmForRows);

  if (!footerLines.empty()) {
    currentY += 4.0f;
    for (const std::string &fl : footerLines) {
      int w = measure(ctx, fl);
      drawString(ctx, fl, startX + (boxWidth - w) / 2.0f, currentY, 0xFFFFFFFF);
      currentY += rowHeight;
    }
  }

  if (wasTexture2D)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);
  if (wasBlend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (wasDepthTest)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (wasLighting)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  // _gMv / _gPr / _gAttrib unwind here automatically.

  s_rendering = false;
}

} // namespace BetterTab
