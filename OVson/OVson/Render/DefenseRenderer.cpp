#include "DefenseRenderer.h"
#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Utils/Logger.h"
#include "TextureLoader.h"
#include <cmath>


namespace BedDefense {
DefenseRenderer *DefenseRenderer::s_instance = nullptr;

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
  float rotationYaw = 0, rotationPitch = 0;
  float fovSetting = 70.0f;
  float pt = (float)partialTicksManual;

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
          camY = preY + (curY - preY) * (double)pt;
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
        env->DeleteLocalRef(settings);
      }

      env->DeleteLocalRef(mcObj);
    }
  } catch (...) {
    return;
  }

  glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT |
               GL_DEPTH_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glColor4f(1, 1, 1, 1);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  float aspect = (float)viewport[2] / (float)viewport[3];

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  float fov = fovSetting;
  float nearP = 0.05f;
  float farP = 2000.0f;
  float f = 1.0f / tanf(fov * (3.14159265f / 360.0f));
  float proj[16] = {f / aspect,
                    0,
                    0,
                    0,
                    0,
                    f,
                    0,
                    0,
                    0,
                    0,
                    (farP + nearP) / (nearP - farP),
                    -1,
                    0,
                    0,
                    (2 * farP * nearP) / (nearP - farP),
                    0};
  glLoadMatrixf(proj);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glRotatef(rotationPitch, 1.0f, 0.0f, 0.0f);
  glRotatef(rotationYaw + 180.0f, 0.0f, 1.0f, 0.0f);

  try {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);

    for (const auto &bed : bedsToDraw) {
      glPushMatrix();
      glTranslated(bed.x + 0.5 - camX, bed.y + 0.8 - camY, bed.z + 0.5 - camZ);
      double deltaX = camX - (bed.x + 0.5);
      double deltaY = camY - (bed.y + 0.8);
      double deltaZ = camZ - (bed.z + 0.5);
      double distXZ = sqrt(deltaX * deltaX + deltaZ * deltaZ);

      float yaw = (float)(atan2(deltaZ, deltaX) * 180.0 / 3.14159265) - 90.0f;
      float pitch = -(float)(atan2(deltaY, distXZ) * 180.0 / 3.14159265);

      glRotatef(-yaw, 0.0f, 1.0f, 0.0f);
      glRotatef(pitch, 1.0f, 0.0f, 0.0f);

      double dist = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
      float baseScale = 0.02f;
      float distScale = 1.0f + (float)(dist / 10.0f);

      if (dist < 8.0f) {
        distScale *= (float)(dist / 8.0f);
      }

      if (distScale < 0.1f)
        distScale = 0.1f;
      if (distScale > 6.0f)
        distScale = 6.0f;

      float finalScale = baseScale * distScale;
      glScalef(finalScale, -finalScale, finalScale);

      std::string distStr = std::to_string((int)dist) + "m";

      float iconSize = 16.0f;
      float padding = 3.0f;
      float textScale = 0.75f;
      float textH = 8.0f * textScale;
      float rawTextW =
          m_font.isInitialized() ? m_font.getStringWidth(distStr) : 20.0f;
      float textW = rawTextW * textScale;
      float gap = 12.0f;

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

  glPopAttrib();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}
} // namespace BedDefense
