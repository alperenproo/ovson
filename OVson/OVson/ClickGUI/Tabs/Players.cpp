#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Render/McFont.h"
#include "../../Render/TextureLoader.h"
#include "../../Java.h"
#include "../../Config/Config.h"
#include "../../Config/StatColors.h"
#include "../../Utils/BedwarsPrestiges.h"
#include "../../Utils/stb_image.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <gl/GL.h>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace Render {
namespace Tabs {

namespace {

struct ItemTex {
  GLuint texId = 0;
  int w = 0, h = 0;
};
static std::unordered_map<std::string, ItemTex> s_itemTextures;
static std::string s_itemTextureDir;

static GLuint getItemTexture(const std::string &rawKey) {
  if (rawKey.empty() || rawKey == "null") return 0;

  std::string k = rawKey;
  for (char &c : k) c = (char)tolower((unsigned char)c);
  while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) k.erase(0, 1);
  while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();

  if (k.rfind("bedwars_", 0) == 0) k = k.substr(8);
  if (k.rfind("item_", 0) == 0) k = k.substr(5);

  if (k == "tools_1" || k == "tools 1" || k == "tools1") k = "diamond_pickaxe";
  else if (k == "tools_2" || k == "tools 2" || k == "tools2") k = "diamond_axe";
  else if (k == "tools_3" || k == "tools 3" || k == "tools3") k = "shears";
  else if (k == "tools" || k == "tool") k = "diamond_pickaxe";
  else if (k == "pickaxe") k = "diamond_pickaxe";
  else if (k == "axe") k = "diamond_axe";
  else if (k == "melee" || k == "sword") k = "gold_sword";
  else if (k == "blocks" || k == "block") k = "wool";
  else if (k == "ranged") k = "bow";
  else if (k == "potions" || k == "potion") k = "speed_potion";
  else if (k == "utility") k = "golden_apple";
  else if (k == "compass") k = "compass";
  else if (k == "oak_planks" || k == "wood_planks" || k == "planks" || k == "wood" || k == "oak_wood_planks") k = "oak_wood_planks";
  else if (k == "hardened_clay" || k == "clay" || k == "terracotta") k = "hardened_clay";
  else if (k == "blast_proof_glass" || k == "glass") k = "blast_proof_glass";
  else if (k == "ender_chest" || k == "compact_chest") k = "chest";
  else if (k == "bucket_water") k = "water_bucket";
  else if (k == "bucket_milk" || k == "milk" || k == "milk_bucket") k = "magic_milk";
  else if (k == "egg" || k == "bridge_egg") k = "bridge_egg";
  else if (k.find("tower") != std::string::npos || k.find("popup") != std::string::npos) k = "chest";
  else if (k.find("stick") != std::string::npos) k = "debug_stick";
  else if (k.find("pane") != std::string::npos) k = "glass_pane";
  else if (k == "silverfish" || k == "bedbug" || k == "spawn_egg") k = "bedbug";
  else if (k == "iron_golem" || k == "dream_defender") k = "dream_defender";
  else if (k == "chainmail_armor" || k == "chainmail_boots" || k == "chainmail_chestplate") k = "chainmail_armor";
  else if (k == "iron_armor" || k == "iron_boots" || k == "iron_chestplate") k = "iron_armor";
  else if (k == "diamond_armor" || k == "diamond_boots" || k == "diamond_chestplate") k = "diamond_armor";
  else if (k == "apple_golden") k = "golden_apple";
  else if (k == "bow_standby") k = "bow";
  else if (k.find("speed") != std::string::npos && (k.find("potion") != std::string::npos || k.find("pot") != std::string::npos)) k = "speed_potion";
  else if (k.find("jump") != std::string::npos && (k.find("potion") != std::string::npos || k.find("pot") != std::string::npos)) k = "jump_potion";
  else if (k.find("invis") != std::string::npos && (k.find("potion") != std::string::npos || k.find("pot") != std::string::npos)) k = "invis_potion";

  auto it = s_itemTextures.find(k);
  if (it != s_itemTextures.end()) {
    return it->second.texId;
  }

  std::vector<std::string> pathsToTry;
  HMODULE hMod = Config::getModuleHandle();
  if (hMod) {
    char dllPath[MAX_PATH] = {0};
    if (GetModuleFileNameA(hMod, dllPath, MAX_PATH) > 0) {
      char *lastSlash = strrchr(dllPath, '\\');
      if (lastSlash) {
        *lastSlash = '\0';
        std::string dllDir = dllPath;
        pathsToTry.push_back(dllDir + "\\assets\\items\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\items\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\..\\assets\\items\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\..\\items\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\..\\..\\assets\\items\\" + k + ".png");
        pathsToTry.push_back(dllDir + "\\..\\..\\items\\" + k + ".png");
      }
    }
  }

  char buf[MAX_PATH];
  if (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) > 0) {
    pathsToTry.push_back(std::string(buf) + "\\OVson\\assets\\items\\" + k + ".png");
    pathsToTry.push_back(std::string(buf) + "\\OVson\\items\\" + k + ".png");
    pathsToTry.push_back(std::string(buf) + "\\.minecraft\\OVson\\assets\\items\\" + k + ".png");
    pathsToTry.push_back(std::string(buf) + "\\.minecraft\\OVson\\items\\" + k + ".png");
  }
  if (GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH) > 0) {
    pathsToTry.push_back(std::string(buf) + "\\OVson\\assets\\items\\" + k + ".png");
    pathsToTry.push_back(std::string(buf) + "\\OVson\\items\\" + k + ".png");
  }
  if (GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH) > 0) {
    pathsToTry.push_back(std::string(buf) + "\\.minecraft\\OVson\\assets\\items\\" + k + ".png");
    pathsToTry.push_back(std::string(buf) + "\\.minecraft\\OVson\\items\\" + k + ".png");
  }

  pathsToTry.push_back("assets\\items\\" + k + ".png");
  pathsToTry.push_back("OVson\\assets\\items\\" + k + ".png");
  pathsToTry.push_back("OVson\\items\\" + k + ".png");
  pathsToTry.push_back("items\\" + k + ".png");
  pathsToTry.push_back(".minecraft\\OVson\\assets\\items\\" + k + ".png");
  pathsToTry.push_back(".minecraft\\OVson\\items\\" + k + ".png");
  pathsToTry.push_back("C:\\Users\\HPC1\\Downloads\\Minecraft_Items_1.13.2_Selected\\" + k + ".png");
  pathsToTry.push_back("C:\\Users\\HPC1\\Downloads\\Minecraft_Items_1.13.2_Selected (1)\\" + k + ".png");
  if (k == "glass_pane") {
    pathsToTry.push_back("C:\\Users\\HPC1\\Downloads\\Minecraft_Items_1.13.2_Selected (1)\\gray_stained_glass_pane.png");
    pathsToTry.push_back("C:\\Users\\HPC1\\Downloads\\Minecraft_Items_1.13.2_Selected\\gray_stained_glass_pane.png");
  }
  if (k == "debug_stick" || k == "stick" || k == "knockback_stick") {
    pathsToTry.push_back("C:\\Users\\HPC1\\Downloads\\Minecraft_Items_1.13.2_Selected\\debug_stick.png");
  }

  int w = 0, h = 0, ch = 0;
  unsigned char *px = nullptr;
  for (const auto &p : pathsToTry) {
    px = stbi_load(p.c_str(), &w, &h, &ch, 4);
    if (px) break;
  }

  if (!px && hMod) {
    std::string resName = "ITEM_" + k;
    HRSRC hRes = FindResourceA(hMod, resName.c_str(), RT_RCDATA);
    if (!hRes) hRes = FindResourceA(hMod, k.c_str(), RT_RCDATA);
    if (hRes) {
      HGLOBAL hData = LoadResource(hMod, hRes);
      if (hData) {
        void *pData = LockResource(hData);
        DWORD sz = SizeofResource(hMod, hRes);
        if (pData && sz > 0) {
          px = stbi_load_from_memory((const stbi_uc *)pData, (int)sz, &w, &h, &ch, 4);
        }
      }
    }
  }

  if (!px) {
    unsigned int blockTex = 0;
    if (k == "wool" || k == "white_wool") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("wool_white");
    else if (k == "hardened_clay" || k == "clay" || k == "terracotta") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("terracotta");
    else if (k == "blast_proof_glass" || k == "glass") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("glass");
    else if (k == "end_stone") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("end_stone");
    else if (k == "obsidian") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("obsidian");
    else if (k == "oak_wood_planks" || k == "wood" || k == "planks" || k == "oak_planks") blockTex = BedDefense::TextureLoader::getInstance()->getTexture("planks_oak");

    if (blockTex != 0) {
      s_itemTextures[k] = {blockTex, 16, 16};
      return blockTex;
    }
  }

  if (!px) {
    std::string fallbackKey;
    if (k.find("sword") != std::string::npos) fallbackKey = "iron_sword";
    else if (k.find("pickaxe") != std::string::npos) fallbackKey = "diamond_pickaxe";
    else if (k.find("axe") != std::string::npos) fallbackKey = "diamond_axe";
    else if (k.find("boots") != std::string::npos || k.find("armor") != std::string::npos || k.find("chestplate") != std::string::npos) fallbackKey = "iron_armor";
    else if (k.find("bow") != std::string::npos) fallbackKey = "bow";
    else if (k.find("potion") != std::string::npos) fallbackKey = "speed_potion";
    else if (k == "white_wool") fallbackKey = "wool";
    else if (k == "wool") fallbackKey = "white_wool";
    else if (k.find("wool") != std::string::npos) fallbackKey = "wool";
    else if (k == "clay" || k == "terracotta") fallbackKey = "hardened_clay";
    else if (k == "hardened_clay") fallbackKey = "terracotta";
    else if (k.find("clay") != std::string::npos) fallbackKey = "hardened_clay";
    else if (k == "glass") fallbackKey = "blast_proof_glass";
    else if (k == "blast_proof_glass") fallbackKey = "glass";
    else if (k.find("glass") != std::string::npos) fallbackKey = "blast_proof_glass";
    else if (k == "wood" || k == "planks" || k == "oak_planks") fallbackKey = "oak_wood_planks";
    else if (k == "oak_wood_planks") fallbackKey = "oak_planks";
    else if (k == "knockback_stick") fallbackKey = "debug_stick";
    else if (k == "stick") fallbackKey = "debug_stick";
    else if (k.find("chest") != std::string::npos) fallbackKey = "chest";

    if (!fallbackKey.empty() && fallbackKey != k) {
      GLuint fTid = getItemTexture(fallbackKey);
      s_itemTextures[k] = {fTid, 16, 16};
      return fTid;
    }

    s_itemTextures[k] = {0, 0, 0};
    return 0;
  }

  GLuint tid = 0;
  glGenTextures(1, &tid);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glBindTexture(GL_TEXTURE_2D, 0);

  stbi_image_free(px);
  s_itemTextures[k] = {tid, w, h};
  return tid;
}

inline void drawBoxQuad(float x0, float y0, float z0,
                        float x1, float y1, float z1,
                        float x2, float y2, float z2,
                        float x3, float y3, float z3,
                        float u0, float v0, float u1, float v1,
                        float texW, float texH, float shade, float alpha) {
  glColor4f(shade, shade, shade, alpha);
  float u_a = u0 / texW, v_a = v0 / texH;
  float u_b = u1 / texW, v_b = v1 / texH;
  glTexCoord2f(u_a, v_a); glVertex3f(x0, y0, z0);
  glTexCoord2f(u_b, v_a); glVertex3f(x1, y1, z1);
  glTexCoord2f(u_b, v_b); glVertex3f(x2, y2, z2);
  glTexCoord2f(u_a, v_b); glVertex3f(x3, y3, z3);
}

void drawModelBox(float u0, float v0, float x, float y, float z,
                  float dx, float dy, float dz, float delta,
                  float texW, float texH, float alpha, bool mirror = false) {
  float x0 = x - delta, x1 = x + dx + delta;
  float y0 = y - delta, y1 = y + dy + delta;
  float z0 = z - delta, z1 = z + dz + delta;

  const float topShade   = 1.00f;
  const float botShade   = 0.50f;
  const float frontShade = 1.00f;
  const float backShade  = 0.70f;
  const float leftShade  = 0.75f;
  const float rightShade = 0.75f;

  float uR0 = u0,                uR1 = u0 + dz;
  float uF0 = u0 + dz,           uF1 = u0 + dz + dx;
  float uL0 = u0 + dz + dx,      uL1 = u0 + dz + dx + dz;
  float uB0 = u0 + dz + dx + dz, uB1 = u0 + dz + dx + dz + dx;

  float uT0 = u0 + dz,           uT1 = u0 + dz + dx;
  float uBot0 = u0 + dz + dx,    uBot1 = u0 + dz + dx + dx;

  float vT0 = v0,           vT1 = v0 + dz;
  float vBot0 = v0,         vBot1 = v0 + dz;
  float vF0 = v0 + dz,      vF1 = v0 + dz + dy;
  float vB0 = v0 + dz,      vB1 = v0 + dz + dy;
  float vR0 = v0 + dz,      vR1 = v0 + dz + dy;
  float vL0 = v0 + dz,      vL1 = v0 + dz + dy;

  if (mirror) {
    std::swap(uR0, uL0);
    std::swap(uR1, uL1);
    std::swap(uF0, uF1);
    std::swap(uB0, uB1);
    std::swap(uT0, uT1);
    std::swap(uBot0, uBot1);
  }

  glBegin(GL_QUADS);

  drawBoxQuad(x0, y1, z1,  x1, y1, z1,  x1, y0, z1,  x0, y0, z1,
              uF0, vF0, uF1, vF1, texW, texH, frontShade, alpha);

  drawBoxQuad(x1, y1, z0,  x0, y1, z0,  x0, y0, z0,  x1, y0, z0,
              uB0, vB0, uB1, vB1, texW, texH, backShade, alpha);

  drawBoxQuad(x0, y1, z0,  x0, y1, z1,  x0, y0, z1,  x0, y0, z0,
              uR0, vR0, uR1, vR1, texW, texH, rightShade, alpha);

  drawBoxQuad(x1, y1, z1,  x1, y1, z0,  x1, y0, z0,  x1, y0, z1,
              uL0, vL0, uL1, vL1, texW, texH, leftShade, alpha);

  drawBoxQuad(x0, y1, z0,  x1, y1, z0,  x1, y1, z1,  x0, y1, z1,
              uT0, vT0, uT1, vT1, texW, texH, topShade, alpha);

  drawBoxQuad(x0, y0, z0,  x1, y0, z0,  x1, y0, z1,  x0, y0, z1,
              uBot0, vBot0, uBot1, vBot1, texW, texH, botShade, alpha);

  glEnd();
}

void renderPlayerModel3D(float texW, float texH, float alpha, bool slim) {
  bool is64x64 = (texH >= 64.0f);
  float armDx = slim ? 3.0f : 4.0f;

  glTranslatef(0.0f, 8.0f, 0.0f);

  glPushMatrix();
  drawModelBox(0.0f, 0.0f, -4.0f, 0.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0.0f, texW, texH, alpha);
  drawModelBox(32.0f, 0.0f, -4.0f, 0.0f, -4.0f, 8.0f, 8.0f, 8.0f, 0.5f, texW, texH, alpha);
  glPopMatrix();

  glPushMatrix();
  drawModelBox(16.0f, 16.0f, -4.0f, -12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 0.0f, texW, texH, alpha);
  if (is64x64) {
    drawModelBox(16.0f, 32.0f, -4.0f, -12.0f, -2.0f, 8.0f, 12.0f, 4.0f, 0.25f, texW, texH, alpha);
  }
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-5.0f, -2.0f, 0.0f);
  {
    float rArmX = slim ? -2.0f : -3.0f;
    drawModelBox(40.0f, 16.0f, rArmX, -10.0f, -2.0f, armDx, 12.0f, 4.0f, 0.0f, texW, texH, alpha);
    if (is64x64) {
      drawModelBox(40.0f, 32.0f, rArmX, -10.0f, -2.0f, armDx, 12.0f, 4.0f, 0.25f, texW, texH, alpha);
    }
  }
  glPopMatrix();

  glPushMatrix();
  glTranslatef(5.0f, -2.0f, 0.0f);
  if (is64x64) {
    drawModelBox(32.0f, 48.0f, -1.0f, -10.0f, -2.0f, armDx, 12.0f, 4.0f, 0.0f, texW, texH, alpha);
    drawModelBox(48.0f, 48.0f, -1.0f, -10.0f, -2.0f, armDx, 12.0f, 4.0f, 0.25f, texW, texH, alpha);
  } else {
    drawModelBox(40.0f, 16.0f, -1.0f, -10.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.0f, texW, texH, alpha, true);
  }
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-2.0f, -12.0f, 0.0f);
  drawModelBox(0.0f, 16.0f, -2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.0f, texW, texH, alpha);
  if (is64x64) {
    drawModelBox(0.0f, 32.0f, -2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.25f, texW, texH, alpha);
  }
  glPopMatrix();

  glPushMatrix();
  glTranslatef(2.0f, -12.0f, 0.0f);
  if (is64x64) {
    drawModelBox(16.0f, 48.0f, -2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.0f, texW, texH, alpha);
    drawModelBox(0.0f, 48.0f, -2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.25f, texW, texH, alpha);
  } else {
    // Legacy 64x32 mirror
    drawModelBox(0.0f, 16.0f, -2.0f, -12.0f, -2.0f, 4.0f, 12.0f, 4.0f, 0.0f, texW, texH, alpha, true);
  }
  glPopMatrix();
}

} // anonymous namespace

void renderPlayers(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Player Search",
                       applyAlpha(0xFFFFFFFF, alpha), 0.50f);
  g_guiFont.drawString(cx, cy + 18.0f, "Look up Bedwars statistics, ranks, and 3D skin previews",
                       applyAlpha(0xFF8A90A0, alpha), 0.36f);
  cy += 38.0f;

  float totalSearchW = g_w - 210.0f;
  float searchH = 38.0f;
  float searchX = mainX + 190.0f;
  float btnW = 76.0f;
  float inputW = totalSearchW - btnW - 8.0f;

  bool hSearch = isHovered(mx, my, searchX, cy, inputW, searchH);
  glDisable(GL_TEXTURE_2D);
  drawTextInput(searchX, cy, inputW, searchH, s_typingSearch, hSearch, alpha);

  if (s_typingSearch) {
    RenderUtils::drawGlow(searchX, cy, inputW, searchH, ClickGUITheme::controlRadius(), ClickGUITheme::accent(), 0.16f * alpha);
  }

  {
    float iconCx = searchX + 16.0f;
    float iconCy = cy + searchH * 0.5f - 1.0f;
    float r = 4.5f;
    DWORD iconCol = s_typingSearch ? ClickGUITheme::accent() : 0xFF7A8090;
    RenderUtils::drawCircle(iconCx, iconCy, r, applyAlpha(iconCol, alpha));
    RenderUtils::drawCircle(iconCx, iconCy, r - 1.3f, applyAlpha(ClickGUITheme::inset(), alpha));
    glLineWidth(1.6f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float aF = (((iconCol >> 24) & 0xFF) / 255.0f) * alpha;
    float rF = ((iconCol >> 16) & 0xFF) / 255.0f;
    float gF = ((iconCol >> 8) & 0xFF) / 255.0f;
    float bF = (iconCol & 0xFF) / 255.0f;
    glColor4f(rF, gF, bF, aF);
    glBegin(GL_LINES);
    glVertex2f(iconCx + 3.0f, iconCy + 3.0f);
    glVertex2f(iconCx + 7.0f, iconCy + 7.0f);
    glEnd();
    glLineWidth(1.0f);
  }
  glEnable(GL_TEXTURE_2D);

  std::string dispSearch = s_playerSearch;
  if (s_typingSearch && (GetTickCount64() / 500) % 2 == 0)
    dispSearch += "|";
  if (dispSearch.empty() && !s_typingSearch)
    dispSearch = "Search player name...";

  float textX = searchX + 32.0f;
  g_guiFont.drawString(
      textX, cy + 7.5f, dispSearch,
      applyAlpha(s_typingSearch ? 0xFFFFFFFF : 0xFF656A78, alpha), 0.48f);

  bool hClear = false;
  if (!s_playerSearch.empty()) {
    float clearX = searchX + inputW - 22.0f;
    float clearY = cy + 10.0f;
    hClear = isHovered(mx, my, clearX - 4.0f, clearY - 4.0f, 20.0f, 20.0f);
    g_guiFont.drawString(
        clearX, clearY, "\xC3\x97", // × symbol
        applyAlpha(hClear ? 0xFFFFFFFF : 0xFF7A8090, alpha), 0.44f);
    if (clickEvent && hClear) {
      s_playerSearch.clear();
      s_typingSearch = true;
    }
  }

  float btnX = searchX + inputW + 8.0f;
  bool canSearch = !s_playerSearch.empty() && !s_searching;
  bool hBtn = isHovered(mx, my, btnX, cy, btnW, searchH);

  if (clickEvent && hSearch && !hClear) {
    s_typingSearch = true;
    s_typingApiKey = s_typingAutoGG = s_typingUrchinKey = false;
  } else if (clickEvent && !hSearch && !hBtn) {
    s_typingSearch = false;
  }

  glDisable(GL_TEXTURE_2D);
  drawThemeButton(btnX, cy, btnW, searchH, hBtn && canSearch, false, alpha * (canSearch ? 1.0f : 0.5f));
  if (hBtn && canSearch) {
    RenderUtils::drawRoundedRect(btnX, cy, btnW, searchH, ClickGUITheme::buttonRadius(), ClickGUITheme::accent(), 0.12f * alpha);
  }
  glEnable(GL_TEXTURE_2D);

  const char *btnLabel = s_searching ? "Searching" : "Search";
  float lblW = g_guiFont.getStringWidth(btnLabel) * (0.40f / 0.5f);
  g_guiFont.drawString(
      btnX + (btnW - lblW) * 0.5f, cy + 9.5f, btnLabel,
      applyAlpha(canSearch ? (hBtn ? 0xFFFFFFFF : ClickGUITheme::accent()) : 0xFF656A78, alpha), 0.40f);

  if (clickEvent && hBtn && canSearch) {
    triggerPlayerSearch(s_playerSearch);
    s_typingSearch = false;
  }

  cy += searchH + 16.0f;

  if (s_searching) {
    float loadCardW = g_w - 210;
    float loadCardH = 50.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(mainX + 190, cy, loadCardW, loadCardH, false, alpha);
    glEnable(GL_TEXTURE_2D);
    int dots = 1 + ((GetTickCount64() / 400) % 3);
    std::string loadText = "Fetching stats";
    for (int i = 0; i < dots; i++)
      loadText += ".";
    g_guiFont.drawString(mainX + 210, cy + 16, loadText,
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += loadCardH + 10;
  } else if (s_hasLookup) {
    if (s_skinPendingReady) {
      if (s_lookupSkinTexId) {
        glDeleteTextures(1, &s_lookupSkinTexId);
        s_lookupSkinTexId = 0;
      }
      glGenTextures(1, &s_lookupSkinTexId);
      glBindTexture(GL_TEXTURE_2D, s_lookupSkinTexId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_skinPendingW, s_skinPendingH,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, s_skinPendingData.data());
      glBindTexture(GL_TEXTURE_2D, 0);
      s_lookupSkinTexW = s_skinPendingW;
      s_lookupSkinTexH = s_skinPendingH;
      s_skinPendingReady = false;
      s_skinPendingData.clear();
    }

    if (s_headPendingReady) {
      if (s_lookupHeadTexId) {
        glDeleteTextures(1, &s_lookupHeadTexId);
        s_lookupHeadTexId = 0;
      }
      glGenTextures(1, &s_lookupHeadTexId);
      glBindTexture(GL_TEXTURE_2D, s_lookupHeadTexId);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_headPendingW, s_headPendingH,
                   0, GL_RGBA, GL_UNSIGNED_BYTE, s_headPendingData.data());
      glBindTexture(GL_TEXTURE_2D, 0);
      s_headPendingReady = false;
      s_headPendingData.clear();
    }

    float cardW = g_w - 210.0f;
    float cardX = mainX + 190.0f;

    auto getRankDisplay = [&]() -> std::string {
      const auto &r = s_lookupResult;
      if (!r.prefix.empty())
        return r.prefix;
      if (r.rank == "ADMIN")
        return "\xC2\xA7" "c[ADMIN]";
      if (r.rank == "MODERATOR")
        return "\xC2\xA7" "2[MOD]";
      if (r.rank == "HELPER")
        return "\xC2\xA7" "9[HELPER]";
      if (r.rank == "YOUTUBER")
        return "\xC2\xA7" "c[\xC2\xA7" "fYOUTUBE\xC2\xA7" "c]";
      if (r.monthlyPackageRank == "SUPERSTAR") {
        std::string plusCol = "\xC2\xA7" "c";
        if (r.rankPlusColor == "GOLD")        plusCol = "\xC2\xA7" "6";
        else if (r.rankPlusColor == "AQUA")   plusCol = "\xC2\xA7" "b";
        else if (r.rankPlusColor == "GREEN")  plusCol = "\xC2\xA7" "a";
        else if (r.rankPlusColor == "LIGHT_PURPLE") plusCol = "\xC2\xA7" "d";
        else if (r.rankPlusColor == "WHITE")  plusCol = "\xC2\xA7" "f";
        else if (r.rankPlusColor == "BLUE")   plusCol = "\xC2\xA7" "9";
        else if (r.rankPlusColor == "DARK_RED")    plusCol = "\xC2\xA7" "4";
        else if (r.rankPlusColor == "DARK_AQUA")   plusCol = "\xC2\xA7" "3";
        else if (r.rankPlusColor == "DARK_GREEN")  plusCol = "\xC2\xA7" "2";
        else if (r.rankPlusColor == "DARK_PURPLE") plusCol = "\xC2\xA7" "5";
        else if (r.rankPlusColor == "YELLOW") plusCol = "\xC2\xA7" "e";
        return "\xC2\xA7" "6[MVP" + plusCol + "++" + "\xC2\xA7" "6]";
      }
      std::string activeRank = r.newPackageRank;
      if (activeRank.empty()) activeRank = r.packageRank;

      if (activeRank == "MVP_PLUS") {
        std::string plusCol = "\xC2\xA7" "c";
        if (r.rankPlusColor == "GOLD")        plusCol = "\xC2\xA7" "6";
        else if (r.rankPlusColor == "AQUA")   plusCol = "\xC2\xA7" "b";
        else if (r.rankPlusColor == "GREEN")  plusCol = "\xC2\xA7" "a";
        else if (r.rankPlusColor == "LIGHT_PURPLE") plusCol = "\xC2\xA7" "d";
        else if (r.rankPlusColor == "WHITE")  plusCol = "\xC2\xA7" "f";
        else if (r.rankPlusColor == "BLUE")   plusCol = "\xC2\xA7" "9";
        else if (r.rankPlusColor == "DARK_RED")    plusCol = "\xC2\xA7" "4";
        else if (r.rankPlusColor == "DARK_AQUA")   plusCol = "\xC2\xA7" "3";
        else if (r.rankPlusColor == "DARK_GREEN")  plusCol = "\xC2\xA7" "2";
        else if (r.rankPlusColor == "DARK_PURPLE") plusCol = "\xC2\xA7" "5";
        else if (r.rankPlusColor == "YELLOW") plusCol = "\xC2\xA7" "e";
        return "\xC2\xA7" "b[MVP" + plusCol + "+" + "\xC2\xA7" "b]";
      }
      if (activeRank == "MVP")      return "\xC2\xA7" "b[MVP]";
      if (activeRank == "VIP_PLUS") return "\xC2\xA7" "a[VIP\xC2\xA7" "6+\xC2\xA7" "a]";
      if (activeRank == "VIP")      return "\xC2\xA7" "a[VIP]";
      return "\xC2\xA7" "7";
    };

    float headerH = 64.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, headerH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    float textOffX = 0;
    GLuint hTex = s_lookupHeadTexId ? s_lookupHeadTexId : 0;
    if (hTex) {
      float sw = 42.0f;
      float sh = 42.0f;
      float sx = cardX + 10.0f;
      float sy = cy + (headerH - sh) * 0.5f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(sx - 1.0f, sy - 1.0f, sw + 2.0f, sh + 2.0f, 4.0f, ClickGUITheme::accent(), 0.35f * alpha);
      glEnable(GL_TEXTURE_2D);

      glColor4f(1.0f, 1.0f, 1.0f, alpha);
      glBindTexture(GL_TEXTURE_2D, hTex);
      glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex2f(sx, sy);
      glTexCoord2f(0.0f, 1.0f); glVertex2f(sx, sy + sh);
      glTexCoord2f(1.0f, 1.0f); glVertex2f(sx + sw, sy + sh);
      glTexCoord2f(1.0f, 0.0f); glVertex2f(sx + sw, sy);
      glEnd();
      glBindTexture(GL_TEXTURE_2D, 0);
      textOffX = 52.0f;
    }

    std::string displayNameToUse = !s_lookupResult.displayName.empty() ? s_lookupResult.displayName : s_lookupName;
    std::string rankStr = getRankDisplay();
    std::string nameWithRank = rankStr.empty() ? displayNameToUse : (rankStr + " " + displayNameToUse);
    if (!Render::g_mcFont.ready) {
      JNIEnv *env = lc ? lc->getEnv() : nullptr;
      if (env) {
        Render::g_mcFont.init(env);
      }
    }
    if (Render::g_mcFont.ready) {
      glPushMatrix();
      glTranslatef(cardX + 14.0f + textOffX, cy + 9.0f, 0.0f);
      glScalef(2.1f, 2.1f, 1.0f);
      Render::g_mcFont.drawStringWithShadow(nameWithRank, 0.0f, 0.0f, 0xFFFFFFFF);
      glPopMatrix();
    } else {
      g_guiFont.drawString(cardX + 14.0f + textOffX, cy + 9.0f, nameWithRank,
                           applyAlpha(0xFFFFFFFF, alpha), 0.60f);
    }

    std::string starFormatted = BedwarsStars::GetFormattedLevel(s_lookupResult);
    if (!Render::g_mcFont.ready) {
      JNIEnv *env = lc ? lc->getEnv() : nullptr;
      if (env) {
        Render::g_mcFont.init(env);
      }
    }
    if (Render::g_mcFont.ready) {
      glPushMatrix();
      glTranslatef(cardX + 14.0f + textOffX, cy + 34.0f, 0.0f);
      glScalef(2.0f, 2.0f, 1.0f);
      Render::g_mcFont.drawStringWithShadow(starFormatted, 0.0f, 0.0f, 0xFFFFFFFF);
      glPopMatrix();
    } else {
      g_guiFont.drawString(cardX + 14.0f + textOffX, cy + 34.0f, starFormatted,
                           applyAlpha(0xFFFFFFFF, alpha), 0.55f);
    }

    std::string nlText = "Network Level " + (s_lookupResult.networkLevel > 0 ? std::to_string(s_lookupResult.networkLevel) : "N/A");
    float nlW = g_guiFont.getStringWidth(nlText) * (0.42f / 0.5f);
    g_guiFont.drawString(cardX + cardW - nlW - 16.0f, cy + 10.0f, nlText,
                         applyAlpha(0xFF9AA0B0, alpha), 0.42f);

    if (!s_lookupResult.uuid.empty()) {
      std::string shortUuid = "UUID: " + s_lookupResult.uuid.substr(0, 8) + "...";
      float uuidW = g_guiFont.getStringWidth(shortUuid) * (0.36f / 0.5f);
      g_guiFont.drawString(cardX + cardW - uuidW - 16.0f, cy + 28.0f, shortUuid,
                           applyAlpha(0xFF656A78, alpha), 0.36f);
    }

    cy += headerH + 10.0f;

    float skinPaneW = 185.0f;
    float statsW = cardW - skinPaneW - 10.0f;
    float mainH = 264.0f;

    auto fmtK = [](int v) -> std::string {
      if (v >= 1000000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fM", v / 1000000.0);
        return buf;
      }
      if (v >= 10000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fK", v / 1000.0);
        return buf;
      }
      return std::to_string(v);
    };
    auto fmtR = [](double v) -> std::string {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.2f", v);
      return buf;
    };

    int fk = s_lookupResult.bedwarsFinalKills;
    int fd = s_lookupResult.bedwarsFinalDeaths;
    int wins = s_lookupResult.bedwarsWins;
    int losses = s_lookupResult.bedwarsLosses;
    int beds = s_lookupResult.bedwarsBedsBroken;
    int bedsLost = s_lookupResult.bedwarsBedsLost;
    int kills = s_lookupResult.bedwarsKills;
    int deaths = s_lookupResult.bedwarsDeaths;
    int games = wins + losses;
    int ws = s_lookupResult.winstreak;

    double fkdr = (fd == 0) ? (double)fk : (double)fk / fd;
    double wlr = (losses == 0) ? (double)wins : (double)wins / losses;
    double blr = (bedsLost == 0) ? (double)beds : (double)beds / bedsLost;
    double kdr = (deaths == 0) ? (double)kills : (double)kills / deaths;
    double winRatePct = (games > 0) ? ((double)wins * 100.0 / games) : 0.0;

    float skinX = cardX + statsW + 10.0f;
    float skinY = cy;

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(skinX, skinY, skinPaneW, mainH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    std::string titleStr = "3D SKIN PREVIEW";
    if (s_lookupSkinTexId) {
      titleStr += s_skinIsSlim ? " \xC2\xA7" "b[SLIM]" : " \xC2\xA7" "7[CLASSIC]";
    }
    g_guiFont.drawString(skinX + 14.0f, skinY + 10.0f, titleStr,
                         applyAlpha(0xFF8A90A0, alpha), 0.34f);

    // Mouse drag to rotate
    bool hSkinPane = isHovered(mx, my, skinX, skinY + 26.0f, skinPaneW, mainH - 26.0f);
    if (clickEvent && hSkinPane) {
      s_skinDragging = true;
      s_lastDragX = mx;
      s_lastDragY = my;
    }
    if (s_skinDragging) {
      if (ctx.lClick) {
        float deltaX = mx - s_lastDragX;
        float deltaY = my - s_lastDragY;
        s_skinYaw += deltaX * 1.2f;
        s_skinPitch += deltaY * 0.8f;
        if (s_skinPitch > 45.0f)  s_skinPitch = 45.0f;
        if (s_skinPitch < -45.0f) s_skinPitch = -45.0f;
        s_lastDragX = mx;
        s_lastDragY = my;
      } else {
        s_skinDragging = false;
      }
    } else if (!hSkinPane) {
      s_skinYaw += 0.20f;
      if (s_skinYaw >= 360.0f) s_skinYaw -= 360.0f;
    }

    if (s_lookupSkinTexId) {
      float centerX = skinX + skinPaneW * 0.5f;
      float centerY = skinY + mainH * 0.5f + 2.0f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawGlow(centerX - 35.0f, centerY - 45.0f, 70.0f, 110.0f, 24.0f,
                           ClickGUITheme::accent(), 0.14f * alpha);
      RenderUtils::drawRoundedRect(centerX - 30.0f, centerY + 82.0f, 60.0f, 7.0f, 3.5f, 0xFF000000, 0.40f * alpha);
      glEnable(GL_TEXTURE_2D);

      // Save current GL state
      GLint viewport[4];
      glGetIntegerv(GL_VIEWPORT, viewport);
      GLfloat prevProjMatrix[16], prevMVMatrix[16];
      glGetFloatv(GL_PROJECTION_MATRIX, prevProjMatrix);
      glGetFloatv(GL_MODELVIEW_MATRIX, prevMVMatrix);
      GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
      GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

      float renderX = skinX;
      float renderY = skinY + 26.0f;
      float renderW = skinPaneW;
      float renderH = mainH - 26.0f;

      int vpX = (int)(renderX);
      int vpY = viewport[3] - (int)(renderY + renderH);
      int vpW = (int)(renderW);
      int vpH = (int)(renderH);

      glViewport(vpX, vpY, vpW, vpH);

      // Perspective projection
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      float aspect = renderW / renderH;
      float fovY = 30.0f;
      float nearP = 10.0f, farP = 200.0f;
      float top = nearP * tanf(fovY * 3.14159265f / 360.0f);
      float right = top * aspect;
      glFrustum(-right, right, -top, top, nearP, farP);

      // Modelview: camera + model rotation
      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
      glTranslatef(0.0f, 0.0f, -68.0f);
      glRotatef(s_skinPitch, 1.0f, 0.0f, 0.0f);
      glRotatef(s_skinYaw, 0.0f, 1.0f, 0.0f);

      // Enable depth test for proper 3D rendering
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LEQUAL);
      glClear(GL_DEPTH_BUFFER_BIT);
      glDisable(GL_CULL_FACE);

      glEnable(GL_TEXTURE_2D);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindTexture(GL_TEXTURE_2D, s_lookupSkinTexId);

      renderPlayerModel3D((float)s_lookupSkinTexW, (float)s_lookupSkinTexH, alpha, s_skinIsSlim);

      glBindTexture(GL_TEXTURE_2D, 0);

      // Restore previous GL state
      if (!depthWasEnabled) glDisable(GL_DEPTH_TEST);
      if (cullWasEnabled) glEnable(GL_CULL_FACE);

      glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
      glMatrixMode(GL_PROJECTION);
      glLoadMatrixf(prevProjMatrix);
      glMatrixMode(GL_MODELVIEW);
      glLoadMatrixf(prevMVMatrix);

      std::string hint = s_skinDragging ? "Rotating..." : "Drag to rotate";
      float hintW = g_guiFont.getStringWidth(hint) * (0.30f / 0.5f);
      g_guiFont.drawString(skinX + (skinPaneW - hintW) * 0.5f, skinY + mainH - 16.0f, hint,
                           applyAlpha(s_skinDragging ? ClickGUITheme::accent() : 0xFF656A78, alpha), 0.30f);
    } else if (s_skinLoading) {
      int dots = 1 + ((GetTickCount64() / 350) % 3);
      std::string sText = "Loading Skin";
      for (int d = 0; d < dots; ++d) sText += ".";
      float stW = g_guiFont.getStringWidth(sText) * (0.38f / 0.5f);
      g_guiFont.drawString(skinX + (skinPaneW - stW) * 0.5f, skinY + mainH * 0.5f - 8.0f, sText,
                           applyAlpha(0xFFA0A5B5, alpha), 0.38f);
    } else {
      std::string noSkin = "No 3D Model";
      float nsW = g_guiFont.getStringWidth(noSkin) * (0.38f / 0.5f);
      g_guiFont.drawString(skinX + (skinPaneW - nsW) * 0.5f, skinY + mainH * 0.5f - 8.0f, noSkin,
                           applyAlpha(0xFF656A78, alpha), 0.38f);
    }

    float statsX = cardX;
    float statsY = cy;

    {
      float chipY = statsY;
      float chipH = 30.0f;
      float chipGap = 8.0f;
      float chipW = (statsW - chipGap * 2.0f) / 3.0f;

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(statsX, chipY, chipW, chipH, false, alpha);
      RenderUtils::drawRoundedRect(statsX + 2.0f, chipY + 4.0f, 3.0f, chipH - 8.0f, 1.5f, 0xFFF59E0B, 0.9f * alpha);
      glEnable(GL_TEXTURE_2D);
      std::string wsStr = "\xC2\xA7" "6Winstreak: \xC2\xA7" "f" + std::to_string(ws);
      g_guiFont.drawString(statsX + 10.0f, chipY + 7.0f, wsStr, applyAlpha(0xFFFFFFFF, alpha), 0.38f);

      float chip2X = statsX + chipW + chipGap;
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(chip2X, chipY, chipW, chipH, false, alpha);
      RenderUtils::drawRoundedRect(chip2X + 2.0f, chipY + 4.0f, 3.0f, chipH - 8.0f, 1.5f, 0xFF3B82F6, 0.9f * alpha);
      glEnable(GL_TEXTURE_2D);
      std::string gmStr = "\xC2\xA7" "9Games: \xC2\xA7" "f" + fmtK(games);
      g_guiFont.drawString(chip2X + 10.0f, chipY + 7.0f, gmStr, applyAlpha(0xFFFFFFFF, alpha), 0.38f);

      float chip3X = chip2X + chipW + chipGap;
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(chip3X, chipY, chipW, chipH, false, alpha);
      RenderUtils::drawRoundedRect(chip3X + 2.0f, chipY + 4.0f, 3.0f, chipH - 8.0f, 1.5f, 0xFF10B981, 0.9f * alpha);
      glEnable(GL_TEXTURE_2D);
      std::string wrStr = "\xC2\xA7" "aWin Rate: \xC2\xA7" "f" + fmtR(winRatePct) + "%";
      g_guiFont.drawString(chip3X + 10.0f, chipY + 7.0f, wrStr, applyAlpha(0xFFFFFFFF, alpha), 0.38f);
    }

    struct ModernStatCard {
      const char *title;
      std::string mainValue;
      uint32_t color;
      std::string breakdown;
      float ratioFrac;
    };

    float fkRatioFrac = (fk + fd > 0) ? (float)fk / (float)(fk + fd) : 0.0f;
    float wRatioFrac  = (wins + losses > 0) ? (float)wins / (float)(wins + losses) : 0.0f;
    float bRatioFrac  = (beds + bedsLost > 0) ? (float)beds / (float)(beds + bedsLost) : 0.0f;
    float kRatioFrac  = (kills + deaths > 0) ? (float)kills / (float)(kills + deaths) : 0.0f;

    ModernStatCard mCards[4] = {
      {
        "FKDR",
        fmtR(fkdr),
        StatColors::getColor(StatColors::StatType::FKDR, fkdr),
        fmtK(fk) + " Final Kills  /  " + std::to_string(fd) + " Deaths",
        fkRatioFrac
      },
      {
        "WLR",
        fmtR(wlr),
        StatColors::getColor(StatColors::StatType::WLR, wlr),
        fmtK(wins) + " Wins  /  " + std::to_string(losses) + " Losses",
        wRatioFrac
      },
      {
        "BBLR",
        fmtR(blr),
        StatColors::getColor(StatColors::StatType::BLR, blr),
        fmtK(beds) + " Beds Broken  /  " + std::to_string(bedsLost) + " Lost",
        bRatioFrac
      },
      {
        "KDR",
        fmtR(kdr),
        StatColors::getColor(StatColors::StatType::KDR, kdr),
        fmtK(kills) + " Kills  /  " + std::to_string(deaths) + " Deaths",
        kRatioFrac
      }
    };

    float gridStartY = statsY + 38.0f;
    float itemGap = 10.0f;
    float itemW = (statsW - itemGap) / 2.0f;
    float itemH = 106.0f;

    for (int i = 0; i < 4; ++i) {
      int col = i % 2;
      int row = i / 2;
      float ix = statsX + col * (itemW + itemGap);
      float iy = gridStartY + row * (itemH + itemGap);
      bool hItem = isHovered(mx, my, ix, iy, itemW, itemH);

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(ix, iy, itemW, itemH, hItem, alpha);

      RenderUtils::drawRoundedRect(ix + 2.0f, iy + 10.0f, 3.5f, itemH - 20.0f, 1.5f, mCards[i].color, 0.95f * alpha);
      RenderUtils::drawGlow(ix + 2.0f, iy + 10.0f, 3.5f, itemH - 20.0f, 5.0f, mCards[i].color, 0.22f * alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(ix + 14.0f, iy + 9.0f, mCards[i].title,
                           applyAlpha(0xFF8A90A0, alpha), 0.35f);

      g_guiFont.drawString(ix + 14.0f, iy + 25.0f, mCards[i].mainValue,
                           applyAlpha(mCards[i].color, alpha), 0.60f);

      g_guiFont.drawString(ix + 14.0f, iy + 60.0f, mCards[i].breakdown,
                           applyAlpha(0xFFB5BAC7, alpha), 0.36f);

      float barX = ix + 14.0f;
      float barY = iy + 88.0f;
      float barW = itemW - 28.0f;
      float barH = 4.0f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(barX, barY, barW, barH, 2.0f, 0xFF14171E, 0.85f * alpha);
      float rFrac = (mCards[i].ratioFrac < 0.0f) ? 0.0f : ((mCards[i].ratioFrac > 1.0f) ? 1.0f : mCards[i].ratioFrac);
      float fillW = barW * rFrac;
      if (fillW > 2.0f) {
        RenderUtils::drawRoundedRect(barX, barY, fillW, barH, 2.0f, mCards[i].color, 0.90f * alpha);
        RenderUtils::drawGlow(barX, barY, fillW, barH, 3.0f, mCards[i].color, 0.25f * alpha);
      }
      glEnable(GL_TEXTURE_2D);
    }

    cy += mainH + 12.0f;

    if (s_lookupUrchinMonthly.has_value() && s_lookupUrchinMonthly->hasData) {
      const auto &m = *s_lookupUrchinMonthly;
      float mCardH = 74.0f;
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, mCardH, false, alpha);
      glEnable(GL_TEXTURE_2D);

      std::string mTitle = "MONTHLY BEDWARS STATS";
      if (!m.fromReadable.empty()) {
        mTitle += " \xC2\xA7" "7(Since " + m.fromReadable + ")";
      }
      g_guiFont.drawString(cardX + 14.0f, cy + 10.0f, mTitle,
                           applyAlpha(0xFF8A90A0, alpha), 0.35f);

      float mBoxY = cy + 28.0f;
      float mBoxH = 36.0f;
      float mGap = 8.0f;
      float mBoxW = (cardW - 28.0f - mGap * 4.0f) / 5.0f;

      struct MStatItem {
        const char *label;
        std::string val;
        uint32_t color;
      };

      char mFkdrBuf[32];
      snprintf(mFkdrBuf, sizeof(mFkdrBuf), "%.2f", m.fkdr);
      char mWlrBuf[32];
      snprintf(mWlrBuf, sizeof(mWlrBuf), "%.2f", m.wlr);

      MStatItem mItems[5] = {
        {"Monthly FKDR", mFkdrBuf, StatColors::getColor(StatColors::StatType::FKDR, m.fkdr)},
        {"Final Kills", std::string("+") + fmtK(m.finalKills), 0xFF10B981},
        {"Final Deaths", std::string("+") + fmtK(m.finalDeaths), 0xFFEF4444},
        {"Monthly WLR", mWlrBuf, StatColors::getColor(StatColors::StatType::WLR, m.wlr)},
        {"Monthly Wins", std::string("+") + fmtK(m.wins), 0xFFF59E0B}
      };

      for (int i = 0; i < 5; ++i) {
        float bx = cardX + 14.0f + i * (mBoxW + mGap);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(bx, mBoxY, mBoxW, mBoxH, 4.0f, 0xFF14171E, 0.85f * alpha);
        RenderUtils::drawRoundedRect(bx + 2.0f, mBoxY + 5.0f, 2.5f, mBoxH - 10.0f, 1.2f, mItems[i].color, 0.95f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(bx + 8.0f, mBoxY + 5.0f, mItems[i].label,
                             applyAlpha(0xFF8A90A0, alpha), 0.30f);
        g_guiFont.drawString(bx + 8.0f, mBoxY + 18.0f, mItems[i].val,
                             applyAlpha(mItems[i].color, alpha), 0.42f);
      }
      cy += mCardH + 10.0f;
    }

    float resCardH = 74.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, resCardH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cardX + 14.0f, cy + 10.0f, "RESOURCES COLLECTED & PROGRESSION",
                         applyAlpha(0xFF8A90A0, alpha), 0.35f);

    {
      float resBoxY = cy + 28.0f;
      float resBoxH = 36.0f;
      float resGap = 8.0f;
      float resBoxW = (cardW - 28.0f - resGap * 4.0f) / 5.0f;

      struct ResItem {
        const char *name;
        std::string val;
        uint32_t color;
      };

      ResItem items[5] = {
        {"Iron", fmtK(s_lookupResult.ironCollected), 0xFFCBD5E1},
        {"Gold", fmtK(s_lookupResult.goldCollected), 0xFFFBBF24},
        {"Diamond", fmtK(s_lookupResult.diamondCollected), 0xFF38BDF8},
        {"Emerald", fmtK(s_lookupResult.emeraldCollected), 0xFF34D399},
        {"Slumber Tickets", fmtK(s_lookupResult.slumberTickets), 0xFFA78BFA}
      };

      for (int i = 0; i < 5; ++i) {
        float rx = cardX + 14.0f + i * (resBoxW + resGap);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(rx, resBoxY, resBoxW, resBoxH, 4.0f, 0xFF14171E, 0.85f * alpha);
        RenderUtils::drawRoundedRect(rx + 2.0f, resBoxY + 5.0f, 2.5f, resBoxH - 10.0f, 1.2f, items[i].color, 0.95f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(rx + 8.0f, resBoxY + 5.0f, items[i].name,
                             applyAlpha(0xFF8A90A0, alpha), 0.30f);
        g_guiFont.drawString(rx + 8.0f, resBoxY + 18.0f, items[i].val,
                             applyAlpha(items[i].color, alpha), 0.42f);
      }
    }
    cy += resCardH + 10.0f;

    auto formatCosmeticName = [](std::string id) -> std::string {
      if (id.empty() || id == "none" || id == "null") return "None";
      for (const char *prefix : {"deathcry_", "killeffect_", "victorydance_", "projectiletrail_", "islandtopper_", "topper_", "glyph_", "beddestroy_", "star_"}) {
        if (id.rfind(prefix, 0) == 0) {
          id = id.substr(strlen(prefix));
          break;
        }
      }
      bool capNext = true;
      for (char &c : id) {
        if (c == '_') {
          c = ' ';
          capNext = true;
        } else if (capNext) {
          c = (char)toupper((unsigned char)c);
          capNext = false;
        }
      }
      return id;
    };

    struct CosmeticSlot {
      const char *label;
      std::string val;
    };

    CosmeticSlot cosmSlots[] = {
      {"Kill Effect", formatCosmeticName(s_lookupResult.activeKillEffect)},
      {"Death Cry", formatCosmeticName(s_lookupResult.activeDeathCry)},
      {"Victory Dance", formatCosmeticName(s_lookupResult.activeVictoryDance)},
      {"Projectile Trail", formatCosmeticName(s_lookupResult.activeProjectileTrail)},
      {"Island Topper", formatCosmeticName(s_lookupResult.activeIslandTopper)},
      {"Bed Destroy", formatCosmeticName(s_lookupResult.activeBedDestroy)},
      {"Glyph", formatCosmeticName(s_lookupResult.activeGlyph)},
      {"Active Star", s_lookupResult.activeStar.empty() ? "Standard" : formatCosmeticName(s_lookupResult.activeStar)}
    };
    int totalCosmetics = sizeof(cosmSlots) / sizeof(cosmSlots[0]);

    float cosmCardH = 110.0f;
    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy, cardW, cosmCardH, false, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cardX + 14.0f, cy + 10.0f, "ACTIVE BEDWARS COSMETICS",
                         applyAlpha(0xFF8A90A0, alpha), 0.35f);

    {
      float cItemGap = 8.0f;
      float cCols = 4;
      float cItemW = (cardW - 28.0f - cItemGap * (cCols - 1)) / cCols;
      float cItemH = 32.0f;
      float cStartY = cy + 28.0f;

      for (int i = 0; i < totalCosmetics; ++i) {
        int col = i % 4;
        int row = i / 4;
        float cxPos = cardX + 14.0f + col * (cItemW + cItemGap);
        float cyPos = cStartY + row * (cItemH + 6.0f);

        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(cxPos, cyPos, cItemW, cItemH, 4.0f, 0xFF14171E, 0.85f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(cxPos + 8.0f, cyPos + 4.0f, cosmSlots[i].label,
                             applyAlpha(0xFF8A90A0, alpha), 0.30f);
        uint32_t valCol = (cosmSlots[i].val == "None") ? 0xFF656A78 : 0xFFFFFFFF;
        g_guiFont.drawString(cxPos + 8.0f, cyPos + 16.0f, cosmSlots[i].val,
                             applyAlpha(valCol, alpha), 0.36f);
      }
    }
    cy += cosmCardH + 10.0f;

    {
      const char *defaultQuickBuy[21] = {
        "wool", "hardened_clay", "blast_proof_glass", "end_stone", "ladder", "oak_wood_planks", "obsidian",
        "stone_sword", "iron_sword", "diamond_sword", "knockback_stick", "chainmail_armor", "iron_armor", "diamond_armor",
        "shears", "wooden_pickaxe", "wooden_axe", "arrow", "bow", "speed_potion", "tnt"
      };

      std::vector<std::string> qbSlots(21);
      if (!s_lookupResult.quickBuy.empty()) {
        std::stringstream ss(s_lookupResult.quickBuy);
        std::string item;
        int idx = 0;
        while (std::getline(ss, item, ',') && idx < 21) {
          qbSlots[idx++] = item;
        }
        while (idx < 21) {
          qbSlots[idx] = defaultQuickBuy[idx];
          idx++;
        }
      } else {
        for (int i = 0; i < 21; ++i) {
          qbSlots[i] = defaultQuickBuy[i];
        }
      }

      auto getItemDisplayName = [&](const std::string &raw) -> std::string {
        if (raw.empty() || raw == "null") return "Empty";
        std::string s = raw;
        for (char &c : s) c = (char)tolower((unsigned char)c);
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();

        if (s == "wool" || s == "white_wool") return "Wool";
        if (s == "wood" || s == "oak_planks" || s == "oak_wood_planks") return "Oak Wood Planks";
        if (s == "hardened_clay" || s == "clay" || s == "terracotta") return "Hardened Clay";
        if (s == "blast_proof_glass" || s == "glass") return "Blast-Proof Glass";
        if (s == "end_stone") return "End Stone";
        if (s == "ladder") return "Ladder";
        if (s == "obsidian") return "Obsidian";
        if (s == "sponge") return "Sponge";
        if (s == "stone_sword") return "Stone Sword";
        if (s == "iron_sword") return "Iron Sword";
        if (s == "diamond_sword") return "Diamond Sword";
        if (s == "wooden_sword" || s == "wood_sword") return "Wooden Sword";
        if (s == "knockback_stick" || s == "stick") return "Stick (Knockback I)";
        if (s == "chainmail_boots" || s == "chainmail_armor") return "Permanent Chainmail Armor";
        if (s == "iron_boots" || s == "iron_armor") return "Permanent Iron Armor";
        if (s == "diamond_boots" || s == "diamond_armor") return "Permanent Diamond Armor";
        if (s == "shears" || s == "permanent_shears") return "Permanent Shears";
        if (s == "bow") return "Bow";
        if (s == "power_bow") return "Bow (Power I)";
        if (s == "punch_bow") return "Bow (Punch I, Power I)";
        if (s == "arrow") return "Arrows";
        if (s == "speed_potion" || s.find("speed") != std::string::npos) return "Speed II Potion (45s)";
        if (s == "jump_potion" || s.find("jump") != std::string::npos) return "Jump V Potion (45s)";
        if (s == "invis_potion" || s.find("invis") != std::string::npos) return "Invisibility Potion (30s)";
        if (s == "water_bucket" || s == "bucket_water") return "Water Bucket";
        if (s == "bridge_egg" || s == "egg") return "Bridge Egg";
        if (s == "magic_milk" || s == "milk" || s == "milk_bucket") return "Magic Milk";
        if (s == "wooden_pickaxe" || s == "wood_pickaxe") return "Wooden Pickaxe";
        if (s == "stone_pickaxe") return "Stone Pickaxe";
        if (s == "iron_pickaxe") return "Iron Pickaxe";
        if (s == "diamond_pickaxe" || s == "pickaxe") return "Diamond Pickaxe";
        if (s == "wooden_axe" || s == "wood_axe") return "Wooden Axe";
        if (s == "stone_axe") return "Stone Axe";
        if (s == "iron_axe") return "Iron Axe";
        if (s == "diamond_axe" || s == "axe") return "Diamond Axe";
        if (s == "golden_apple") return "Golden Apple";
        if (s == "fireball") return "Fireball";
        if (s == "tnt") return "TNT";
        if (s == "ender_pearl") return "Ender Pearl";
        if (s == "bedbug" || s == "silverfish") return "Bedbug";
        if (s == "dream_defender" || s == "iron_golem") return "Dream Defender";
        if (s == "compact_pop_up_tower" || s == "popup_tower") return "Compact Pop-up Tower";
        if (s == "compact_chest" || s == "chest" || s == "ender_chest") return "Compact Chest";
        if (s == "tools_1" || s == "tools 1") return "Pickaxe Slot";
        if (s == "tools_2" || s == "tools 2") return "Axe Slot";
        if (s == "tools_3" || s == "tools 3") return "Shears Slot";
        if (s == "tools") return "Tools Slot";
        if (s == "melee") return "Melee Slot";
        if (s == "blocks") return "Blocks Slot";
        if (s == "ranged") return "Ranged Slot";
        if (s == "potions") return "Potions Slot";
        if (s == "utility") return "Utility Slot";
        if (s == "compass") return "Compass Slot";
        return formatCosmeticName(raw);
      };

      const int kCols = 9;
      const int kRows = 5;
      float slotSize = 38.0f;
      float slotGap = 2.0f;
      float gridW = kCols * slotSize + (kCols - 1) * slotGap;
      float gridH = kRows * slotSize + (kRows - 1) * slotGap;
      float containerW = gridW + 16.0f;
      float containerH = gridH + 16.0f;

      float qbCardH = containerH + 102.0f;
      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy, cardW, qbCardH, false, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cardX + 14.0f, cy + 10.0f, "QUICK BUY & HOTBAR CONFIGURATION",
                           applyAlpha(0xFF8A90A0, alpha), 0.35f);

      float chestX = cardX + 14.0f;
      float chestY = cy + 28.0f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(chestX, chestY, containerW, containerH, 5.0f, 0xFF14171E, 0.95f * alpha);
      RenderUtils::drawRoundedOutline(chestX, chestY, containerW, containerH, 5.0f, 1.0f, 0xFF2A2D35, 0.6f * alpha);
      glEnable(GL_TEXTURE_2D);

      float gridStartX = chestX + 8.0f;
      float gridStartY = chestY + 8.0f;
      std::string hoveredSlotName;

      auto drawSlotItem = [&](const std::string &k, float sx, float sy) {
        GLuint tid = getItemTexture(k);
        if (tid != 0) {
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          glEnable(GL_TEXTURE_2D);
          glBindTexture(GL_TEXTURE_2D, tid);
          glColor4f(1.0f, 1.0f, 1.0f, alpha);
          float pad = (k == "glass_pane") ? 1.0f : 2.0f;
          glBegin(GL_QUADS);
          glTexCoord2f(0.0f, 0.0f); glVertex2f(sx + pad, sy + pad);
          glTexCoord2f(0.0f, 1.0f); glVertex2f(sx + pad, sy + slotSize - pad);
          glTexCoord2f(1.0f, 1.0f); glVertex2f(sx + slotSize - pad, sy + slotSize - pad);
          glTexCoord2f(1.0f, 0.0f); glVertex2f(sx + slotSize - pad, sy + pad);
          glEnd();
          glBindTexture(GL_TEXTURE_2D, 0);
          return;
        }

        float cxP = sx + slotSize * 0.5f;
        float cyP = sy + slotSize * 0.5f;

        glDisable(GL_TEXTURE_2D);
        if (k == "glass_pane") {
          RenderUtils::drawRect(sx + 5.0f, sy + 5.0f, slotSize - 10.0f, slotSize - 10.0f, 0x664B5563, alpha);
          RenderUtils::drawOutline(sx + 5.0f, sy + 5.0f, slotSize - 10.0f, slotSize - 10.0f, 1.0f, 0x889CA3AF, alpha);
          glColor4f(1.0f, 1.0f, 1.0f, 0.45f * alpha);
          glLineWidth(1.0f);
          glBegin(GL_LINES);
          glVertex2f(sx + 6.0f, sy + 6.0f);
          glVertex2f(sx + slotSize - 7.0f, sy + slotSize - 7.0f);
          glEnd();
        } else if (k == "wool" || k == "white_wool") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFFF1F5F9, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFFCBD5E1, alpha);
        } else if (k == "wood" || k == "oak_planks" || k == "oak_wood_planks" || k == "planks") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFF926D43, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFF714F28, alpha);
          RenderUtils::drawRect(cxP - 9.0f, cyP - 3.0f, 18.0f, 1.5f, 0xFF583D1F, alpha);
          RenderUtils::drawRect(cxP - 9.0f, cyP + 3.5f, 18.0f, 1.5f, 0xFF583D1F, alpha);
        } else if (k == "hardened_clay" || k == "clay" || k == "terracotta") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFFD97757, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFFA34D35, alpha);
          RenderUtils::drawRect(cxP - 5.0f, cyP - 5.0f, 4.0f, 4.0f, 0xFFBC583B, alpha);
          RenderUtils::drawRect(cxP + 2.0f, cyP + 2.0f, 4.0f, 4.0f, 0xFFBC583B, alpha);
        } else if (k == "blast_proof_glass" || k == "glass") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0x6694A3B8, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFFCBD5E1, alpha);
          glColor4f(1.0f, 1.0f, 1.0f, 0.6f * alpha);
          glLineWidth(1.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 5.0f, cyP - 5.0f); glVertex2f(cxP - 1.0f, cyP - 1.0f);
          glVertex2f(cxP + 1.0f, cyP + 1.0f); glVertex2f(cxP + 5.0f, cyP + 5.0f);
          glEnd();
        } else if (k == "end_stone") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFFEFE9AC, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFFD8D08E, alpha);
          RenderUtils::drawRect(cxP - 5.0f, cyP - 3.0f, 3.0f, 3.0f, 0xFFC9C07A, alpha);
          RenderUtils::drawRect(cxP + 2.0f, cyP + 1.0f, 3.0f, 3.0f, 0xFFC9C07A, alpha);
        } else if (k == "obsidian") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFF1E142B, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFF3D235C, alpha);
          RenderUtils::drawRect(cxP - 4.0f, cyP - 4.0f, 3.0f, 3.0f, 0xFF583385, alpha);
          RenderUtils::drawRect(cxP + 2.0f, cyP + 1.0f, 2.5f, 2.5f, 0xFF7346A8, alpha);
        } else if (k == "ladder") {
          glColor4f(0.57f, 0.43f, 0.26f, alpha);
          glLineWidth(2.0f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP - 8.0f); glVertex2f(cxP - 6.0f, cyP + 8.0f);
          glVertex2f(cxP + 6.0f, cyP - 8.0f); glVertex2f(cxP + 6.0f, cyP + 8.0f);
          glVertex2f(cxP - 6.0f, cyP - 4.0f); glVertex2f(cxP + 6.0f, cyP - 4.0f);
          glVertex2f(cxP - 6.0f, cyP);        glVertex2f(cxP + 6.0f, cyP);
          glVertex2f(cxP - 6.0f, cyP + 4.0f); glVertex2f(cxP + 6.0f, cyP + 4.0f);
          glEnd();
        } else if (k == "tnt") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFFDC2626, alpha);
          RenderUtils::drawRect(cxP - 9.0f, cyP - 3.0f, 18.0f, 5.0f, 0xFFFFFFFF, alpha);
        } else if (k == "sponge") {
          RenderUtils::drawRoundedRect(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 2.0f, 0xFFEAB308, alpha);
          RenderUtils::drawOutline(cxP - 9.0f, cyP - 9.0f, 18.0f, 18.0f, 1.0f, 0xFFCA8A04, alpha);
          RenderUtils::drawCircle(cxP - 3.0f, cyP - 3.0f, 1.5f, 0xFFA16207, alpha);
          RenderUtils::drawCircle(cxP + 3.0f, cyP + 2.0f, 1.8f, 0xFFA16207, alpha);
          RenderUtils::drawCircle(cxP - 2.0f, cyP + 4.0f, 1.2f, 0xFFA16207, alpha);
        } else if (k == "chest" || k == "ender_chest" || k == "compact_chest") {
          uint32_t cCol = (k == "ender_chest") ? 0xFF1E293B : 0xFF785128;
          uint32_t oCol = (k == "ender_chest") ? 0xFF0F172A : 0xFF4A3219;
          RenderUtils::drawRoundedRect(cxP - 8.0f, cyP - 7.0f, 16.0f, 14.0f, 2.0f, cCol, alpha);
          RenderUtils::drawOutline(cxP - 8.0f, cyP - 7.0f, 16.0f, 14.0f, 1.0f, oCol, alpha);
          RenderUtils::drawRect(cxP - 1.5f, cyP - 2.0f, 3.0f, 4.0f, (k == "ender_chest") ? 0xFF38BDF8 : 0xFFFCD34D, alpha);
        } else if (k.find("sword") != std::string::npos) {
          uint32_t bCol = 0xFFCBD5E1;
          if (k == "stone_sword") bCol = 0xFF9CA3AF;
          else if (k == "diamond_sword") bCol = 0xFF38BDF8;
          else if (k == "gold_sword" || k == "golden_sword") bCol = 0xFFFBBF24;
          glColor4f(((bCol>>16)&0xFF)/255.f, ((bCol>>8)&0xFF)/255.f, (bCol&0xFF)/255.f, alpha);
          glLineWidth(3.0f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP + 6.0f);
          glVertex2f(cxP + 7.0f, cyP - 7.0f);
          glEnd();
          glColor4f(0.5f, 0.4f, 0.2f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 7.0f, cyP + 2.0f);
          glVertex2f(cxP - 2.0f, cyP + 7.0f);
          glEnd();
        } else if (k == "stick" || k == "knockback_stick" || k.find("stick") != std::string::npos) {
          glColor4f(0.55f, 0.35f, 0.15f, alpha);
          glLineWidth(3.0f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 7.0f, cyP + 7.0f);
          glVertex2f(cxP + 7.0f, cyP - 7.0f);
          glEnd();
          RenderUtils::drawCircle(cxP + 4.0f, cyP - 4.0f, 2.5f, 0xFFE879F9, 0.8f * alpha);
        } else if (k.find("boots") != std::string::npos) {
          uint32_t btCol = 0xFFCBD5E1;
          if (k == "chainmail_boots") btCol = 0xFFA3A3A3;
          else if (k == "diamond_boots") btCol = 0xFF38BDF8;
          RenderUtils::drawRoundedRect(cxP - 7.5f, cyP - 6.0f, 6.0f, 12.0f, 2.0f, btCol, alpha);
          RenderUtils::drawRoundedRect(cxP + 1.5f, cyP - 6.0f, 6.0f, 12.0f, 2.0f, btCol, alpha);
        } else if (k.find("armor") != std::string::npos || k.find("chestplate") != std::string::npos) {
          uint32_t arCol = 0xFFCBD5E1;
          if (k.find("chainmail") != std::string::npos) arCol = 0xFFA3A3A3;
          else if (k.find("diamond") != std::string::npos) arCol = 0xFF38BDF8;
          RenderUtils::drawRoundedRect(cxP - 7.0f, cyP - 7.0f, 14.0f, 14.0f, 2.0f, arCol, alpha);
          RenderUtils::drawRect(cxP - 2.0f, cyP - 7.0f, 4.0f, 3.5f, 0xFF1E293B, alpha);
        } else if (k.find("potion") != std::string::npos) {
          uint32_t potCol = 0xFF38BDF8;
          if (k == "jump_potion") potCol = 0xFF4ADE80;
          else if (k == "invis_potion") potCol = 0xFFF472B6;
          RenderUtils::drawRect(cxP - 2.0f, cyP - 8.0f, 4.0f, 4.0f, 0xFFE2E8F0, 0.85f * alpha);
          RenderUtils::drawRoundedRect(cxP - 6.0f, cyP - 4.0f, 12.0f, 12.0f, 3.0f, potCol, alpha);
        } else if (k == "bow" || k == "power_bow" || k == "punch_bow") {
          glColor4f(0.55f, 0.35f, 0.15f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINE_STRIP);
          glVertex2f(cxP - 7.0f, cyP - 7.0f);
          glVertex2f(cxP + 6.0f, cyP - 3.0f);
          glVertex2f(cxP + 6.0f, cyP + 3.0f);
          glVertex2f(cxP - 7.0f, cyP + 7.0f);
          glEnd();
          glColor4f(0.85f, 0.85f, 0.85f, 0.7f * alpha);
          glLineWidth(1.2f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 7.0f, cyP - 7.0f);
          glVertex2f(cxP - 7.0f, cyP + 7.0f);
          glEnd();
        } else if (k == "arrow") {
          glColor4f(0.8f, 0.8f, 0.8f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 7.0f, cyP + 7.0f);
          glVertex2f(cxP + 7.0f, cyP - 7.0f);
          glEnd();
          RenderUtils::drawCircle(cxP + 7.0f, cyP - 7.0f, 2.5f, 0xFF9CA3AF, alpha);
        } else if (k == "shears" || k == "permanent_shears") {
          glColor4f(0.8f, 0.8f, 0.8f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP - 6.0f);
          glVertex2f(cxP + 6.0f, cyP + 6.0f);
          glVertex2f(cxP - 6.0f, cyP + 6.0f);
          glVertex2f(cxP + 6.0f, cyP - 6.0f);
          glEnd();
          RenderUtils::drawRect(cxP - 6.0f, cyP + 3.0f, 4.0f, 4.0f, 0xFFDC2626, alpha);
          RenderUtils::drawRect(cxP + 2.0f, cyP + 3.0f, 4.0f, 4.0f, 0xFFDC2626, alpha);
        } else if (k.find("pickaxe") != std::string::npos) {
          uint32_t hdCol = (k.find("diamond") != std::string::npos) ? 0xFF38BDF8 : 0xFF926D43;
          glColor4f(0.55f, 0.35f, 0.15f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP + 6.0f);
          glVertex2f(cxP + 5.0f, cyP - 5.0f);
          glEnd();
          glColor4f(((hdCol>>16)&0xFF)/255.f, ((hdCol>>8)&0xFF)/255.f, (hdCol&0xFF)/255.f, alpha);
          glLineWidth(3.0f);
          glBegin(GL_LINE_STRIP);
          glVertex2f(cxP - 3.0f, cyP - 8.0f);
          glVertex2f(cxP + 5.0f, cyP - 5.0f);
          glVertex2f(cxP + 8.0f, cyP + 3.0f);
          glEnd();
        } else if (k.find("axe") != std::string::npos) {
          uint32_t hdCol = (k.find("diamond") != std::string::npos) ? 0xFF38BDF8 : 0xFF926D43;
          glColor4f(0.55f, 0.35f, 0.15f, alpha);
          glLineWidth(2.5f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP + 6.0f);
          glVertex2f(cxP + 5.0f, cyP - 5.0f);
          glEnd();
          RenderUtils::drawRoundedRect(cxP + 1.0f, cyP - 8.0f, 7.0f, 8.0f, 2.0f, hdCol, alpha);
        } else if (k == "golden_apple" || k == "apple_golden") {
          RenderUtils::drawCircle(cxP, cyP + 0.5f, 7.5f, 0xFFFBBF24, alpha);
          RenderUtils::drawCircle(cxP + 2.0f, cyP + 0.5f, 5.5f, 0xFFF59E0B, alpha);
          RenderUtils::drawRect(cxP - 1.0f, cyP - 7.5f, 2.0f, 3.5f, 0xFF78350F, alpha);
        } else if (k == "fireball") {
          RenderUtils::drawCircle(cxP, cyP, 7.5f, 0xFFEA580C, alpha);
          RenderUtils::drawCircle(cxP, cyP, 5.0f, 0xFFFBBF24, alpha);
          RenderUtils::drawCircle(cxP, cyP, 2.0f, 0xFFFEF08A, alpha);
        } else if (k == "water_bucket" || k == "bucket_water") {
          RenderUtils::drawRoundedRect(cxP - 6.0f, cyP - 5.5f, 12.0f, 11.0f, 2.5f, 0xFFCBD5E1, alpha);
          RenderUtils::drawRect(cxP - 4.5f, cyP - 4.0f, 9.0f, 4.0f, 0xFF2563EB, alpha);
        } else if (k == "magic_milk" || k == "milk" || k == "milk_bucket" || k == "bucket_milk") {
          RenderUtils::drawRoundedRect(cxP - 6.0f, cyP - 5.5f, 12.0f, 11.0f, 2.5f, 0xFFCBD5E1, alpha);
          RenderUtils::drawRect(cxP - 4.5f, cyP - 4.0f, 9.0f, 4.0f, 0xFFFFFFFF, alpha);
        } else if (k == "ender_pearl") {
          RenderUtils::drawCircle(cxP, cyP, 7.5f, 0xFF065F46, alpha);
          RenderUtils::drawCircle(cxP, cyP, 5.5f, 0xFF0D9488, alpha);
          RenderUtils::drawCircle(cxP - 2.0f, cyP - 2.0f, 2.0f, 0xFF5EEAD4, alpha);
        } else if (k == "bridge_egg" || k == "egg") {
          RenderUtils::drawCircle(cxP, cyP + 1.0f, 6.5f, 0xFFF1F5F9, alpha);
          RenderUtils::drawCircle(cxP, cyP - 2.0f, 5.0f, 0xFFE2E8F0, alpha);
          if (k == "bridge_egg") {
            RenderUtils::drawCircle(cxP, cyP, 2.5f, 0xFF38BDF8, alpha);
          }
        } else if (k == "compact_pop_up_tower" || k == "popup_tower" || k.find("tower") != std::string::npos || k.find("popup") != std::string::npos) {
          RenderUtils::drawRoundedRect(cxP - 8.0f, cyP - 7.0f, 16.0f, 14.0f, 2.0f, 0xFF785128, alpha);
          RenderUtils::drawOutline(cxP - 8.0f, cyP - 7.0f, 16.0f, 14.0f, 1.0f, 0xFF4A3219, alpha);
        } else if (k == "bedbug" || k == "silverfish" || k == "spawn_egg") {
          RenderUtils::drawCircle(cxP, cyP, 6.5f, 0xFF94A3B8, alpha);
          RenderUtils::drawCircle(cxP - 2.0f, cyP - 2.0f, 1.5f, 0xFFEF4444, alpha);
          RenderUtils::drawCircle(cxP + 2.0f, cyP - 2.0f, 1.5f, 0xFFEF4444, alpha);
        } else if (k == "dream_defender" || k == "iron_golem") {
          RenderUtils::drawRoundedRect(cxP - 7.0f, cyP - 8.0f, 14.0f, 16.0f, 2.0f, 0xFFE2E8F0, alpha);
          RenderUtils::drawRect(cxP - 1.5f, cyP - 3.0f, 3.0f, 6.0f, 0xFF94A3B8, alpha);
          RenderUtils::drawRect(cxP - 4.0f, cyP - 4.0f, 2.0f, 2.0f, 0xFFEF4444, alpha);
          RenderUtils::drawRect(cxP + 2.0f, cyP - 4.0f, 2.0f, 2.0f, 0xFFEF4444, alpha);
        } else if (k == "compass") {
          RenderUtils::drawCircle(cxP, cyP, 7.5f, 0xFF475569, alpha);
          RenderUtils::drawCircle(cxP, cyP, 5.5f, 0xFF0F172A, alpha);
          glColor4f(0.9f, 0.2f, 0.2f, alpha);
          glLineWidth(2.0f);
          glBegin(GL_LINES);
          glVertex2f(cxP, cyP); glVertex2f(cxP, cyP - 5.0f);
          glEnd();
        } else {
          RenderUtils::drawRoundedRect(cxP - 7.0f, cyP - 7.0f, 14.0f, 14.0f, 2.0f, 0x446B7280, alpha);
          RenderUtils::drawOutline(cxP - 7.0f, cyP - 7.0f, 14.0f, 14.0f, 1.0f, 0x889CA3AF, alpha);
        }
        glEnable(GL_TEXTURE_2D);
      };

      for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
          float sx = gridStartX + c * (slotSize + slotGap);
          float sy = gridStartY + r * (slotSize + slotGap);
          bool hSlot = isHovered(mx, my, sx, sy, slotSize, slotSize);

          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRect(sx, sy, slotSize, slotSize, 0xFF181A1F, alpha);
          RenderUtils::drawRect(sx, sy, slotSize, 1.5f, 0xFF0B0D10, 0.9f * alpha);
          RenderUtils::drawRect(sx, sy, 1.5f, slotSize, 0xFF0B0D10, 0.9f * alpha);
          RenderUtils::drawRect(sx, sy + slotSize - 1.5f, slotSize, 1.5f, 0xFF373A42, 0.6f * alpha);
          RenderUtils::drawRect(sx + slotSize - 1.5f, sy, 1.5f, slotSize, 0xFF373A42, 0.6f * alpha);
          if (hSlot) {
            RenderUtils::drawRect(sx, sy, slotSize, slotSize, 0x33FFFFFF, alpha);
          }
          glEnable(GL_TEXTURE_2D);

          bool isBorder = (r == 0 || r == kRows - 1 || c == 0 || c == kCols - 1);
          if (isBorder) {
            drawSlotItem("glass_pane", sx, sy);
          } else {
            int itemIdx = (r - 1) * 7 + (c - 1);
            if (itemIdx >= 0 && itemIdx < 21) {
              const std::string &itemKey = qbSlots[itemIdx];
              if (!itemKey.empty() && itemKey != "null" && itemKey != "glass_pane") {
                drawSlotItem(itemKey, sx, sy);
                if (hSlot) hoveredSlotName = getItemDisplayName(itemKey);
              }
            }
          }
        }
      }

      float hotbarLabelY = chestY + containerH + 12.0f;
      g_guiFont.drawString(chestX, hotbarLabelY, "Hotbar", applyAlpha(0xFFFFFFFF, alpha), 0.42f);

      std::vector<std::string> hotbarSlots(9, "null");
      hotbarSlots[0] = "Melee";
      if (!s_lookupResult.favoriteSlots.empty()) {
        std::stringstream hss(s_lookupResult.favoriteSlots);
        std::string hSlotItem;
        int hIdx = 0;
        while (std::getline(hss, hSlotItem, ',') && hIdx < 9) {
          if (!hSlotItem.empty()) {
            hotbarSlots[hIdx] = hSlotItem;
          }
          hIdx++;
        }
      }

      float hotbarY = hotbarLabelY + 18.0f;
      for (int i = 0; i < 9; ++i) {
        float sx = chestX + i * (slotSize + slotGap);
        float sy = hotbarY;
        bool hSlot = isHovered(mx, my, sx, sy, slotSize, slotSize);

        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRect(sx, sy, slotSize, slotSize, 0xFF181A1F, alpha);
        RenderUtils::drawRect(sx, sy, slotSize, 1.5f, 0xFF0B0D10, 0.9f * alpha);
        RenderUtils::drawRect(sx, sy, 1.5f, slotSize, 0xFF0B0D10, 0.9f * alpha);
        RenderUtils::drawRect(sx, sy + slotSize - 1.5f, slotSize, 1.5f, 0xFF373A42, 0.6f * alpha);
        RenderUtils::drawRect(sx + slotSize - 1.5f, sy, 1.5f, slotSize, 0xFF373A42, 0.6f * alpha);
        if (hSlot) {
          RenderUtils::drawRect(sx, sy, slotSize, slotSize, 0x33FFFFFF, alpha);
        }
        glEnable(GL_TEXTURE_2D);

        const std::string &cat = hotbarSlots[i];
        if (cat == "null" || cat == "none" || cat.empty()) {
          float cxP = sx + slotSize * 0.5f;
          float cyP = sy + slotSize * 0.5f;
          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawCircle(cxP, cyP, 8.5f, 0xFFDC2626, alpha);
          RenderUtils::drawCircle(cxP, cyP, 5.5f, 0xFF181A1F, alpha);
          glColor4f(0.86f, 0.15f, 0.15f, alpha);
          glLineWidth(2.8f);
          glBegin(GL_LINES);
          glVertex2f(cxP - 6.0f, cyP + 6.0f);
          glVertex2f(cxP + 6.0f, cyP - 6.0f);
          glEnd();
          glEnable(GL_TEXTURE_2D);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Unassigned";
        } else if (cat == "Melee" || cat == "melee" || cat == "sword") {
          drawSlotItem("gold_sword", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Melee (Sword)";
        } else if (cat == "Blocks" || cat == "blocks") {
          drawSlotItem("wool", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Blocks";
        } else if (cat == "Tools_1" || cat == "tools_1" || cat == "Tools" || cat == "tools") {
          drawSlotItem("diamond_pickaxe", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Tools (Pickaxe)";
        } else if (cat == "Tools_2" || cat == "tools_2") {
          drawSlotItem("diamond_axe", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Tools (Axe)";
        } else if (cat == "Tools_3" || cat == "tools_3") {
          drawSlotItem("shears", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Tools (Shears)";
        } else if (cat == "Ranged" || cat == "ranged" || cat == "bow") {
          drawSlotItem("bow", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Ranged (Bow)";
        } else if (cat == "Potions" || cat == "potions") {
          drawSlotItem("speed_potion", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Potions";
        } else if (cat == "Utility" || cat == "utility") {
          drawSlotItem("golden_apple", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Utility";
        } else if (cat == "Compass" || cat == "compass") {
          drawSlotItem("compass", sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": Compass";
        } else {
          drawSlotItem(cat, sx, sy);
          if (hSlot) hoveredSlotName = "Slot " + std::to_string(i + 1) + ": " + getItemDisplayName(cat);
        }
      }

      if (!hoveredSlotName.empty()) {
        float ttW = g_guiFont.getStringWidth(hoveredSlotName) * (0.36f / 0.5f) + 16.0f;
        float ttH = 20.0f;
        float ttX = mx + 12.0f;
        float ttY = my - 12.0f;
        if (ttX + ttW > cardX + cardW - 10.0f) ttX = mx - ttW - 6.0f;

        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(ttX, ttY, ttW, ttH, 4.0f, 0xF20F1217, alpha);
        RenderUtils::drawRoundedOutline(ttX, ttY, ttW, ttH, 4.0f, 1.0f, ClickGUITheme::accent(), 0.7f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(ttX + 8.0f, ttY + 4.0f, hoveredSlotName, applyAlpha(0xFFFFFFFF, alpha), 0.36f);
      }

      cy += qbCardH + 12.0f;
    }

    if (Config::isTagsEnabled()) {
      std::string activeS = Config::getActiveTagService();

      bool servesUrchin = (activeS == "Urchin" || activeS == "Both" ||
                           activeS == "Khadow");
      bool servesSeraph = (activeS == "Seraph" || activeS == "Both" ||
                           activeS == "Khadow");
      bool hasUrchin = s_lookupUrchinTags &&
                       !s_lookupUrchinTags->tags.empty() && servesUrchin;
      bool hasSeraph = s_lookupSeraphTags &&
                       !s_lookupSeraphTags->tags.empty() && servesSeraph;

      struct CleanTagInfo {
        std::string badge;
        std::string reason;
        std::string replayCmd;
      };

      auto copyStringToClipboard = [](const std::string &s) {
        if (OpenClipboard(nullptr)) {
          EmptyClipboard();
          HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
          if (hGlob) {
            void *ptr = GlobalLock(hGlob);
            if (ptr) {
              memcpy(ptr, s.c_str(), s.size() + 1);
              GlobalUnlock(hGlob);
              SetClipboardData(CF_TEXT, hGlob);
            }
          }
          CloseClipboard();
        }
      };

      auto parseTag = [](const std::string &rawType, const std::string &rawReason) -> CleanTagInfo {
        CleanTagInfo info;

        std::string b = rawType;
        for (char &c : b) {
          if (c == '_' || c == '-') c = ' ';
          else c = (char)toupper((unsigned char)c);
        }
        while (!b.empty() && b.front() == ' ') b.erase(0, 1);
        while (!b.empty() && b.back() == ' ') b.pop_back();
        info.badge = b.empty() ? "FLAGGED" : b;

        std::string r = rawReason;
        size_t pReplay = r.find("/replay");
        if (pReplay == std::string::npos) pReplay = r.find("replay:");
        if (pReplay != std::string::npos) {
          size_t cmdStart = r.find("/replay", pReplay);
          if (cmdStart == std::string::npos) cmdStart = pReplay;
          size_t cmdEnd = r.find_first_of(")\r\n", cmdStart);
          std::string cmd = (cmdEnd != std::string::npos) ? r.substr(cmdStart, cmdEnd - cmdStart) : r.substr(cmdStart);
          while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == ')' || cmd.back() == ']')) cmd.pop_back();
          if (cmd.rfind("/replay", 0) != 0 && cmd.find("/replay") != std::string::npos) {
            cmd = cmd.substr(cmd.find("/replay"));
          }
          info.replayCmd = cmd;

          size_t parenOpen = r.rfind('(', pReplay);
          if (parenOpen != std::string::npos && parenOpen < pReplay) {
            size_t parenClose = r.find(')', pReplay);
            if (parenClose != std::string::npos) {
              r.erase(parenOpen, (parenClose - parenOpen) + 1);
            } else {
              r.erase(parenOpen);
            }
          } else {
            r.erase(pReplay, (cmdEnd != std::string::npos ? (cmdEnd - pReplay + 1) : std::string::npos));
          }
        }

        std::string lowerR = r;
        for (char &c : lowerR) c = (char)tolower((unsigned char)c);
        std::string lowerType = rawType;
        for (char &c : lowerType) c = (char)tolower((unsigned char)c);

        if (!lowerType.empty() && lowerR.rfind(lowerType, 0) == 0) {
          r = r.substr(rawType.length());
        }
        while (!r.empty() && (r.front() == ' ' || r.front() == ':' || r.front() == '-' || r.front() == '\t')) {
          r.erase(0, 1);
        }
        while (!r.empty() && (r.back() == ' ' || r.back() == '\t')) {
          r.pop_back();
        }

        if (!r.empty() && islower((unsigned char)r[0])) {
          r[0] = (char)toupper((unsigned char)r[0]);
        }
        if (r.empty()) {
          r = info.replayCmd.empty() ? "Tagged in anticheat database" : "Reported replay logged";
        }
        info.reason = r;
        return info;
      };

      auto wrapTextLines = [&](const std::string &text, float maxW, float fontScale) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string line;
        std::string word;
        std::stringstream ss(text);
        while (ss >> word) {
          std::string test = line.empty() ? word : (line + " " + word);
          float tw = g_guiFont.getStringWidth(test.c_str()) * (fontScale / 0.5f);
          if (tw > maxW && !line.empty()) {
            lines.push_back(line);
            line = word;
          } else {
            line = test;
          }
        }
        if (!line.empty()) {
          lines.push_back(line);
        }
        return lines;
      };

      auto renderMinimalTagCard = [&](const std::string &serviceTitle,
                                      uint32_t titleCol,
                                      uint32_t tagCol,
                                      const std::vector<CleanTagInfo> &tagList) {
        if (tagList.empty()) return;

        float innerY = 34.0f;
        struct TagLayout {
          float y = 0;
          std::string prefix;
          float prefixW = 0;
          float sepW = 0;
          std::vector<std::string> reasonLines;
          bool hasReplay = false;
          float replayY = 0;
          float replayW = 0;
          float rowH = 0;
        };

        std::vector<TagLayout> layouts(tagList.size());
        for (size_t i = 0; i < tagList.size(); ++i) {
          const auto &t = tagList[i];
          TagLayout &tl = layouts[i];
          tl.y = innerY;

          tl.prefix = "[" + t.badge + "]";
          tl.prefixW = g_guiFont.getStringWidth(tl.prefix.c_str()) * (0.42f / 0.5f);
          tl.sepW = g_guiFont.getStringWidth(" - ") * (0.42f / 0.5f);

          float firstLineAvail = cardW - 32.0f - tl.prefixW - tl.sepW;
          if (firstLineAvail < 80.0f) {
            tl.reasonLines = wrapTextLines(t.reason, cardW - 32.0f, 0.42f);
          } else {
            tl.reasonLines = wrapTextLines(t.reason, firstLineAvail, 0.42f);
          }

          float lineCount = (float)(tl.reasonLines.empty() ? 1 : tl.reasonLines.size());
          float textH = lineCount * 17.0f;

          if (!t.replayCmd.empty()) {
            tl.hasReplay = true;
            tl.replayY = innerY + textH + 4.0f;
            std::string dispCmd = "Replay: " + t.replayCmd;
            tl.replayW = g_guiFont.getStringWidth(dispCmd.c_str()) * (0.37f / 0.5f);
            tl.rowH = textH + 4.0f + 18.0f;
          } else {
            tl.hasReplay = false;
            tl.rowH = textH;
          }

          innerY += tl.rowH + ((i + 1 < tagList.size()) ? 10.0f : 12.0f);
        }

        float totalCardH = innerY;

        glDisable(GL_TEXTURE_2D);
        drawThemeCard(cardX, cy, cardW, totalCardH, false, alpha);
        RenderUtils::drawRoundedRect(cardX + 4.0f, cy + 9.0f, 3.5f, 15.0f, 1.5f, titleCol, 0.95f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(cardX + 14.0f, cy + 9.0f, serviceTitle.c_str(),
                             applyAlpha(titleCol, alpha), 0.46f);

        for (size_t i = 0; i < tagList.size(); ++i) {
          const auto &t = tagList[i];
          const auto &tl = layouts[i];
          float tagBaseY = cy + tl.y;

          g_guiFont.drawString(cardX + 14.0f, tagBaseY, tl.prefix.c_str(),
                               applyAlpha(tagCol, alpha), 0.42f);

          g_guiFont.drawString(cardX + 14.0f + tl.prefixW, tagBaseY, " - ",
                               applyAlpha(0xFF888892, alpha), 0.42f);

          float textX = cardX + 14.0f + tl.prefixW + tl.sepW;
          float curY = tagBaseY;
          for (size_t li = 0; li < tl.reasonLines.size(); ++li) {
            float lx = (li == 0) ? textX : (cardX + 20.0f);
            g_guiFont.drawString(lx, curY, tl.reasonLines[li].c_str(),
                                 applyAlpha(0xFFE2E8F0, alpha), 0.42f);
            curY += 17.0f;
          }

          if (tl.hasReplay) {
            float repY = cy + tl.replayY;
            float repX = cardX + 20.0f;
            std::string dispCmd = "Replay: " + t.replayCmd;
            float baseW = g_guiFont.getStringWidth(dispCmd.c_str()) * (0.37f / 0.5f);
            bool hReplay = isHovered(mx, my, repX, repY - 1.0f, baseW + 60.0f, 16.0f);

            if (hReplay) {
              dispCmd += " (copy)";
              float fullW = g_guiFont.getStringWidth(dispCmd.c_str()) * (0.37f / 0.5f);
              g_guiFont.drawString(repX, repY, dispCmd.c_str(),
                                   applyAlpha(0xFF38BDF8, alpha), 0.37f);
              glDisable(GL_TEXTURE_2D);
              RenderUtils::drawRect(repX, repY + 13.0f, fullW, 1.0f, 0xFF38BDF8, alpha * 0.6f);
              glEnable(GL_TEXTURE_2D);
            } else {
              g_guiFont.drawString(repX, repY, dispCmd.c_str(),
                                   applyAlpha(0xFF7DD3FC, alpha), 0.37f);
            }

            if (clickEvent && hReplay) {
              copyStringToClipboard(t.replayCmd);
              NotificationManager::getInstance()->add(
                  serviceTitle, "Copied replay command to clipboard!", NotificationType::Success);
            }
          }
        }

        cy += totalCardH + 10.0f;
      };

      if (hasUrchin) {
        std::vector<CleanTagInfo> parsedTags;
        for (const auto &t : s_lookupUrchinTags->tags) {
          if (!t.type.empty()) {
            parsedTags.push_back(parseTag(t.type, t.reason));
          }
        }
        if (!parsedTags.empty()) {
          renderMinimalTagCard("Urchin", 0xFF00E5FF, 0xFFFBBF24, parsedTags);
        }
      }

      if (hasSeraph) {
        std::vector<CleanTagInfo> parsedTags;
        for (const auto &t : s_lookupSeraphTags->tags) {
          if (!t.type.empty()) {
            parsedTags.push_back(parseTag(t.type, t.reason));
          }
        }
        if (!parsedTags.empty()) {
          renderMinimalTagCard("Seraph", 0xFFFF3344, 0xFFFF5555, parsedTags);
        }
      }
    }
  }
  cy += 20;
}

} // namespace Tabs
} // namespace Render
