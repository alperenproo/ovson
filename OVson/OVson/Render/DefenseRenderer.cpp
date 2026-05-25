#include "DefenseRenderer.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Utils/Logger.h"
#include "TextureLoader.h"
#include <algorithm>
#include <cmath>
#include <vector>


namespace BedDefense {
DefenseRenderer *DefenseRenderer::s_instance = nullptr;

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


static void drawCross(float x, float y, float radius, int color) {
  float alpha = (float)((color >> 24) & 0xFF) / 255.0f;
  float red = (float)((color >> 16) & 0xFF) / 255.0f;
  float green = (float)((color >> 8) & 0xFF) / 255.0f;
  float blue = (float)(color & 0xFF) / 255.0f;

  glColor4f(red, green, blue, alpha);
  glDisable(GL_TEXTURE_2D);
  glLineWidth(2.0f);

  glBegin(GL_LINES);
  glVertex2f(x - radius, y - radius);
  glVertex2f(x + radius, y + radius);

  glVertex2f(x + radius, y - radius);
  glVertex2f(x - radius, y + radius);
  glEnd();
}

DefenseRenderer::DefenseRenderer() {
  m_ids.initialized = false;
  Logger::info("DefenseRenderer initialized");
}

void DefenseRenderer::initIds(void *envPtr) {
  JNIEnv *env = (JNIEnv *)envPtr;
  if (m_ids.initialized)
    return;

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls)
      return;
    m_ids.mc_theMc = (void *)lc->GetStaticFieldID(
        mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;",
        "field_71432_P", "S", "Lave;");

    m_ids.mc_renderViewEntity = (void *)lc->GetFieldID(
        mcCls, "renderViewEntity", "Lnet/minecraft/entity/Entity;",
        "field_175622_Z", "ad", "Lpk;");

    m_ids.mc_gameSettings = (void *)lc->GetFieldID(
        mcCls, "gameSettings", "Lnet/minecraft/client/settings/GameSettings;",
        "field_71474_y", "t", "Lavh;");

    m_ids.mc_timer =
        (void *)lc->GetFieldID(mcCls, "timer", "Lnet/minecraft/util/Timer;",
                               "field_71428_T", "Y", "Lavl;");

    m_ids.mc_renderManager = (void *)lc->GetFieldID(
        mcCls, "renderManager",
        "Lnet/minecraft/client/renderer/entity/RenderManager;",
        "field_175616_W", "aa", "Lbiu;");

    m_ids.mc_getTextureManager = (void *)lc->GetMethodID(
        mcCls, "getTextureManager",
        "()Lnet/minecraft/client/renderer/texture/TextureManager;",
        "func_110434_K", "P", "()Lbmj;");

    jclass rmCls =
        lc->GetClass("net.minecraft.client.renderer.entity.RenderManager");
    if (!rmCls)
      return;
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

    jclass entCls = lc->GetClass("net.minecraft.entity.Entity");
    if (!entCls)
      return;
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
    m_ids.ent_yaw = (void *)lc->GetFieldID(entCls, "rotationYaw", "F",
                                           "field_70177_z", "y");
    m_ids.ent_pitch = (void *)lc->GetFieldID(entCls, "rotationPitch", "F",
                                             "field_70125_A", "z");

    jclass gsCls = lc->GetClass("net.minecraft.client.settings.GameSettings");
    if (!gsCls)
      return;
    m_ids.gs_fovSetting =
        (void *)lc->GetFieldID(gsCls, "fovSetting", "F", "field_74334_X", "aI");

    jclass timerCls = lc->GetClass("net.minecraft.util.Timer");
    if (!timerCls)
      return;
    m_ids.timer_partialTicks = (void *)lc->GetFieldID(
        timerCls, "renderPartialTicks", "F", "field_74281_c", "c");

    {
      jclass erCls =
          lc->GetClass("net.minecraft.client.renderer.EntityRenderer");
      if (erCls) {
        m_ids.mc_entityRenderer = (void *)lc->GetFieldID(
            mcCls, "entityRenderer",
            "Lnet/minecraft/client/renderer/EntityRenderer;",
            "field_71460_t", "j", "Lbfb;");
        m_ids.er_getFOVModifier = (void *)lc->GetMethodID(
            erCls, "getFOVModifier", "(FZ)F", "func_78481_a", "a");
        m_ids.er_thirdPersonDistanceTemp = (void *)lc->GetFieldID(
            erCls, "thirdPersonDistanceTemp", "F", "field_78491_C", "C");
      } else {
        m_ids.mc_entityRenderer = nullptr;
        m_ids.er_getFOVModifier = nullptr;
        m_ids.er_thirdPersonDistanceTemp = nullptr;
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

    jclass ariCls = lc->GetClass("net.minecraft.client.renderer.ActiveRenderInfo");
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

    jclass texMapCls =
        lc->GetClass("net.minecraft.client.renderer.texture.TextureMap");
    if (!texMapCls)
      return;
    m_ids.texMap_locationBlocksTexture = (void *)lc->GetStaticFieldID(
        texMapCls, "locationBlocksTexture",
        "Lnet/minecraft/util/ResourceLocation;", "field_110575_b", "g", "Ljy;");

    jclass texManCls =
        lc->GetClass("net.minecraft.client.renderer.texture.TextureManager");
    if (!texManCls)
      return;
    m_ids.texMan_bindTexture = (void *)lc->GetMethodID(
        texManCls, "bindTexture", "(Lnet/minecraft/util/ResourceLocation;)V",
        "func_110577_a", "a", "(Ljy;)V");

    m_ids.initialized = true;
  } catch (...) {
    Logger::error("Failed to initialize DefenseRenderer JNI IDs");
  }
}

DefenseRenderer::~DefenseRenderer() {}

DefenseRenderer *DefenseRenderer::getInstance() {
  if (!s_instance) {
    s_instance = new DefenseRenderer();
  }
  return s_instance;
}

void DefenseRenderer::destroy() {
  if (s_instance) {
    delete s_instance;
    s_instance = nullptr;
  }
}

void DefenseRenderer::setupBillboard(double camX, double camY, double camZ,
                                     double bedX, double bedY, double bedZ) {
  glTranslated(bedX - camX, bedY - camY + 2.0, bedZ - camZ);

  double dx = camX - bedX;
  double dz = camZ - bedZ;
  float yaw = -(float)(atan2(dz, dx) * 180.0 / 3.14159265) - 90.0f;

  glRotatef(-yaw, 0.0f, 1.0f, 0.0f);
}

void DefenseRenderer::renderBlockIcon(float x, float y,
                                      const DefenseLayer &layer) {
  float size = 0.5f;

  TextureLoader *texLoader = TextureLoader::getInstance();
  std::string texName = getTextureNameForBlock(layer.blockName, layer.metadata);
  unsigned int localTex = texLoader->getTexture(texName);

  if (localTex != 0) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, localTex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x - size, y - size, 0.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(x - size, y + size, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(x + size, y + size, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(x + size, y - size, 0.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
  } else {
    glDisable(GL_TEXTURE_2D);

    uint32_t c = layer.color;
    float r = ((c >> 16) & 0xFF) / 255.0f;
    float g = ((c >> 8) & 0xFF) / 255.0f;
    float b = (c & 0xFF) / 255.0f;

    glColor4f(r, g, b, 1.0f);

    glBegin(GL_QUADS);
    glVertex3f(x - size, y - size, 0.0f);
    glVertex3f(x - size, y + size, 0.0f);
    glVertex3f(x + size, y + size, 0.0f);
    glVertex3f(x + size, y - size, 0.0f);
    glEnd();
  }
}

std::string
DefenseRenderer::getTextureNameForBlock(const std::string &blockName,
                                        int metadata) {
  const char *colors[] = {"white",  "orange", "magenta", "lightblue",
                          "yellow", "lime",   "pink",    "gray",
                          "silver", "cyan",   "purple",  "blue",
                          "brown",  "green",  "red",     "black"};

  if (blockName.find("wool") != std::string::npos) {
    if (metadata >= 0 && metadata < 16)
      return std::string("wool_") + colors[metadata];
    return "wool_white";
  }

  if (blockName.find("stained_hardened_clay") != std::string::npos ||
      blockName.find("terracotta") != std::string::npos) {
    if (metadata >= 0 && metadata < 16)
      return std::string("terracotta_") + colors[metadata];
    return "terracotta_white";
  }

  if (blockName.find("stained_glass") != std::string::npos) {
    if (metadata >= 0 && metadata < 16)
      return std::string("glass_") + colors[metadata];
    return "glass_white";
  }

  if (blockName.find("glass") != std::string::npos) {
    return "glass";
  }

  if (blockName.find("planks") != std::string::npos) {
    const char *plankTypes[] = {"planks_oak",    "planks_spruce",
                                "planks_birch",  "planks_jungle",
                                "planks_acacia", "planks_darkoak"};
    if (metadata >= 0 && metadata < 6)
      return plankTypes[metadata];
    return "planks_oak";
  }

  if (blockName.find("log2") != std::string::npos) {
    if (metadata % 4 == 0)
      return "log_acacia";
    return "log_darkoak";
  }
  if (blockName.find("log") != std::string::npos) {
    const char *logTypes[] = {"log_oak", "log_spruce", "log_birch",
                              "log_jungle"};
    int logType = (metadata % 4);
    if (logType >= 0 && logType < 4)
      return logTypes[logType];
    return "log_oak";
  }

  if (blockName.find("wood") != std::string::npos) {
    return "planks_oak";
  }

  if (blockName.find("obsidian") != std::string::npos)
    return "obsidian";
  if (blockName.find("end_stone") != std::string::npos)
    return "end_stone";
  if (blockName.find("hardened_clay") != std::string::npos)
    return "terracotta";

  return "";
}

static void drawRoundedRect(float x, float y, float w, float h, float radius,
                            int color) {
  float alpha = (float)((color >> 24) & 0xFF) / 255.0f;
  float red = (float)((color >> 16) & 0xFF) / 255.0f;
  float green = (float)((color >> 8) & 0xFF) / 255.0f;
  float blue = (float)(color & 0xFF) / 255.0f;

  glDisable(GL_TEXTURE_2D);
  glColor4f(red, green, blue, alpha);

  if (radius > w / 2.0f)
    radius = w / 2.0f;
  if (radius > h / 2.0f)
    radius = h / 2.0f;

  glBegin(GL_POLYGON);
  for (int i = 0; i <= 360; i += 10) {
    float rad = (float)i * 3.14159f / 180.0f;
    float c = cos(rad);
    float s = sin(rad);

    float cx = (c > 0) ? (x + w - radius) : (x + radius);
    float cy = (s > 0) ? (y + h - radius) : (y + radius);

    glVertex2f(cx + c * radius, cy + s * radius);
  }
  glEnd();
}

void DefenseRenderer::render(void *hdcPtr, double partialTicksManual) {
  HDC hdc = (HDC)hdcPtr;
  BedDefenseManager *manager = BedDefenseManager::getInstance();
  if (!manager || !manager->isEnabled())
    return;

  if (!lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  initIds(env);
  if (!m_font.isInitialized() && hdc) {
    m_font.init(hdc);
  }

  std::vector<DetectedBed> bedsToDraw;
  {
    std::lock_guard<std::mutex> lock(manager->getMutex());
    const auto &bedsMap = manager->getBeds();
    if (bedsMap.empty())
      return;
    for (const auto &pair : bedsMap)
      bedsToDraw.push_back(pair.second);
  }

  double camX = 0, camY = 0, camZ = 0;
  double feetY = 0;
  float rotationYaw = 0, rotationPitch = 0;
  float fovSetting = 70.0f;
  float pt = (float)partialTicksManual;
  float thirdPersonDist = 0.0f;
  int thirdPersonMode = 0;

  if (!m_ids.initialized)
    return;

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = env->GetStaticObjectField(mcCls, (jfieldID)m_ids.mc_theMc);
    if (mcObj) {
      jobject timerObj = env->GetObjectField(mcObj, (jfieldID)m_ids.mc_timer);
      if (timerObj) {
        float realPt =
            env->GetFloatField(timerObj, (jfieldID)m_ids.timer_partialTicks);
        if (realPt > 0 && realPt <= 1.0f)
          pt = realPt;
        env->DeleteLocalRef(timerObj);
      }

      jobject renderManager =
          (jfieldID)m_ids.mc_renderManager
              ? env->GetObjectField(mcObj, (jfieldID)m_ids.mc_renderManager)
              : nullptr;
      if (renderManager) {
        camX =
            env->GetDoubleField(renderManager, (jfieldID)m_ids.rm_viewerPosX);
        camY =
            env->GetDoubleField(renderManager, (jfieldID)m_ids.rm_viewerPosY);
        camZ =
            env->GetDoubleField(renderManager, (jfieldID)m_ids.rm_viewerPosZ);
        feetY = camY;
        camY += 1.62;
        rotationPitch =
            env->GetFloatField(renderManager, (jfieldID)m_ids.rm_playerViewX);
        rotationYaw =
            env->GetFloatField(renderManager, (jfieldID)m_ids.rm_playerViewY);
        env->DeleteLocalRef(renderManager);
      } else {
        jobject entity = (jfieldID)m_ids.mc_renderViewEntity
                             ? env->GetObjectField(
                                   mcObj, (jfieldID)m_ids.mc_renderViewEntity)
                             : nullptr;
        if (entity) {
          double curX = env->GetDoubleField(entity, (jfieldID)m_ids.ent_posX);
          double curY = env->GetDoubleField(entity, (jfieldID)m_ids.ent_posY);
          double curZ = env->GetDoubleField(entity, (jfieldID)m_ids.ent_posZ);
          double preX = env->GetDoubleField(entity, (jfieldID)m_ids.ent_prevX);
          double preY = env->GetDoubleField(entity, (jfieldID)m_ids.ent_prevY);
          double preZ = env->GetDoubleField(entity, (jfieldID)m_ids.ent_prevZ);
          camX = preX + (curX - preX) * (double)pt;
          feetY = preY + (curY - preY) * (double)pt;
          camY = feetY + 1.62;
          camZ = preZ + (curZ - preZ) * (double)pt;
          rotationYaw = env->GetFloatField(entity, (jfieldID)m_ids.ent_yaw);
          rotationPitch = env->GetFloatField(entity, (jfieldID)m_ids.ent_pitch);
          env->DeleteLocalRef(entity);
        }
      }

      jobject settings =
          env->GetObjectField(mcObj, (jfieldID)m_ids.mc_gameSettings);
      if (settings) {
        fovSetting =
            env->GetFloatField(settings, (jfieldID)m_ids.gs_fovSetting);
        if (m_ids.gs_thirdPersonView)
          thirdPersonMode = env->GetIntField(
              settings, (jfieldID)m_ids.gs_thirdPersonView);
        env->DeleteLocalRef(settings);
      }

      if (m_ids.mc_entityRenderer) {
        jobject er = env->GetObjectField(
            mcObj, (jfieldID)m_ids.mc_entityRenderer);
        if (er) {
          if (m_ids.er_getFOVModifier) {
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
            thirdPersonDist = env->GetFloatField(
                er, (jfieldID)m_ids.er_thirdPersonDistanceTemp);
          }
          env->DeleteLocalRef(er);
        }
      }

      env->DeleteLocalRef(mcObj);
    }
  } catch (...) {
    return;
  }

  bool useActiveRenderInfo = false;
  float modelview[16];
  float projection[16];
  int viewport[4];

  if (m_ids.ari_MODELVIEW && m_ids.ari_PROJECTION && m_ids.ari_VIEWPORT) {
    jclass ariCls = lc->GetClass("net.minecraft.client.renderer.ActiveRenderInfo");
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

  if (viewport[2] <= 0 || viewport[3] <= 0) return;
  const float vpW = (float)viewport[2];
  const float vpH = (float)viewport[3];
  const float aspect = vpW / vpH;

  const double yawRad =
      (double)(rotationYaw + 180.0f) * 3.14159265358979 / 180.0;
  const double pitchRad =
      (double)(rotationPitch) * 3.14159265358979 / 180.0;
  const double sinYaw = sin(yawRad), cosYaw = cos(yawRad);
  const double sinPitch = sin(pitchRad), cosPitch = cos(pitchRad);
  const double fovHalfRad =
      (double)fovSetting * 0.5 * 3.14159265358979 / 180.0;
  const double f = 1.0 / tan(fovHalfRad);

  struct ProjectedBed {
    const DetectedBed *bed;
    float pixelX = 0;
    float pixelY = 0;
    float scale  = 1.0f;
    double dist  = 0.0;
  };
  std::vector<ProjectedBed> projected;
  projected.reserve(bedsToDraw.size());

  for (const auto &bed : bedsToDraw) {
    double wx = bed.x + 0.5;
    double wy = bed.y + 1.6;
    double wz = bed.z + 0.5;

    double rx = wx - camX;
    double ry = wy - (useActiveRenderInfo ? feetY : camY);
    double rz = wz - camZ;

    double ndcX = 0.0, ndcY = 0.0;
    double dist = 0.0;

    if (useActiveRenderInfo) {
      Vec4 pos{ rx, ry, rz, 1.0 };
      Vec4 eyeSpace = matMul(modelview, pos);

      if (eyeSpace.z >= -0.05) continue;

      Vec4 clipSpace = matMul(projection, eyeSpace);
      if (clipSpace.w == 0.0) continue;

      ndcX = clipSpace.x / clipSpace.w;
      ndcY = clipSpace.y / clipSpace.w;
      dist = sqrt(eyeSpace.x * eyeSpace.x + eyeSpace.y * eyeSpace.y + eyeSpace.z * eyeSpace.z);
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

      if (vz2 >= -0.05) continue; // behind camera (i fucking hate this)

      ndcX = (f / aspect) * vx / -vz2;
      ndcY = f * vy / -vz2;
      dist = sqrt(rx * rx + ry * ry + rz * rz);
    }

    if (ndcX < -1.4 || ndcX > 1.4 || ndcY < -1.4 || ndcY > 1.4) continue;
    if (dist < 0.5) continue;

    ProjectedBed p;
    p.bed = &bed;
    p.pixelX = (float)(viewport[0] + viewport[2] * (ndcX * 0.5 + 0.5));
    p.pixelY = (float)(viewport[1] + viewport[3] * (1.0 - (ndcY * 0.5 + 0.5)));
    float activeF = useActiveRenderInfo ? projection[5] : (float)f;
    float s = (float)(activeF / dist) * 15.0f;
    if (s < 0.5f) s = 0.5f;
    if (s > 30.0f) s = 30.0f;
    p.scale = s;
    p.dist = dist;
    projected.push_back(p);
  }

  std::sort(projected.begin(), projected.end(),
            [](const ProjectedBed &a, const ProjectedBed &b) {
              return a.dist > b.dist;
            });

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT |
               GL_DEPTH_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(1, 1, 1, 1);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, vpW, vpH, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  try {
    for (const auto &p : projected) {
      const auto &bed = *p.bed;

      glPushMatrix();
      glTranslatef(p.pixelX, p.pixelY, 0.0f);
      glScalef(p.scale, p.scale, 1.0f);

      std::string distStr = std::to_string((int)p.dist) + "m";

      float iconSize = 16.0f;
      float padding = 3.0f;
      float textScale = 0.75f;
      float textH = 8.0f * textScale;
      float rawTextW =
          m_font.isInitialized() ? m_font.getStringWidth(distStr) : 20.0f;
      float textW = rawTextW * textScale;
      float gap = 6.0f;

      bool isEmpty = bed.layers.empty();
      float contentW =
          isEmpty ? (iconSize)
                  : ((float)bed.layers.size() * (iconSize + 1.0f) - 1.0f);

      float boxW = (textW > contentW ? textW : contentW) + padding * 4.0f;
      float boxH = padding + textH + gap + iconSize + padding;

      drawRoundedRect(-boxW / 2, -boxH / 2, boxW, boxH, 4.0f, 0xAA000000);

      if (m_font.isInitialized()) {
        glPushMatrix();
        float textY = (-boxH / 2 + padding) / textScale;
        glScalef(textScale, textScale, 1.0f);
        m_font.drawString(-rawTextW / 2.0f, textY, distStr, 0xFFFFFFFF);
        glPopMatrix();
      }

      float contentY = -boxH / 2 + padding + textH + gap + iconSize / 2.0f;

      if (isEmpty) {
        drawCross(0, contentY, (iconSize / 2.0f) - 2.0f, 0xFFFFFFFF);
      } else {
        float startX = -contentW / 2.0f + (iconSize / 2.0f);
        for (size_t i = 0; i < bed.layers.size(); i++) {
          glPushMatrix();
          glTranslatef(startX + (float)i * (iconSize + 1.0f), contentY, 0);
          glScalef(iconSize, iconSize, 1.0f);
          renderBlockIcon(0, 0, bed.layers[i]);
          glPopMatrix();
        }
      }

      glPopMatrix();
    }
  } catch (...) {
  }

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glDepthMask(GL_TRUE);
  glPopAttrib();
}
} // namespace BedDefense
