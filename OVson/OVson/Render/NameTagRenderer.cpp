#include "NameTagRenderer.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/StatsTracker.h"
#include "../Services/Hypixel.h"
#include "../Utils/Anticheat/Anticheat.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/Logger.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <gl/GL.h>
#include <string>
#include <vector>

namespace OVson {

static FILE *g_ntLog = nullptr;
static DWORD g_ntLogLastTick = 0;

static FILE *openNtLog() {
  if (g_ntLog) return g_ntLog;
  wchar_t path[MAX_PATH];
  GetTempPathW(MAX_PATH, path);
  wcscat_s(path, L"ovson_nametag.log");
  _wfopen_s(&g_ntLog, path, L"a");
  return g_ntLog;
}

static bool shouldLogNow() {
  static constexpr bool kEnableNameTagLog = true;
  if (!kEnableNameTagLog) return false;
  DWORD now = GetTickCount();
  if (now - g_ntLogLastTick < 2000) return false;
  g_ntLogLastTick = now;
  return true;
}

static void ntLog(const char *fmt, ...) {
  FILE *f = openNtLog();
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fputc('\n', f);
  fflush(f);
}

struct Vec4 {
  double x, y, z, w;
};

static inline Vec4 matMul(const float *m, const Vec4 &v) {
  return Vec4{
      m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
      m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
      m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
      m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
  };
}

static bool readFloatBuffer(JNIEnv *env, jobject bufferObj, float *outArray, int size) {
  if (!bufferObj) return false;
  void *address = env->GetDirectBufferAddress(bufferObj);
  if (address) {
    memcpy(outArray, address, size * sizeof(float));
    return true;
  }
  jclass bufferCls = env->GetObjectClass(bufferObj);
  jmethodID getMethod = env->GetMethodID(bufferCls, "get", "(I)F");
  if (!getMethod) {
    env->DeleteLocalRef(bufferCls);
    return false;
  }
  for (int i = 0; i < size; ++i) {
    outArray[i] = env->CallFloatMethod(bufferObj, getMethod, i);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      env->DeleteLocalRef(bufferCls);
      return false;
    }
  }
  env->DeleteLocalRef(bufferCls);
  return true;
}

static bool readIntBuffer(JNIEnv *env, jobject bufferObj, int *outArray, int size) {
  if (!bufferObj) return false;
  void *address = env->GetDirectBufferAddress(bufferObj);
  if (address) {
    memcpy(outArray, address, size * sizeof(int));
    return true;
  }
  jclass bufferCls = env->GetObjectClass(bufferObj);
  jmethodID getMethod = env->GetMethodID(bufferCls, "get", "(I)I");
  if (!getMethod) {
    env->DeleteLocalRef(bufferCls);
    return false;
  }
  for (int i = 0; i < size; ++i) {
    outArray[i] = env->CallIntMethod(bufferObj, getMethod, i);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      env->DeleteLocalRef(bufferCls);
      return false;
    }
  }
  env->DeleteLocalRef(bufferCls);
  return true;
}

static NameTagRenderer *s_instance = nullptr;

NameTagRenderer::NameTagRenderer() {}
NameTagRenderer::~NameTagRenderer() {}

NameTagRenderer *NameTagRenderer::getInstance() {
  if (!s_instance)
    s_instance = new NameTagRenderer();
  return s_instance;
}

void NameTagRenderer::destroy() {
  if (s_instance) {
    delete s_instance;
    s_instance = nullptr;
  }
}

void NameTagRenderer::initIds() {
  if (m_ids.initialized)
    return;
  if (!lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  ntLog("initIds: starting");
  auto findCls = [&](const char *mcpName,
                     std::initializer_list<const char *> obfs) -> jclass {
    jclass c = lc->GetClass(mcpName);
    if (c) return c;
    for (const char *obf : obfs) {
      c = env->FindClass(obf);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (c) return c;
    }
    return nullptr;
  };

  try {
    jclass mcCls = findCls("net.minecraft.client.Minecraft",
                           {"ave", "avc", "avd"});
    if (!mcCls) { ntLog("initIds: FAIL mcCls"); return; }
    m_ids.mc_theMc = (void *)lc->GetStaticFieldID(
        mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;",
        "field_71432_P", "S", "Lave;");
    m_ids.mc_theWorld = (void *)lc->GetFieldID(
        mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
        "field_71441_e", "f", "Lbcv;");
    m_ids.mc_renderViewEntity = (void *)lc->GetFieldID(
        mcCls, "renderViewEntity", "Lnet/minecraft/entity/Entity;",
        "field_175622_Z", "ad", "Lpk;");
    m_ids.mc_renderManager = (void *)lc->GetFieldID(
        mcCls, "renderManager",
        "Lnet/minecraft/client/renderer/entity/RenderManager;",
        "field_175616_W", "aa", "Lbiu;");
    m_ids.mc_gameSettings = (void *)lc->GetFieldID(
        mcCls, "gameSettings", "Lnet/minecraft/client/settings/GameSettings;",
        "field_71474_y", "t", "Lavh;");
    m_ids.mc_timer = (void *)lc->GetFieldID(
        mcCls, "timer", "Lnet/minecraft/util/Timer;",
        "field_71428_T", "Y", "Lavl;");
    m_ids.mc_fontRendererObj = (void *)lc->GetFieldID(
        mcCls, "fontRendererObj",
        "Lnet/minecraft/client/gui/FontRenderer;",
        "field_71466_p", "p", "Lavn;");

    jclass rmCls =
        findCls("net.minecraft.client.renderer.entity.RenderManager",
                {"biu", "bjf", "bje"});
    if (!rmCls) { ntLog("initIds: FAIL rmCls"); return; }
    m_ids.rm_viewerPosX =
        (void *)lc->GetFieldID(rmCls, "viewerPosX", "D", "field_78725_b", "o");
    m_ids.rm_viewerPosY =
        (void *)lc->GetFieldID(rmCls, "viewerPosY", "D", "field_78726_c", "p");
    m_ids.rm_viewerPosZ =
        (void *)lc->GetFieldID(rmCls, "viewerPosZ", "D", "field_78723_d", "q");
    m_ids.rm_playerViewX =
        (void *)lc->GetFieldID(rmCls, "playerViewX", "F", "field_78732_j", "f");
    m_ids.rm_playerViewY =
        (void *)lc->GetFieldID(rmCls, "playerViewY", "F", "field_78735_i", "e");

    jclass entCls = findCls("net.minecraft.entity.Entity",
                            {"pk", "rt", "qx"});
    if (!entCls) { ntLog("initIds: FAIL entCls"); return; }
    m_ids.ent_posX =
        (void *)lc->GetFieldID(entCls, "posX", "D", "field_70165_t", "s");
    m_ids.ent_posY =
        (void *)lc->GetFieldID(entCls, "posY", "D", "field_70163_u", "t");
    m_ids.ent_posZ =
        (void *)lc->GetFieldID(entCls, "posZ", "D", "field_70161_v", "u");
    m_ids.ent_prevX =
        (void *)lc->GetFieldID(entCls, "prevPosX", "D", "field_70169_q", "p");
    m_ids.ent_prevY =
        (void *)lc->GetFieldID(entCls, "prevPosY", "D", "field_70167_r", "q");
    m_ids.ent_prevZ =
        (void *)lc->GetFieldID(entCls, "prevPosZ", "D", "field_70166_s", "r");

    jclass epCls = findCls("net.minecraft.entity.player.EntityPlayer",
                           {"zw", "wn", "ahd", "xe", "yw"});
    if (!epCls) { ntLog("initIds: FAIL epCls"); return; }
    m_ids.player_getCommandSenderName = (void *)lc->GetMethodID(
        epCls, "getName", "()Ljava/lang/String;", "func_70005_c_", "Q",
        "()Ljava/lang/String;");
    m_ids.player_getGameProfile = (void *)lc->GetMethodID(
        epCls, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
        "func_146103_bH", "bC", "()Lcom/mojang/authlib/GameProfile;");
    if (!m_ids.player_getGameProfile) {
      m_ids.player_getGameProfile = (void *)env->GetMethodID(
          epCls, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }

    jclass gpCls = env->FindClass("com/mojang/authlib/GameProfile");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (gpCls) {
      m_ids.gameProfile_getName =
          (void *)env->GetMethodID(gpCls, "getName", "()Ljava/lang/String;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      env->DeleteLocalRef(gpCls);
    }

    jclass worldCls = findCls("net.minecraft.world.World",
                              {"ahb", "ahq", "ahr"});
    if (!worldCls) { ntLog("initIds: FAIL worldCls"); return; }
    m_ids.world_playerEntities = nullptr;
    auto tryWorldList = [&](const char *name) {
      if (m_ids.world_playerEntities) return;
      m_ids.world_playerEntities =
          (void *)env->GetFieldID(worldCls, name, "Ljava/util/List;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (m_ids.world_playerEntities)
        ntLog("initIds: world.playerEntities matched obf '%s'", name);
    };
    tryWorldList("playerEntities");
    tryWorldList("field_73010_i");
    tryWorldList("k");
    tryWorldList("j");
    tryWorldList("i");
    tryWorldList("l");
    tryWorldList("m");
    tryWorldList("h");
    tryWorldList("g");
    if (!m_ids.world_playerEntities) {
      ntLog("initIds: FAIL world.playerEntities (no name matched)");
      return;
    }

    jclass listCls = env->FindClass("java/util/List");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (listCls) {
      m_ids.list_size = (void *)env->GetMethodID(listCls, "size", "()I");
      m_ids.list_get =
          (void *)env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
      env->DeleteLocalRef(listCls);
    }

    jclass gsCls = findCls("net.minecraft.client.settings.GameSettings",
                           {"avh", "avg", "avi"});
    if (!gsCls) { ntLog("initIds: FAIL gsCls"); return; }
    m_ids.gs_fovSetting =
        (void *)lc->GetFieldID(gsCls, "fovSetting", "F", "field_74334_X", "aI");

    jclass timerCls = findCls("net.minecraft.util.Timer",
                              {"avl", "avk", "avm"});
    if (!timerCls) { ntLog("initIds: FAIL timerCls"); return; }
    m_ids.timer_partialTicks = (void *)lc->GetFieldID(
        timerCls, "renderPartialTicks", "F", "field_74281_c", "c");

    {
      jclass erCls = findCls("net.minecraft.client.renderer.EntityRenderer",
                             {"bfb", "bfa", "bfc"});
      if (erCls) {
        jclass mcClsLocal = findCls("net.minecraft.client.Minecraft",
                                    {"ave", "avc", "avd"});
        if (mcClsLocal) {
          m_ids.mc_entityRenderer = (void *)lc->GetFieldID(
              mcClsLocal, "entityRenderer",
              "Lnet/minecraft/client/renderer/EntityRenderer;",
              "field_71460_t", "j", "Lbfb;");
        }
        m_ids.er_getFOVModifier = (void *)lc->GetMethodID(
            erCls, "getFOVModifier", "(FZ)F", "func_78481_a", "a");
        m_ids.er_thirdPersonDistanceTemp = (void *)lc->GetFieldID(
            erCls, "thirdPersonDistanceTemp", "F", "field_78491_C", "C");
      }
    }

    m_ids.gs_thirdPersonView = (void *)lc->GetFieldID(
        gsCls, "thirdPersonView", "I", "field_74320_O", "aA");
    if (!m_ids.gs_thirdPersonView) {
      m_ids.gs_thirdPersonView =
          (void *)env->GetFieldID(gsCls, "thirdPersonView", "I");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!m_ids.gs_thirdPersonView) {
      m_ids.gs_thirdPersonView =
          (void *)env->GetFieldID(gsCls, "field_74320_O", "I");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }

    jclass ariCls = findCls("net.minecraft.client.renderer.ActiveRenderInfo",
                            {"bex", "bey", "bez"});
    if (ariCls) {
      m_ids.ari_MODELVIEW = (void *)lc->GetStaticFieldID(
          ariCls, "MODELVIEW", "Ljava/nio/FloatBuffer;", "field_178812_b", "b");
      m_ids.ari_PROJECTION = (void *)lc->GetStaticFieldID(
          ariCls, "PROJECTION", "Ljava/nio/FloatBuffer;", "field_178813_c", "c");
      m_ids.ari_VIEWPORT = (void *)lc->GetStaticFieldID(
          ariCls, "VIEWPORT", "Ljava/nio/IntBuffer;", "field_178814_a", "a");
    } else {
      m_ids.ari_MODELVIEW = nullptr;
      m_ids.ari_PROJECTION = nullptr;
      m_ids.ari_VIEWPORT = nullptr;
    }

    jclass fontCls = findCls("net.minecraft.client.gui.FontRenderer",
                             {"avn", "avo", "avp"});
    if (!fontCls) { ntLog("initIds: FAIL fontCls"); return; }
    m_ids.font_drawString =
        (void *)lc->GetMethodID(fontCls, "drawStringWithShadow",
                                "(Ljava/lang/String;FFI)I", "func_175063_a", "a");
    m_ids.font_getStringWidth = (void *)lc->GetMethodID(
        fontCls, "getStringWidth", "(Ljava/lang/String;)I", "func_78256_a", "a");

    m_ids.initialized = true;
    Logger::info("NameTagRenderer: JNI cache ready");
    ntLog("initIds: ALL OK — JNI cache ready");
  } catch (...) {
    Logger::error("NameTagRenderer: initIds failed");
    ntLog("initIds: EXCEPTION thrown during initialisation");
  }
}

struct NameTagDraw {
  std::string label;
  float pixelX = 0;
  float pixelY = 0;
  float scale  = 1.0f;
  double dist  = 0.0;
};

void NameTagRenderer::render(void *hdcPtr, double partialTicksManual) {
  (void)hdcPtr;

  const bool logThisPass = shouldLogNow();
  if (logThisPass)
    ntLog("--- render() entry: cfgEnabled=%d lc=%p ---",
          Config::isNameTagsEnabled() ? 1 : 0, (void *)lc);

  if (!Config::isNameTagsEnabled()) {
    if (logThisPass) ntLog("  gate: config disabled");
    return;
  }
  if (!lc) {
    if (logThisPass) ntLog("  gate: lc null");
    return;
  }
  JNIEnv *env = lc->getEnv();
  if (!env) {
    if (logThisPass) ntLog("  gate: env null");
    return;
  }

  initIds();
  if (!m_ids.initialized) {
    if (logThisPass) ntLog("  gate: JNI ids not initialised");
    return;
  }

  double camX = 0, camY = 0, camZ = 0;
  double feetY = 0;
  float rotationYaw = 0, rotationPitch = 0;
  float fovSetting = 70.0f;
  float pt = (float)partialTicksManual;
  float thirdPersonDist = 0.0f;
  int thirdPersonMode = 0;

  jobject mcObj = nullptr;
  jobject worldObj = nullptr;
  jobject playerList = nullptr;
  jobject viewerEntity = nullptr;
  jobject fontObj = nullptr;

  const char *step = "(none)";
  try {
    if (logThisPass) ntLog("  gate: entering camera-state try");
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) mcCls = env->FindClass("ave");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!mcCls) mcCls = env->FindClass("avc");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!mcCls) mcCls = env->FindClass("avd");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!mcCls) {
      if (logThisPass) ntLog("  gate: mcCls NULL (Minecraft class)");
      return;
    }
    mcObj = env->GetStaticObjectField(mcCls, (jfieldID)m_ids.mc_theMc);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      if (logThisPass) ntLog("  gate: theMc static-field threw");
    }
    if (!mcObj) {
      if (logThisPass)
        ntLog("  gate: mcObj NULL — theMc fieldID=%p (Minecraft.theMinecraft)",
              m_ids.mc_theMc);
      return;
    }
    if (logThisPass) ntLog("  gate: mcObj=%p OK", (void *)mcObj);

    step = "GetObjectField(mc.timer)";

    jobject timerObj = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_timer);
    if (timerObj) {
      step = "GetFloatField(timer.partialTicks)";
      float realPt = env->GetFloatField(timerObj, (jfieldID)m_ids.timer_partialTicks);
      if (realPt > 0 && realPt <= 1.0f) pt = realPt;
      env->DeleteLocalRef(timerObj);
    }

    step = "GetObjectField(mc.renderManager)";
    jobject rm = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_renderManager);
    if (rm) {
      step = "rm.viewerPosX";
      camX = env->GetDoubleField(rm, (jfieldID)m_ids.rm_viewerPosX);
      step = "rm.viewerPosY";
      camY = env->GetDoubleField(rm, (jfieldID)m_ids.rm_viewerPosY);
      step = "rm.viewerPosZ";
      camZ = env->GetDoubleField(rm, (jfieldID)m_ids.rm_viewerPosZ);
      feetY = camY;
      camY += 1.62;
      step = "rm.playerViewX";
      rotationPitch = env->GetFloatField(rm, (jfieldID)m_ids.rm_playerViewX);
      step = "rm.playerViewY";
      rotationYaw = env->GetFloatField(rm, (jfieldID)m_ids.rm_playerViewY);
      env->DeleteLocalRef(rm);
    } else {
      jobject ve = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_renderViewEntity);
      if (ve) {
        double cx = env->GetDoubleField(ve, (jfieldID)m_ids.ent_posX);
        double cy = env->GetDoubleField(ve, (jfieldID)m_ids.ent_posY);
        double cz = env->GetDoubleField(ve, (jfieldID)m_ids.ent_posZ);
        double px = env->GetDoubleField(ve, (jfieldID)m_ids.ent_prevX);
        double py = env->GetDoubleField(ve, (jfieldID)m_ids.ent_prevY);
        double pz = env->GetDoubleField(ve, (jfieldID)m_ids.ent_prevZ);
        camX = px + (cx - px) * pt;
        feetY = py + (cy - py) * pt;
        camY = feetY + 1.62;
        camZ = pz + (cz - pz) * pt;
        env->DeleteLocalRef(ve);
      }
    }

    step = "GetObjectField(mc.gameSettings)";
    jobject settings = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_gameSettings);
    if (settings) {
      step = "gs.fovSetting";
      fovSetting = env->GetFloatField(settings, (jfieldID)m_ids.gs_fovSetting);
      if (m_ids.gs_thirdPersonView) {
        step = "gs.thirdPersonView";
        thirdPersonMode =
            env->GetIntField(settings, (jfieldID)m_ids.gs_thirdPersonView);
      }
      env->DeleteLocalRef(settings);
    }

    if (m_ids.mc_entityRenderer) {
      step = "GetObjectField(mc.entityRenderer)";
      jobject er = env->GetObjectField(mcObj,
                                       (jfieldID)m_ids.mc_entityRenderer);
      if (er) {
        if (m_ids.er_getFOVModifier) {
          step = "er.getFOVModifier()";
          jfloat realFov = env->CallFloatMethod(
              er, (jmethodID)m_ids.er_getFOVModifier,
              (jfloat)pt, (jboolean)JNI_TRUE);
          if (env->ExceptionCheck()) {
            env->ExceptionClear();
          } else if (realFov > 1.0f && realFov < 180.0f) {
            fovSetting = (float)realFov;
          }
        }
        if (m_ids.er_thirdPersonDistanceTemp) {
          step = "er.thirdPersonDistanceTemp";
          thirdPersonDist = env->GetFloatField(
              er, (jfieldID)m_ids.er_thirdPersonDistanceTemp);
        }
        env->DeleteLocalRef(er);
      }
    }

    step = "GetObjectField(mc.theWorld)";
    worldObj = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_theWorld);
    if (!worldObj) {
      if (logThisPass) ntLog("  gate: worldObj NULL "
                             "(mc.theWorld field returned null — "
                             "are we on the main menu?)");
      env->DeleteLocalRef(mcObj);
      return;
    }

    void *acFid = Anticheat::getWorldPlayerEntitiesFieldID();
    jfieldID effFid = acFid ? (jfieldID)acFid
                            : (jfieldID)m_ids.world_playerEntities;
    step = "GetObjectField(world.playerEntities)";
    playerList = env->GetObjectField(worldObj, effFid);
    step = "GetObjectField(mc.renderViewEntity)";
    viewerEntity = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_renderViewEntity);
    step = "GetObjectField(mc.fontRendererObj)";
    fontObj = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_fontRendererObj);
    step = "(camera-state complete)";
    if (logThisPass)
      ntLog("  pre-checks: worldObj=%p playerList=%p viewerEntity=%p fontObj=%p",
            (void *)worldObj, (void *)playerList,
            (void *)viewerEntity, (void *)fontObj);
  } catch (...) {
    if (logThisPass)
      ntLog("  gate: EXCEPTION in camera-state try block at step '%s'",
            step);
    if (mcObj) env->DeleteLocalRef(mcObj);
    return;
  }

  if (!playerList || !fontObj) {
    if (logThisPass)
      ntLog("  gate: playerList=%p fontObj=%p — one is NULL, bailing",
            (void *)playerList, (void *)fontObj);
    if (worldObj) env->DeleteLocalRef(worldObj);
    if (playerList) env->DeleteLocalRef(playerList);
    if (viewerEntity) env->DeleteLocalRef(viewerEntity);
    if (fontObj) env->DeleteLocalRef(fontObj);
    if (mcObj) env->DeleteLocalRef(mcObj);
    return;
  }

  bool useActiveRenderInfo = false;
  float modelview[16];
  float projection[16];
  int viewport[4];

  if (m_ids.ari_MODELVIEW && m_ids.ari_PROJECTION && m_ids.ari_VIEWPORT) {
    jclass ariCls =
        lc->GetClass("net.minecraft.client.renderer.ActiveRenderInfo");
    if (!ariCls) ariCls = env->FindClass("bex");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!ariCls) ariCls = env->FindClass("bey");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (ariCls) {
      jobject modelviewBuf = env->GetStaticObjectField(ariCls, (jfieldID)m_ids.ari_MODELVIEW);
      jobject projectionBuf = env->GetStaticObjectField(ariCls, (jfieldID)m_ids.ari_PROJECTION);
      jobject viewportBuf = env->GetStaticObjectField(ariCls, (jfieldID)m_ids.ari_VIEWPORT);

      if (modelviewBuf && projectionBuf) {
        if (readFloatBuffer(env, modelviewBuf, modelview, 16) &&
            readFloatBuffer(env, projectionBuf, projection, 16)) {
          useActiveRenderInfo = true;
          bool hasViewport = readIntBuffer(env, viewportBuf, viewport, 4);
          if (!hasViewport) {
            glGetIntegerv(GL_VIEWPORT, (GLint*)viewport);
          }
        }
      }

      if (modelviewBuf) env->DeleteLocalRef(modelviewBuf);
      if (projectionBuf) env->DeleteLocalRef(projectionBuf);
      if (viewportBuf) env->DeleteLocalRef(viewportBuf);
    }
  }

  if (!useActiveRenderInfo) {
    glGetIntegerv(GL_VIEWPORT, (GLint*)viewport);
  }

  if (viewport[2] <= 0 || viewport[3] <= 0) {
    if (logThisPass)
      ntLog("  gate: viewport degenerate (%d %d %d %d)",
            viewport[0], viewport[1], viewport[2], viewport[3]);
    if (worldObj) env->DeleteLocalRef(worldObj);
    if (playerList) env->DeleteLocalRef(playerList);
    if (viewerEntity) env->DeleteLocalRef(viewerEntity);
    if (fontObj) env->DeleteLocalRef(fontObj);
    if (mcObj) env->DeleteLocalRef(mcObj);
    return;
  }
  const float vpW = (float)viewport[2];
  const float vpH = (float)viewport[3];
  const float aspect = vpW / vpH;

  const double yawRad =
      (double)(rotationYaw + 180.0f) * 3.14159265358979 / 180.0;
  const double pitchRad =
      (double)(rotationPitch) * 3.14159265358979 / 180.0;
  const double sinYaw = sin(yawRad);
  const double cosYaw = cos(yawRad);
  const double sinPitch = sin(pitchRad);
  const double cosPitch = cos(pitchRad);

  const double fovHalfRad = (double)fovSetting * 0.5 * 3.14159265358979 / 180.0;
  const double f = 1.0 / tan(fovHalfRad);

  jint listSize = env->CallIntMethod(playerList, (jmethodID)m_ids.list_size);
  if (env->ExceptionCheck()) { env->ExceptionClear(); listSize = 0; }

  std::vector<NameTagDraw> draws;
  draws.reserve((size_t)listSize);

  if (logThisPass) {
    std::string acNames;
    size_t acCount = Anticheat::getTrackedPlayersSnapshot(acNames);
    ntLog("======== pass listSize=%d tpMode=%d tpDist=%.2f fov=%.1f "
          "ACtracked=%zu acNames={%s}",
          (int)listSize, thirdPersonMode, thirdPersonDist, fovSetting,
          acCount, acNames.c_str());
    ntLog("  cam: pos=(%.2f,%.2f,%.2f) feetY=%.2f yaw=%.1f pitch=%.1f",
          camX, camY, camZ, feetY, rotationYaw, rotationPitch);
    ntLog("  proj: f=%.3f aspect=%.3f vpW=%.0f vpH=%.0f useARI=%d",
          f, aspect, vpW, vpH, useActiveRenderInfo ? 1 : 0);
    if (useActiveRenderInfo) {
      ntLog("  MV row0: %7.3f %7.3f %7.3f %7.3f",
            modelview[0], modelview[4], modelview[8], modelview[12]);
      ntLog("  MV row1: %7.3f %7.3f %7.3f %7.3f",
            modelview[1], modelview[5], modelview[9], modelview[13]);
      ntLog("  MV row2: %7.3f %7.3f %7.3f %7.3f",
            modelview[2], modelview[6], modelview[10], modelview[14]);
      ntLog("  MV row3: %7.3f %7.3f %7.3f %7.3f",
            modelview[3], modelview[7], modelview[11], modelview[15]);
      ntLog("  PR row0: %7.3f %7.3f %7.3f %7.3f",
            projection[0], projection[4], projection[8], projection[12]);
      ntLog("  PR row1: %7.3f %7.3f %7.3f %7.3f",
            projection[1], projection[5], projection[9], projection[13]);
      ntLog("  PR row2: %7.3f %7.3f %7.3f %7.3f",
            projection[2], projection[6], projection[10], projection[14]);
      ntLog("  PR row3: %7.3f %7.3f %7.3f %7.3f",
            projection[3], projection[7], projection[11], projection[15]);
    }
  }

  SafeGuard::installSehTranslator();
  for (jint i = 0; i < listSize; ++i) {
    SafeGuard::run("NameTag/player-iter", [&]() {
    jobject player =
        env->CallObjectMethod(playerList, (jmethodID)m_ids.list_get, i);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!player) { if (logThisPass) ntLog("  [%d] NULL entity", (int)i); return; }

    if (thirdPersonMode == 0 && viewerEntity &&
        env->IsSameObject(player, viewerEntity)) {
      if (logThisPass) ntLog("  [%d] SELF-skip (first-person)", (int)i);
      env->DeleteLocalRef(player);
      return;
    }

    double cx = env->GetDoubleField(player, (jfieldID)m_ids.ent_posX);
    double cy = env->GetDoubleField(player, (jfieldID)m_ids.ent_posY);
    double cz = env->GetDoubleField(player, (jfieldID)m_ids.ent_posZ);
    double px = env->GetDoubleField(player, (jfieldID)m_ids.ent_prevX);
    double py = env->GetDoubleField(player, (jfieldID)m_ids.ent_prevY);
    double pz = env->GetDoubleField(player, (jfieldID)m_ids.ent_prevZ);
    double wx = px + (cx - px) * pt;
    double wy = py + (cy - py) * pt + (double)Config::getNameTagHeight();
    double wz = pz + (cz - pz) * pt;

    double rx = wx - camX;
    double ry = wy - (useActiveRenderInfo ? feetY : camY);
    double rz = wz - camZ;

    double ndcX = 0.0, ndcY = 0.0;
    double dist = 0.0;

    double mVx = rx * cosYaw + rz * sinYaw;
    double mVz1 = -rx * sinYaw + rz * cosYaw;
    double mVy = (wy - camY) * cosPitch - mVz1 * sinPitch;
    double mVz2 = (wy - camY) * sinPitch + mVz1 * cosPitch;
    if (thirdPersonMode == 2) { mVx = -mVx; mVz2 = -mVz2; }
    if (thirdPersonMode > 0 && thirdPersonDist > 0.01f)
      mVz2 -= (double)thirdPersonDist;
    double manualNdcX = (mVz2 < -0.05)
                           ? (f / aspect) * mVx / -mVz2 : 999.0;
    double manualNdcY = (mVz2 < -0.05) ? f * mVy / -mVz2 : 999.0;

    if (useActiveRenderInfo) {
      Vec4 pos{ rx, ry, rz, 1.0 };
      Vec4 eyeSpace = matMul(modelview, pos);
      Vec4 clipSpace = matMul(projection, eyeSpace);
      double ariNdcX = (clipSpace.w != 0.0) ? clipSpace.x / clipSpace.w : 999.0;
      double ariNdcY = (clipSpace.w != 0.0) ? clipSpace.y / clipSpace.w : 999.0;

      if (logThisPass && i < 4) {
        ntLog("  [%d] world=(%.2f,%.2f,%.2f) rel=(%.2f,%.2f,%.2f)",
              (int)i, wx, wy, wz, rx, ry, rz);
        ntLog("        ARI eye=(%.2f,%.2f,%.2f,%.2f) clip=(%.2f,%.2f,%.2f,%.2f) ndc=(%.3f,%.3f)",
              eyeSpace.x, eyeSpace.y, eyeSpace.z, eyeSpace.w,
              clipSpace.x, clipSpace.y, clipSpace.z, clipSpace.w,
              ariNdcX, ariNdcY);
        ntLog("        MAN eye=(%.2f,%.2f,%.2f) ndc=(%.3f,%.3f)",
              mVx, mVy, mVz2, manualNdcX, manualNdcY);
      }

      if (eyeSpace.z >= -0.05) {
        if (logThisPass) ntLog("  [%d] behind-cam (ARI) eyeZ=%.2f manVz=%.2f",
                               (int)i, eyeSpace.z, mVz2);
        env->DeleteLocalRef(player);
        return;
      }

      if (clipSpace.w == 0.0) {
        env->DeleteLocalRef(player);
        return;
      }

      ndcX = ariNdcX;
      ndcY = ariNdcY;
      dist = sqrt(eyeSpace.x * eyeSpace.x +
                  eyeSpace.y * eyeSpace.y +
                  eyeSpace.z * eyeSpace.z);
    } else {
      double vx = rx * cosYaw + rz * sinYaw;
      double vz1 = -rx * sinYaw + rz * cosYaw;
      double vy = ry * cosPitch - vz1 * sinPitch;
      double vz2 = ry * sinPitch + vz1 * cosPitch;
      if (thirdPersonMode == 2) {
        vx = -vx;
        vz2 = -vz2;
      }
      if (thirdPersonMode > 0 && thirdPersonDist > 0.01f)
        vz2 -= (double)thirdPersonDist;

      if (vz2 >= -0.05) {
        if (logThisPass) ntLog("  [%d] behind-cam vz2=%.2f", (int)i, vz2);
        env->DeleteLocalRef(player);
        return;
      }

      ndcX = (f / aspect) * vx / -vz2;
      ndcY = f * vy / -vz2;
      dist = sqrt((wx - camX) * (wx - camX) + (wy - camY - 2.4) * (wy - camY - 2.4) +
                  (wz - camZ) * (wz - camZ));
    }

    if (ndcX < -1.2 || ndcX > 1.2 || ndcY < -1.2 || ndcY > 1.2) {
      if (logThisPass) ntLog("  [%d] off-screen ndc=(%.2f,%.2f)",
                             (int)i, ndcX, ndcY);
      env->DeleteLocalRef(player);
      return;
    }

    float pixelX = (float)(viewport[0] + viewport[2] * (ndcX * 0.5 + 0.5));
    float pixelY = (float)(viewport[1] + viewport[3] * (1.0 - (ndcY * 0.5 + 0.5)));

    if (dist < 0.5 || dist > 64.0) {
      if (logThisPass) ntLog("  [%d] dist-bounds dist=%.1f", (int)i, dist);
      env->DeleteLocalRef(player);
      return;
    }

    std::string profileName, entityName, name;
    if (m_ids.player_getGameProfile && m_ids.gameProfile_getName) {
      jobject profile = env->CallObjectMethod(
          player, (jmethodID)m_ids.player_getGameProfile);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      } else if (profile) {
        jstring jn = (jstring)env->CallObjectMethod(
            profile, (jmethodID)m_ids.gameProfile_getName);
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
        } else if (jn) {
          const char *c = env->GetStringUTFChars(jn, nullptr);
          if (c) profileName.assign(c);
          env->ReleaseStringUTFChars(jn, c);
          env->DeleteLocalRef(jn);
        }
        env->DeleteLocalRef(profile);
      }
    }
    if (m_ids.player_getCommandSenderName) {
      jstring jn = (jstring)env->CallObjectMethod(
          player, (jmethodID)m_ids.player_getCommandSenderName);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      } else if (jn) {
        const char *c = env->GetStringUTFChars(jn, nullptr);
        if (c) entityName.assign(c);
        env->ReleaseStringUTFChars(jn, c);
        env->DeleteLocalRef(jn);
      }
    }
    name = !profileName.empty() ? profileName : entityName;
    if (name.empty()) {
      if (logThisPass)
        ntLog("  [%d] NO NAME (both profile and entity returned empty)", (int)i);
      env->DeleteLocalRef(player);
      return;
    }

    bool sneaking = Anticheat::isPlayerSneaking(name);
    if (logThisPass)
      ntLog("  [%d] sneak-check name='%s' result=%d", (int)i,
            name.c_str(), sneaking ? 1 : 0);
    if (sneaking) {
      env->DeleteLocalRef(player);
      return;
    }

    int star = 0;
    bool isNicked = false;
    bool foundStats = false;
    Hypixel::PlayerStats statsCopy;
    std::string hitKey;
    std::string cleanName, lowerName, stripped;
    size_t cacheSize = 0;
    {
      std::lock_guard<std::mutex> lk(OVson::g_statsMutex);
      cacheSize = OVson::g_playerStatsMap.size();

      for (size_t k = 0; k < name.size(); ++k) {
        unsigned char ch = (unsigned char)name[k];
        if (ch == 0xC2 && k + 2 < name.size() &&
            (unsigned char)name[k + 1] == 0xA7) {
          k += 2; // skip § + code byte
          continue;
        }
        stripped += (char)ch;
      }

      for (char c : stripped) {
        if (c >= 'A' && c <= 'Z')
          cleanName += (char)(c + 32);
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
          cleanName += c;
      }
      lowerName = stripped;
      for (auto &ch2 : lowerName) {
        if (ch2 >= 'A' && ch2 <= 'Z')
          ch2 += 32;
      }

      auto it = OVson::g_playerStatsMap.find(cleanName);
      if (it != OVson::g_playerStatsMap.end()) hitKey = "cleanName";
      if (it == OVson::g_playerStatsMap.end()) {
        it = OVson::g_playerStatsMap.find(name);
        if (it != OVson::g_playerStatsMap.end()) hitKey = "name";
      }
      if (it == OVson::g_playerStatsMap.end()) {
        it = OVson::g_playerStatsMap.find(lowerName);
        if (it != OVson::g_playerStatsMap.end()) hitKey = "lowerName";
      }

      if (it != OVson::g_playerStatsMap.end()) {
        star = it->second.bedwarsStar;
        isNicked = it->second.isNicked;
        statsCopy = it->second;
        foundStats = true;
      }
    }

    if (foundStats && statsCopy.inGameHealth == 0) {
      if (logThisPass) ntLog("  [%d] dead-skip name='%s'",
                             (int)i, name.c_str());
      env->DeleteLocalRef(player);
      return;
    }

    if (logThisPass) {
      ntLog("  [%d] profile='%s' entity='%s' stripped='%s' clean='%s' "
            "lower='%s' found=%d hitKey=%s star=%d nicked=%d cacheSize=%zu",
            (int)i, profileName.c_str(), entityName.c_str(),
            stripped.c_str(), cleanName.c_str(), lowerName.c_str(),
            foundStats ? 1 : 0,
            foundStats ? hitKey.c_str() : "-",
            star, isNicked ? 1 : 0, cacheSize);
    }

    if (!foundStats) {
      const std::string &fetchKey = !stripped.empty() ? stripped : name;
      OVson::requestStatsForVisiblePlayer(fetchKey);
      if (logThisPass)
        ntLog("    -> requested fetch for '%s'", fetchKey.c_str());
      env->DeleteLocalRef(player);
      return;
    }

    NameTagDraw d;
    if (isNicked) {
      d.label = "\xC2\xA7" "4[NICKED]";
    } else {
      auto slots = Config::getNameTagStats();
      auto fmtF = [](float v) {
        char b[16];
        sprintf_s(b, "%.2f", v);
        return std::string(b);
      };
      auto fmtI = [](int v) { return std::to_string(v); };

      const float fkdr = statsCopy.bedwarsFinalDeaths > 0
          ? (float)statsCopy.bedwarsFinalKills /
                (float)statsCopy.bedwarsFinalDeaths
          : (float)statsCopy.bedwarsFinalKills;
      const float wlr = statsCopy.bedwarsLosses > 0
          ? (float)statsCopy.bedwarsWins /
                (float)statsCopy.bedwarsLosses
          : (float)statsCopy.bedwarsWins;

      std::string label;
      for (const auto &slot : slots) {
        if (!slot.second) continue;
        std::string part;
        if (slot.first == "star") {
          part = BedwarsStars::GetFormattedLevel(statsCopy.bedwarsStar);
        } else if (slot.first == "fkdr") {
          part = "\xC2\xA7" "e" "FKDR " "\xC2\xA7" "f" + fmtF(fkdr);
        } else if (slot.first == "fk") {
          part = "\xC2\xA7" "e" "FK " "\xC2\xA7" "f" + fmtI(statsCopy.bedwarsFinalKills);
        } else if (slot.first == "wins") {
          part = "\xC2\xA7" "e" "W " "\xC2\xA7" "f" + fmtI(statsCopy.bedwarsWins);
        } else if (slot.first == "wlr") {
          part = "\xC2\xA7" "e" "WLR " "\xC2\xA7" "f" + fmtF(wlr);
        } else if (slot.first == "ws") {
          part = "\xC2\xA7" "e" "WS " "\xC2\xA7" "f" + fmtI(statsCopy.winstreak);
        }
        if (part.empty()) continue;
        if (!label.empty()) label += " \xC2\xA7" "7|\xC2\xA7" "f ";
        label += part;
      }
      if (label.empty()) {
        env->DeleteLocalRef(player);
        return;
      }
      d.label = label;
    }
    d.pixelX = pixelX;
    d.pixelY = pixelY;
    float activeF = useActiveRenderInfo ? projection[5] : (float)f;
    float s = (float)(activeF / dist) * 8.0f;
    if (s < 0.45f) s = 0.45f;
    if (s > 18.0f) s = 18.0f;
    d.scale = s;
    d.dist = dist;
    draws.push_back(std::move(d));

    env->DeleteLocalRef(player);
    });
  }

  std::sort(draws.begin(), draws.end(),
            [](const NameTagDraw &a, const NameTagDraw &b) {
              return a.dist > b.dist;
            });

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT |
               GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, vpW, vpH, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  for (const auto &d : draws) {
    SafeGuard::run("NameTag/draw-label", [&]() {
    jstring jlabel = env->NewStringUTF(d.label.c_str());
    if (!jlabel) return;

    int textWidth = 0;
    if (m_ids.font_getStringWidth) {
      textWidth = env->CallIntMethod(fontObj,
                                     (jmethodID)m_ids.font_getStringWidth, jlabel);
      if (env->ExceptionCheck()) { env->ExceptionClear(); textWidth = 0; }
    }
    if (textWidth <= 0) {
      env->DeleteLocalRef(jlabel);
      return;
    }

    glPushMatrix();
    glTranslatef(d.pixelX, d.pixelY, 0.0f);
    glScalef(d.scale, d.scale, 1.0f);

    float halfW = (float)textWidth / 2.0f + 2.0f;
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
    glBegin(GL_QUADS);
    glVertex2f(-halfW, -5.0f);
    glVertex2f( halfW, -5.0f);
    glVertex2f( halfW,  4.0f);
    glVertex2f(-halfW,  4.0f);
    glEnd();
    glEnable(GL_TEXTURE_2D);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    env->CallIntMethod(fontObj, (jmethodID)m_ids.font_drawString, jlabel,
                       (jfloat)(-textWidth / 2.0f), (jfloat)(-4.0f),
                       (jint)0xFFFFFFFF);
    if (env->ExceptionCheck()) env->ExceptionClear();

    glPopMatrix();
    env->DeleteLocalRef(jlabel);
    }); // SafeGuard::run draw
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glDepthMask(GL_TRUE);
  glPopAttrib();

  if (worldObj) env->DeleteLocalRef(worldObj);
  if (playerList) env->DeleteLocalRef(playerList);
  if (viewerEntity) env->DeleteLocalRef(viewerEntity);
  if (fontObj) env->DeleteLocalRef(fontObj);
  if (mcObj) env->DeleteLocalRef(mcObj);
}

} // namespace OVson
