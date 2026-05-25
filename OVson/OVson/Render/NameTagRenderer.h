#pragma once
#include <Windows.h>

namespace OVson {

class NameTagRenderer {
public:
  static NameTagRenderer *getInstance();
  static void destroy();

  void render(void *hdcPtr, double partialTicksManual);

private:
  NameTagRenderer();
  ~NameTagRenderer();
  NameTagRenderer(const NameTagRenderer &) = delete;
  NameTagRenderer &operator=(const NameTagRenderer &) = delete;

  void initIds();

  struct JNICache {
    bool initialized = false;
    void *mc_theMc = nullptr;
    void *mc_theWorld = nullptr;
    void *mc_renderViewEntity = nullptr;
    void *mc_renderManager = nullptr;
    void *mc_gameSettings = nullptr;
    void *mc_timer = nullptr;
    void *mc_fontRendererObj = nullptr;
    void *rm_viewerPosX = nullptr, *rm_viewerPosY = nullptr,
         *rm_viewerPosZ = nullptr;
    void *rm_playerViewX = nullptr, *rm_playerViewY = nullptr;
    void *ent_posX = nullptr, *ent_posY = nullptr, *ent_posZ = nullptr;
    void *ent_prevX = nullptr, *ent_prevY = nullptr, *ent_prevZ = nullptr;
    void *player_getName = nullptr;       // ()Lnet/minecraft/util/IChatComponent;
    void *player_getCommandSenderName = nullptr; // ()Ljava/lang/String; fallback
    void *player_getGameProfile = nullptr;     // ()Lcom/mojang/authlib/GameProfile;
    void *gameProfile_getName    = nullptr;    // ()Ljava/lang/String;
    // WorldClient.playerEntities (List<EntityPlayer>)
    void *world_playerEntities = nullptr;
    // List<>.size() / get(int)
    void *list_size = nullptr;
    void *list_get = nullptr;
    void *gs_fovSetting = nullptr;
    // Timer.renderPartialTicks
    void *timer_partialTicks = nullptr;
    void *mc_entityRenderer = nullptr;
    void *er_getFOVModifier = nullptr;
    void *er_thirdPersonDistanceTemp = nullptr;
    // gameSettings.thirdPersonView: 0 first-person, 1 back, 2 front.
    void *gs_thirdPersonView = nullptr;
    // ActiveRenderInfo matrices for robust camera projection
    void *ari_MODELVIEW = nullptr;
    void *ari_PROJECTION = nullptr;
    void *ari_VIEWPORT = nullptr;
    // FontRenderer.drawStringWithShadow + getStringWidth
    void *font_drawString = nullptr;
    void *font_getStringWidth = nullptr;
  } m_ids;
};

} // namespace OVson
