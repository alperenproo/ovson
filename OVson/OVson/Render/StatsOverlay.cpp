#include "StatsOverlay.h"
#include "../Logic/StatsTracker.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Java.h"
#include "../Services/Hypixel.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/GlGuard.h"
#include "../Utils/Logger.h"
#include "FontRenderer.h"
#include "RenderUtils.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <gl/GL.h>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::ofstream g_overlayDebugLog;

static void writeOverlayLog(const char *msg) {
  if (!g_overlayDebugLog.is_open()) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\/");
    std::string dir = exePath.substr(0, lastSlash);
    std::string logPath = dir + "\\ovson_overlay_debug.txt";
    g_overlayDebugLog.open(logPath, std::ios::app);
  }
  if (g_overlayDebugLog.is_open()) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeStr[64];
    sprintf_s(timeStr, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    g_overlayDebugLog << timeStr << msg << std::endl;
    g_overlayDebugLog.flush();
  }
}

bool StatsOverlay::s_initialized = false;
bool StatsOverlay::s_enabled = false;
bool StatsOverlay::s_lastInsertState = false;

GLuint StatsOverlay::s_displayList = 0;
bool StatsOverlay::s_dirty = false;
int StatsOverlay::s_lastStatsCount = 0;

static FontRenderer g_font;

static bool s_rendering = false;

void StatsOverlay::init() {
  if (s_initialized)
    return;

  s_initialized = true;
  s_enabled = false;
  writeOverlayLog("StatsOverlay initialized");
  Logger::info("StatsOverlay initialized");
}

void StatsOverlay::shutdown() { s_initialized = false; }

static uint32_t colorFromRGB(int r, int g, int b, int a = 255) {
  return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) |
         (b & 0xFF);
}

// will change these colors too when i got time



static uint32_t colorForTeam(const std::string &team) {
  if (team == "Red")
    return colorFromRGB(255, 85, 85);
  if (team == "Blue")
    return colorFromRGB(85, 85, 255);
  if (team == "Green")
    return colorFromRGB(85, 255, 85);
  if (team == "Yellow")
    return colorFromRGB(255, 255, 85);
  if (team == "Aqua")
    return colorFromRGB(85, 255, 255);
  if (team == "White")
    return colorFromRGB(255, 255, 255);
  if (team == "Pink")
    return colorFromRGB(255, 85, 255);
  if (team == "Gray" || team == "Grey")
    return colorFromRGB(85, 85, 85);
  return colorFromRGB(255, 255, 255);
}

static float s_panelX = -1.0f;
static float s_panelY = 50.0f;
static bool s_isDragging = false;
static float s_dragOffsetX = 0.0f;
static float s_dragOffsetY = 0.0f;

static uint32_t hsbToRgb(float h, float s, float b) {
  float r = 0, g = 0, bl = 0;
  if (s == 0) {
    r = g = bl = b;
  } else {
    float h_val = (h - floor(h)) * 6.0f;
    float f = h_val - floor(h_val);
    float p = b * (1.0f - s);
    float q = b * (1.0f - s * f);
    float t = b * (1.0f - s * (1.0f - f));
    switch ((int)h_val) {
    case 0:
      r = b;
      g = t;
      bl = p;
      break;
    case 1:
      r = q;
      g = b;
      bl = p;
      break;
    case 2:
      r = p;
      g = b;
      bl = t;
      break;
    case 3:
      r = p;
      g = q;
      bl = b;
      break;
    case 4:
      r = t;
      g = p;
      bl = b;
      break;
    case 5:
      r = b;
      g = p;
      bl = q;
      break;
    }
  }
  return colorFromRGB((int)(r * 255), (int)(g * 255), (int)(bl * 255));
}

void StatsOverlay::render(void *hdcPtr) {
  if (s_rendering) return;
  s_rendering = true;
  HDC hdc = (HDC)hdcPtr;
  static bool s_loggedOnce = false;
  if (!s_loggedOnce) {
    writeOverlayLog("StatsOverlay::render() called for first time");
    s_loggedOnce = true;
  }

  if (!g_font.isInitialized()) {
    if (hdc) {
      g_font.init(hdc);
    }
  }

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  JNIEnv *env = lc->getEnv();
  if (!env || !mcCls) {
    s_rendering = false;
    return;
  }

  jfieldID f_mc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                       "Lnet/minecraft/client/Minecraft;",
                                       "field_71432_P", "S", "Lave;");
  if (!f_mc) {
    s_rendering = false;
    return;
  }

  jobject mc = env->GetStaticObjectField(mcCls, f_mc);
  if (!mc) {
    s_rendering = false;
    return;
  }

  jfieldID f_screen = lc->GetFieldID(mcCls, "currentScreen",
                                     "Lnet/minecraft/client/gui/GuiScreen;",
                                     "field_71462_r", "m", "Laxu;");
  if (!f_screen)
    f_screen =
        lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiScreen;");
  if (!f_screen)
    f_screen = lc->FindFieldBySignature(mcCls, "Laxu;");

  jobject screen = f_screen ? env->GetObjectField(mc, f_screen) : nullptr;

  if (screen) {
    jclass chatCls = lc->GetClass("net.minecraft.client.gui.GuiChat");
    if (chatCls && env->IsInstanceOf(screen, chatCls)) {
      jfieldID f_inputField = lc->GetFieldID(
          chatCls, "inputField", "Lnet/minecraft/client/gui/GuiTextField;",
          "field_146415_a", "a", "Lavw;");
      if (!f_inputField)
        f_inputField = lc->FindFieldBySignature(
            chatCls, "Lnet/minecraft/client/gui/GuiTextField;");
      if (!f_inputField)
        f_inputField = lc->FindFieldBySignature(chatCls, "Lavw;");

      jobject inputField =
          f_inputField ? env->GetObjectField(screen, f_inputField) : nullptr;

      if (inputField) {
        jclass tfCls = env->GetObjectClass(inputField);

        jmethodID m_getText = lc->GetMethodID(
            tfCls, "getText", "()Ljava/lang/String;", "func_146179_b", "b");
        if (!m_getText)
          m_getText = lc->FindMethodBySignature(tfCls, "()Ljava/lang/String;");

        jstring textJ =
            m_getText ? (jstring)env->CallObjectMethod(inputField, m_getText)
                      : nullptr;
        if (env->ExceptionCheck()) {
          env->ExceptionClear();
          textJ = nullptr;
        }

        if (textJ) {
          const char *utf = env->GetStringUTFChars(textJ, 0);
          if (utf) {
            const std::string &cp = ::Config::getCommandPrefix();
            bool isDoubled =
                (strlen(utf) >= cp.length() * 2 &&
                 strncmp(utf + cp.length(), cp.c_str(), cp.length()) == 0);
            if (!cp.empty() && strncmp(utf, cp.c_str(), cp.length()) == 0 &&
                !isDoubled) {
              jfieldID f_x = lc->GetFieldID(tfCls, "xPosition", "I",
                                            "field_146209_f", "a");
              jfieldID f_y = lc->GetFieldID(tfCls, "yPosition", "I",
                                            "field_146210_g", "f");
              jfieldID f_w =
                  lc->GetFieldID(tfCls, "width", "I", "field_146218_h", "i");
              jfieldID f_h =
                  lc->GetFieldID(tfCls, "height", "I", "field_146219_i", "j");

              if (f_x && f_y && f_w && f_h) {
                float x = (float)env->GetIntField(inputField, f_x);
                float y = (float)env->GetIntField(inputField, f_y);
                float w = (float)env->GetIntField(inputField, f_w);
                float h = (float)env->GetIntField(inputField, f_h);

                jclass screenCls =
                    lc->GetClass("net.minecraft.client.gui.GuiScreen");
                jfieldID f_sw = screenCls
                                    ? lc->GetFieldID(screenCls, "width", "I",
                                                     "field_146294_l", "l")
                                    : nullptr;
                jfieldID f_sh = screenCls
                                    ? lc->GetFieldID(screenCls, "height", "I",
                                                     "field_146295_m", "m")
                                    : nullptr;

                float sw =
                    (f_sw) ? (float)env->GetIntField(screen, f_sw) : 427.0f;
                float sh =
                    (f_sh) ? (float)env->GetIntField(screen, f_sh) : 240.0f;

                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, sw, sh, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glPushMatrix();
                glLoadIdentity();
                glPushAttrib(GL_ALL_ATTRIB_BITS);
                glDisable(GL_TEXTURE_2D);
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_LINE_SMOOTH);

                float time = (float)(GetTickCount64() % 4000) / 4000.0f;

                for (int i = 0; i < 2; i++) {
                  glLineWidth(1.0f + (i * 1.0f));
                  glBegin(GL_LINE_LOOP);
                  for (int j = 0; j < 4; j++) {
                    float shift = (float)j / 4.0f + time;
                    uint32_t color = hsbToRgb(shift, 0.8f, 1.0f);
                    float r = ((color >> 16) & 0xFF) / 255.0f;
                    float g = ((color >> 8) & 0xFF) / 255.0f;
                    float b = (color & 0xFF) / 255.0f;
                    glColor4f(r, g, b, i == 0 ? 0.9f : 0.25f);

                    if (j == 0)
                      glVertex2f(x - 2.0f, y - 1.0f);
                    if (j == 1)
                      glVertex2f(x + w + 2.0f, y - 1.0f);
                    if (j == 2)
                      glVertex2f(x + w + 2.0f, y + h - 2.0f);
                    if (j == 3)
                      glVertex2f(x - 2.0f, y + h - 2.0f);
                  }
                  glEnd();
                }

                glPopAttrib();
                glMatrixMode(GL_MODELVIEW);
                glPopMatrix();
                glMatrixMode(GL_PROJECTION);
                glPopMatrix();
              }
            }
            env->ReleaseStringUTFChars(textJ, utf);
          }
          env->DeleteLocalRef(textJ);
        }
        env->DeleteLocalRef(inputField);
      }
    }
    env->DeleteLocalRef(screen);
  }
  env->DeleteLocalRef(mc);

  std::string mode = Config::getOverlayMode();
  if (mode != "gui") {
    s_rendering = false;
    return;
  }

  // MOVED TO CLICKGUI
  /*SHORT insertState = GetAsyncKeyState(VK_INSERT);
  bool isInsertDown = (insertState & 0x8000) != 0;
  if (!s_lastInsertState && isInsertDown) {
          s_enabled = !s_enabled;
  }
  s_lastInsertState = isInsertDown;*/

  if (!s_enabled) {
    s_rendering = false;
    return;
  }

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  float screenWidth = (float)viewport[2];
  float screenHeight = (float)viewport[3];

  if (screenWidth <= 0)
    screenWidth = 1920.0f;
  if (screenHeight <= 0)
    screenHeight = 1080.0f;

  GlGuard::GlMatrixGuard _gPr(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, screenWidth, screenHeight, 0, -1, 1);

  GlGuard::GlMatrixGuard _gMv(GL_MODELVIEW);
  glLoadIdentity();

  GlGuard::GlAttribGuard _gAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT |
                                    GL_CURRENT_BIT);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);

  std::vector<std::pair<std::string, Hypixel::PlayerStats>> statsData;
  {
    bool activeMatch = OVson::isInHypixelGame() &&
                       !OVson::isInPreGameLobby();
    std::lock_guard<std::mutex> lock(OVson::g_statsMutex);
    std::unordered_set<std::string> seen;
    for (const auto &pair : OVson::g_playerStatsMap) {
      if (activeMatch && pair.second.teamColor.empty())
        continue;
      
      std::string lower = pair.first;
      for (auto &c : lower) if (c >= 'A' && c <= 'Z') c += 32;
      if (seen.count(lower)) continue;
      seen.insert(lower);

      statsData.push_back(pair);
    }
  }

  struct ColumnDef {
    std::string title;
    float width;
    bool enabled;
    float x;
  };
  std::vector<ColumnDef> columns = {
    {"PLAYER", 130.0f, true, 0.0f},
    {"TAGS", 160.0f, Config::isOvShowTags(), 0.0f},
    {"STAR", 70.0f, Config::isOvShowStar(), 0.0f},
    {"F. KILLS", 85.0f, Config::isOvShowFk(), 0.0f},
    {"FKDR", 90.0f, Config::isOvShowFkdr(), 0.0f},
    {"KILLS", 85.0f, Config::isOvShowKills(), 0.0f},
    {"KDR", 85.0f, Config::isOvShowKdr(), 0.0f},
    {"BEDS", 85.0f, Config::isOvShowBeds(), 0.0f},
    {"BLR", 85.0f, Config::isOvShowBlr(), 0.0f},
    {"WINS", 85.0f, Config::isOvShowWins(), 0.0f},
    {"WLR", 85.0f, Config::isOvShowWlr(), 0.0f},
    {"WS", 65.0f, Config::isOvShowWs(), 0.0f},
    {"PING", 70.0f, Config::isOvShowPing(), 0.0f}
  };

  float currentX_col = 12.0f;
  for (auto &c : columns) {
    if (c.enabled) {
      c.x = currentX_col;
      currentX_col += c.width;
    }
  }

  float panelWidth = currentX_col + 10.0f;
  float rowHeight = 28.0f;
  float headerHeight = 32.0f;
  float panelHeight = headerHeight + (statsData.size() * rowHeight) + 10.0f;

  if (!s_isDragging && s_panelX < 0) {
    s_panelX = (screenWidth - panelWidth) / 2.0f;
    s_panelY = screenHeight / 5.0f;
  }

  float panelX = s_panelX;
  float panelY = s_panelY;
  if (hdc) {
    HWND hwnd = WindowFromDC(hdc);
    POINT pt;
    if (hwnd && GetCursorPos(&pt)) {
      ScreenToClient(hwnd, &pt);
      float mouseX = (float)pt.x;
      float mouseY = (float)pt.y;
      bool isLButtonDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
      if (isLButtonDown) {
        if (!s_isDragging) {
          if (mouseX >= s_panelX && mouseX <= s_panelX + panelWidth &&
              mouseY >= s_panelY && mouseY <= s_panelY + headerHeight) {
            s_isDragging = true;
            s_dragOffsetX = mouseX - s_panelX;
            s_dragOffsetY = mouseY - s_panelY;
          }
        } else {
          s_panelX = mouseX - s_dragOffsetX;
          s_panelY = mouseY - s_dragOffsetY;

          if (s_panelX < 0)
            s_panelX = 0;
          if (s_panelY < 0)
            s_panelY = 0;
          if (s_panelX + panelWidth > screenWidth)
            s_panelX = screenWidth - panelWidth;
          if (s_panelY + panelHeight > screenHeight)
            s_panelY = screenHeight - panelHeight;
        }
      } else {
        s_isDragging = false;
      }
    }
  }

  std::string sortMode = Config::getSortMode();
  bool isDesc = Config::isTabSortDescending();

  std::sort(
      statsData.begin(), statsData.end(),
      [sortMode, isDesc](const auto &a, const auto &b) {
        const Hypixel::PlayerStats &sA = a.second;
        const Hypixel::PlayerStats &sB = b.second;

        auto compareNumeric = [isDesc](double valA, double valB) {
          if (isDesc)
            return valA > valB;
          return valA < valB;
        };

        if (sortMode == "Star")
          return compareNumeric((double)sA.bedwarsStar, (double)sB.bedwarsStar);
        if (sortMode == "FK")
          return compareNumeric((double)sA.bedwarsFinalKills,
                                (double)sB.bedwarsFinalKills);
        if (sortMode == "FKDR") {
          double fkdrA =
              (sA.bedwarsFinalDeaths == 0)
                  ? (double)sA.bedwarsFinalKills
                  : (double)sA.bedwarsFinalKills / sA.bedwarsFinalDeaths;
          double fkdrB =
              (sB.bedwarsFinalDeaths == 0)
                  ? (double)sB.bedwarsFinalKills
                  : (double)sB.bedwarsFinalKills / sB.bedwarsFinalDeaths;
          return compareNumeric(fkdrA, fkdrB);
        }
        if (sortMode == "Wins")
          return compareNumeric((double)sA.bedwarsWins, (double)sB.bedwarsWins);
        if (sortMode == "WLR") {
          double wlrA = (sA.bedwarsLosses == 0)
                            ? (double)sA.bedwarsWins
                            : (double)sA.bedwarsWins / sA.bedwarsLosses;
          double wlrB = (sB.bedwarsLosses == 0)
                            ? (double)sB.bedwarsWins
                            : (double)sB.bedwarsWins / sB.bedwarsLosses;
          return compareNumeric(wlrA, wlrB);
        }
        if (sortMode == "WS")
          return compareNumeric((double)sA.winstreak, (double)sB.winstreak);

        const std::string &teamA = sA.teamColor;
        const std::string &teamB = sB.teamColor;

        if (teamA.empty() && !teamB.empty())
          return false;
        if (!teamA.empty() && teamB.empty())
          return true;

        if (teamA != teamB) {
          if (isDesc)
            return teamA < teamB;
          return teamA > teamB;
        }

        return a.first < b.first;
      });

  panelX = s_panelX;
  panelY = s_panelY;

  int currentStatsSize = (int)statsData.size();
  if (s_displayList == 0) {
    s_displayList = glGenLists(1);
    s_dirty = true;
  }

  if (currentStatsSize != s_lastStatsCount) {
    s_dirty = true;
    s_lastStatsCount = currentStatsSize;
  }

  static ULONGLONG s_lastRebuildTime = 0;
  ULONGLONG now = GetTickCount64();
  if ((now - s_lastRebuildTime) > 500) {
    s_dirty = true;
  }

  if (s_dirty) {
    s_lastRebuildTime = now;
    glNewList(s_displayList, GL_COMPILE);

    float localX = 0.0f;
    float localY = 0.0f;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);

    RenderUtils::drawRoundedRect(localX, localY, panelWidth, panelHeight, 8.0f,
                                 0xAA0A0A0C);
    RenderUtils::drawRoundedOutline(localX, localY, panelWidth, panelHeight,
                                    8.0f, 1.5f, 0xFF252528);

    float sepY = localY + headerHeight;
    RenderUtils::drawRect(localX + 5, sepY, panelWidth - 10, 1, 0xFF2A2A2E);

    float headerTextY = localY + 9.0f;
    for (const auto &c : columns) {
      if (c.enabled) {
        g_font.drawString(localX + c.x, headerTextY, c.title, colorFromRGB(255, 255, 255));
      }
    }

    float currentY = localY + headerHeight + 5.0f;
    for (const auto &pair : statsData) {
      std::string name = pair.first;
      Hypixel::PlayerStats stats = pair.second;

      double fkdr =
          (stats.bedwarsFinalDeaths == 0)
              ? stats.bedwarsFinalKills
              : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
      double wlr = (stats.bedwarsLosses == 0)
                       ? stats.bedwarsWins
                       : (double)stats.bedwarsWins / stats.bedwarsLosses;

      std::stringstream fkdrSs;
      std::stringstream wlrSs;
      fkdrSs << std::fixed << std::setprecision(2) << fkdr;
      wlrSs << std::fixed << std::setprecision(2) << wlr;

      uint32_t nameColor = stats.teamColor.empty()
                               ? colorFromRGB(255, 255, 255)
                               : colorForTeam(stats.teamColor);

      auto fmtCommas = [](int val) -> std::string {
          std::string s = std::to_string(val);
          int n = (int)s.length() - 3;
          while (n > 0) { s.insert(n, ","); n -= 3; }
          return s;
      };

      for (const auto &c : columns) {
          if (!c.enabled) continue;
          float cx = localX + c.x;
          
          if (c.title == "PLAYER") {
              g_font.drawString(cx, currentY, name, nameColor);
          } else if (c.title == "TAGS") {
              float currentTagX = cx;
              std::string activeS = Config::getActiveTagService();

              auto getHUDAbbr = [](const std::string &raw) -> std::string {
                  std::string t = raw;
                  for (auto &cc : t) cc = toupper(cc);
                  if (t.find("BLATANT") != std::string::npos) return "BC";
                  if (t.find("CLOSET") != std::string::npos) return "CC";
                  if (t.find("CONFIRMED") != std::string::npos) return "C";
                  if (t.find("CHEATER") != std::string::npos) return "C";
                  if (t.find("SNIPER") != std::string::npos) return "S";
                  return "";
              };

              auto drawAbbr = [&](const std::string &rawType, uint32_t color, bool isUrchin) {
                  std::string abbr = getHUDAbbr(rawType);
                  if (abbr.empty()) abbr = isUrchin ? "U" : "S";
                  std::string braced = "[" + abbr + "]";
                  g_font.drawString(currentTagX + 1.0f, currentY, braced, color & 0x40FFFFFF);
                  g_font.drawString(currentTagX, currentY, braced, color);
                  currentTagX += g_font.getStringWidth(braced) + 5.0f;
              };

              auto drawTag = [&](const std::string &rawType, uint32_t baseColor) {
                  std::string type = rawType;
                  std::replace(type.begin(), type.end(), '_', ' ');
                  for (auto &cc : type) cc = toupper(cc);
                  g_font.drawString(currentTagX + 1.0f, currentY, type, baseColor & 0x40FFFFFF);
                  g_font.drawString(currentTagX, currentY, type, baseColor);
                  currentTagX += g_font.getStringWidth(type) + 12.0f;
              };

              std::vector<std::string> urchinTags, seraphTags;
              for (const auto &tag : stats.rawTags) {
                  if (tag.find("URCHIN:") == 0 && (activeS == "Urchin" || activeS == "Both")) urchinTags.push_back(tag.substr(7));
                  else if (tag.find("SERAPH:") == 0 && (activeS == "Seraph" || activeS == "Both")) seraphTags.push_back(tag.substr(7));
              }

              bool hasU = !urchinTags.empty();
              bool hasS = !seraphTags.empty();

              if (hasU && hasS) {
                  for (const auto &type : urchinTags) drawAbbr(type, colorFromRGB(220, 20, 60), true);
                  for (const auto &type : seraphTags) drawAbbr(type, colorFromRGB(255, 85, 85), false);
              } else {
                  if (hasU) {
                      for (const auto &type : urchinTags) {
                          uint32_t color = colorFromRGB(255, 255, 255);
                          std::string t = type; for (auto &cc : t) cc = toupper(cc);
                          if (t.find("BLATANT") != std::string::npos) color = colorFromRGB(220, 20, 60);
                          else if (t.find("CONFIRMED") != std::string::npos) color = colorFromRGB(148, 0, 211);
                          drawTag(type, color);
                      }
                  }
                  if (hasS) {
                      for (const auto &type : seraphTags) {
                          uint32_t color = colorFromRGB(255, 85, 85);
                          std::string t = type; for (auto &cc : t) cc = toupper(cc);
                          if (t.find("CONFIRMED") != std::string::npos) color = colorFromRGB(211, 0, 148);
                          drawTag(type, color);
                      }
                  }
              }
              if (!hasU && !hasS) g_font.drawString(currentTagX, currentY, "-", colorFromRGB(100, 100, 105));
          } else if (stats.isNicked && c.title != "PLAYER" && c.title != "PING") {
              if (c.title == "STAR") g_font.drawString(cx, currentY, "[NICKED]", colorFromRGB(170, 0, 0));
          } else {
              if (c.title == "STAR") g_font.drawString(cx, currentY, std::to_string(stats.bedwarsStar), StatColors::getColor(StatColors::StatType::Star, stats.bedwarsStar));
              else if (c.title == "F. KILLS") g_font.drawString(cx, currentY, fmtCommas(stats.bedwarsFinalKills), StatColors::getColor(StatColors::StatType::FinalKills, stats.bedwarsFinalKills));
              else if (c.title == "FKDR") {
                  std::stringstream ss; ss << std::fixed << std::setprecision(2) << fkdr;
                  g_font.drawString(cx, currentY, ss.str(), StatColors::getColor(StatColors::StatType::FKDR, fkdr));
              }
              else if (c.title == "KILLS") g_font.drawString(cx, currentY, fmtCommas(stats.bedwarsKills), StatColors::getColor(StatColors::StatType::Kills, stats.bedwarsKills));
              else if (c.title == "KDR") {
                  double kd = stats.bedwarsDeaths == 0 ? stats.bedwarsKills : (double)stats.bedwarsKills / stats.bedwarsDeaths;
                  std::stringstream ss; ss << std::fixed << std::setprecision(2) << kd;
                  g_font.drawString(cx, currentY, ss.str(), StatColors::getColor(StatColors::StatType::KDR, kd));
              }
              else if (c.title == "BEDS") g_font.drawString(cx, currentY, fmtCommas(stats.bedwarsBedsBroken), StatColors::getColor(StatColors::StatType::Beds, stats.bedwarsBedsBroken));
              else if (c.title == "BLR") {
                  double blr = stats.bedwarsBedsLost == 0 ? stats.bedwarsBedsBroken : (double)stats.bedwarsBedsBroken / stats.bedwarsBedsLost;
                  std::stringstream ss; ss << std::fixed << std::setprecision(2) << blr;
                  g_font.drawString(cx, currentY, ss.str(), StatColors::getColor(StatColors::StatType::BLR, blr));
              }
              else if (c.title == "WINS") g_font.drawString(cx, currentY, fmtCommas(stats.bedwarsWins), StatColors::getColor(StatColors::StatType::Wins, stats.bedwarsWins));
              else if (c.title == "WLR") {
                  std::stringstream ss; ss << std::fixed << std::setprecision(2) << wlr;
                  g_font.drawString(cx, currentY, ss.str(), StatColors::getColor(StatColors::StatType::WLR, wlr));
              }
              else if (c.title == "WS") g_font.drawString(cx, currentY, std::to_string(stats.winstreak), StatColors::getColor(StatColors::StatType::WS, stats.winstreak));
              else if (c.title == "PING") {
                  int p = stats.lastPing;
                  int mode = Config::getPingDisplayMode();
                  if (mode == 1 && stats.auroraPing != -1) p = stats.auroraPing;
                  else if (mode == 2 && stats.auroraPingRecentAvg != -1) p = stats.auroraPingRecentAvg;

                  std::string pStr = (p < 0) ? "N/A" : std::to_string(p);
                  uint32_t pCol = (p < 0) ? colorFromRGB(170, 170, 170) : StatColors::getColor(StatColors::StatType::Ping, (double)p);
                  g_font.drawString(cx, currentY, pStr, pCol);
              }
          }
      }

      currentY += rowHeight;
    }

    glEndList();
    s_dirty = false;
  }

  glPushMatrix();
  glTranslatef(panelX, panelY, 0.0f);
  glCallList(s_displayList);
  glPopMatrix();

  // _gAttrib / _gMv / _gPr unwind here automatically

  s_rendering = false;
}
