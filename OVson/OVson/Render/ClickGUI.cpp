#include "ClickGUI.h"
#include "../Config/Config.h"
#include "../Config/StatColors.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Logic/StatsTracker.h"
#include "../Net/Http.h"
#include "../Services/AbyssService.h"
#include "../Services/Hypixel.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/NumberDenicker.h"
#include "../Utils/ReplaySpammer.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/SensitivityFix.h"
#include "../Utils/Timer.h"
#include "../Utils/stb_image.h"
#include "FontRenderer.h"
#include "NotificationManager.h"
#include "RenderUtils.h"
#include "StatsOverlay.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <gl/GL.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace Render {

static bool s_open = false;
static bool s_init = false;
static FontRenderer g_guiFont;
static float s_animAlpha = 0.0f;
static float s_targetAlpha = 0.0f;
static float s_openingScale = 0.95f;
static int s_activeTab = 0;
static int s_targetTab = 0;
static float s_tabIndicatorY = 80.0f;
static float s_contentSlide = 0.0f;
static float s_contentAlpha = 1.0f;
static bool s_lastLButton = false;
static bool s_lastInsert = false;
static bool s_lastBackspace = false;
static std::string s_playerSearch = "";
static std::string s_apiKeyInput = "";
static std::string s_autoGGInput = "";
static std::string s_urchinKeyInput = "";
static std::string s_seraphKeyInput = "";
static std::string s_auroraApiKeyInput = "";
static bool s_typingSearch = false;
static bool s_typingApiKey = false;
static bool s_typingAutoGG = false;
static bool s_typingUrchinKey = false;
static bool s_typingSeraphKey = false;
static bool s_typingAuroraApiKey = false;
static bool s_typingPrefix = false;
static std::string s_prefixInput = ".";
static float s_scrollOffset = 0.0f;
static float s_targetScroll = 0.0f;
static bool s_isDropdownOpen = false;
static float s_dropdownAnim = 0.0f;
static bool s_isTagsDropdownOpen = false;
static float s_tagsDropdownAnim = 0.0f;
static bool s_isSortOrderDropdownOpen = false;
static float s_sortOrderDropdownAnim = 0.0f;
static bool s_isPingModeDropdownOpen = false;
static float s_pingModeDropdownAnim = 0.0f;

// Colors tab state
static int s_colorSelectedStat = 0;
static bool s_colorPickerOpen = false;
static float s_cpHue = 0.0f;
static float s_cpSat = 1.0f;
static float s_cpVal = 1.0f;
static bool s_cpDraggingSV = false;
static bool s_cpDraggingHue = false;
static char s_cpMinBuf[16] = "0";
static char s_cpMaxBuf[16] = "100";
static int s_cpMinLen = 1;
static int s_cpMaxLen = 3;
static int s_cpEditingField = 0;  // 0=none, 1=min, 2=max
static int s_cpEditRangeIdx = -1; // -1 = adding new, >=0 = editing existing

static int s_columnTargetMode = 0; // 0 = Overlay, 1 = Better Tab
static bool s_isColumnTargetDropdownOpen = false;
static float s_columnTargetDropdownAnim = 0.0f;
static Hypixel::PlayerStats s_lookupResult;
static bool s_hasLookup = false;
static bool s_searching = false;
static std::string s_lookupName = "";

static std::optional<Urchin::PlayerTags> s_lookupUrchinTags;
static std::optional<Seraph::PlayerTags> s_lookupSeraphTags;
static std::atomic<bool> s_tagsFetched{false};

static GLuint s_lookupSkinTexId = 0;
static std::string s_lookupSkinUuid = "";
static std::atomic<bool> s_skinLoading{false};
static std::vector<uint8_t> s_skinPendingData;
static int s_skinPendingW = 0, s_skinPendingH = 0;
static std::atomic<bool> s_skinPendingReady{false};
static float g_x = 100.0f;
static float g_y = 100.0f;
static float g_w = 700.0f;
static float g_h = 420.0f;
static bool s_dragging = false;
static float s_dragOffsetX = 0.0f;
static float s_dragOffsetY = 0.0f;
static bool s_waitingForKey = false;

std::string ClickGUI::getKeyName(int vk) {
  if (vk == VK_INSERT)
    return "INSERT";
  if (vk == VK_DELETE)
    return "DELETE";
  if (vk == VK_HOME)
    return "HOME";
  if (vk == VK_END)
    return "END";
  if (vk == VK_PRIOR)
    return "PAGE UP";
  if (vk == VK_NEXT)
    return "PAGE DOWN";
  if (vk == VK_RSHIFT)
    return "RSHIFT";
  if (vk == VK_LSHIFT)
    return "LSHIFT";

  UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
  char buf[32] = {0};
  if (GetKeyNameTextA(scanCode << 16, buf, 32))
    return std::string(buf);
  return "Key " + std::to_string(vk);
}

#define THEME_NAVY Config::getThemeColor()
const DWORD THEME_BG = 0xFF0D0D0F;
const DWORD THEME_SIDEBAR = 0xFF121214;
const DWORD THEME_CARD = 0xFF18181B;
const DWORD THEME_ACCENT = THEME_NAVY;
const DWORD THEME_BORDER = 0xFF252528;

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static uint32_t applyAlpha(uint32_t color, float alpha) {
  uint8_t a = (uint8_t)(((color >> 24) & 0xFF) * alpha);
  return (uint32_t)((a << 24) | (color & 0x00FFFFFF));
}

// moved to RenderUtils.h

static void setMouseGrabbed(bool grabbed) {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  jclass mouseCls = lc->GetClass("org.lwjgl.input.Mouse");
  if (!mouseCls)
    return;
  jmethodID m_setGrabbed =
      env->GetStaticMethodID(mouseCls, "setGrabbed", "(Z)V");
  if (m_setGrabbed) {
    env->CallStaticVoidMethod(mouseCls, m_setGrabbed, grabbed);
  }
}

static bool isHovered(float mx, float my, float x, float y, float w, float h) {
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

static bool isIngame() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return false;
  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return false;
  }
  jobject mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj)
    return false;
  jfieldID f_screen = env->GetFieldID(mcCls, "currentScreen",
                                      "Lnet/minecraft/client/gui/GuiScreen;");
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = env->GetFieldID(mcCls, "field_71462_r",
                               "Lnet/minecraft/client/gui/GuiScreen;");
  }
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = env->GetFieldID(mcCls, "m", "Laxu;");
  }
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    return false;
  }
  jobject screen = env->GetObjectField(mcObj, f_screen);
  bool ingame = (screen == nullptr);
  if (screen)
    env->DeleteLocalRef(screen);
  env->DeleteLocalRef(mcObj);
  return ingame;
}

void ClickGUI::init() {
  s_init = true;
  TimeUtil::init();
}

void ClickGUI::shutdown() {}

bool ClickGUI::isOpen() { return s_open; }

void ClickGUI::toggle() {
  s_open = !s_open;
  s_targetAlpha = s_open ? 1.0f : 0.0f;

  if (s_open) {
    s_openingScale = 0.95f;
    s_apiKeyInput = Config::getApiKey();
    s_autoGGInput = Config::getAutoGGMessage();
    s_urchinKeyInput = Config::getUrchinApiKey();
    s_seraphKeyInput = Config::getSeraphApiKey();
    s_auroraApiKeyInput = Config::getAuroraApiKey();
    ShowCursor(TRUE);
    setMouseGrabbed(false);
    FocusFix::setIngameFocus(false);
  } else {
    if (isIngame()) {
      ShowCursor(FALSE);
      setMouseGrabbed(true);
      FocusFix::setIngameFocus(true);
    }
    s_dragging = false;
  }
}

void ClickGUI::updateInput(HWND hwnd) {
  int key = Config::getClickGuiKey();
  bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
  if (down && !s_lastInsert) {
    if (Config::isClickGuiOn()) {
      toggle();
    } else {
      if (Config::getOverlayMode() == "gui") {
        bool current = StatsOverlay::isEnabled();
        StatsOverlay::setEnabled(!current);
        NotificationManager::getInstance()->add(
            "Overlay",
            !current ? "Stats Overlay Enabled" : "Stats Overlay Disabled",
            NotificationType::Info);
      } else {
        NotificationManager::getInstance()->add(
            "Overlay", "Unlock 'GUI' mode to toggle overlay",
            NotificationType::Warning);
      }
    }
  }
  s_lastInsert = down;
}

struct SwitchAnim {
  float currX = 0.0f;
  float targetX = 0.0f;
};
static SwitchAnim s_switches[50];

static void drawSwitch(int id, float x, float y, bool enabled, bool hovered,
                       float alpha) {
  float w = 34.0f;
  float h = 18.0f;

  static std::unordered_map<int, float> anims;
  if (anims.find(id) == anims.end())
    anims[id] = enabled ? 1.0f : 0.0f;
  anims[id] += ((enabled ? 1.0f : 0.0f) - anims[id]) * 0.2f;
  float t = anims[id];

  // background pill (bp)
  DWORD bgBase = RenderUtils::lerpColor(0xFF2D2D31, THEME_NAVY, t);
  if (hovered)
    bgBase = RenderUtils::lerpColor(bgBase, 0xFFFFFFFF, 0.15f);

  RenderUtils::drawRoundedRect(x, y, w, h, h / 2.0f, bgBase, alpha);

  float knobX = x + 2.0f + t * (w - h);
  RenderUtils::drawCircle(knobX + (h - 4.0f) / 2.0f + 1.0f, y + h / 2.0f,
                          (h - 4.0f) / 2.0f, 0xFFFFFFFF, alpha);
}

void ClickGUI::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  if (!s_open)
    return;

  if (msg == WM_MOUSEWHEEL) {
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    s_targetScroll -= (float)delta * 0.5f;
    return;
  }

  if (msg == WM_KEYDOWN) {
    if (wParam == VK_ESCAPE) {
      toggle();
      return;
    }
    if ((wParam == 'V') && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
      if (s_typingSearch || s_typingApiKey || s_typingAutoGG ||
          s_typingUrchinKey || s_typingSeraphKey || s_typingAuroraApiKey) {
        if (OpenClipboard(NULL)) {
          HANDLE hData = GetClipboardData(CF_TEXT);
          if (hData) {
            char *pszText = static_cast<char *>(GlobalLock(hData));
            if (pszText) {
              std::string text(pszText);
              std::string filtered;
              for (char c : text)
                if (c >= 32 && c <= 126)
                  filtered += c;

              std::string *target =
                  s_typingSearch
                      ? &s_playerSearch
                      : (s_typingApiKey
                             ? &s_apiKeyInput
                             : (s_typingAutoGG
                                    ? &s_autoGGInput
                                    : (s_typingUrchinKey
                                           ? &s_urchinKeyInput
                                           : (s_typingSeraphKey
                                                  ? &s_seraphKeyInput
                                                  : (s_typingAuroraApiKey
                                                         ? &s_auroraApiKeyInput
                                                         : &s_prefixInput)))));
              int cap = (s_typingAutoGG || s_typingUrchinKey ||
                         s_typingSeraphKey || s_typingAuroraApiKey)
                            ? 100
                            : (s_typingPrefix ? 1 : 48);
              if (target->length() + filtered.length() < cap) {
                *target += filtered;
                NotificationManager::getInstance()->add(
                    "Input", "Pasted from clipboard", NotificationType::Info);
              } else {
                NotificationManager::getInstance()->add(
                    "Input", "Text too long!", NotificationType::Warning);
              }
              GlobalUnlock(hData);
            }
          }
          CloseClipboard();
        }
      }
      return;
    }
  }

  if (msg == WM_CHAR) {
    char c = (char)wParam;
    if (s_typingSearch || s_typingApiKey || s_typingAutoGG ||
        s_typingUrchinKey || s_typingSeraphKey || s_typingAuroraApiKey ||
        s_typingPrefix) {
      std::string *target =
          s_typingSearch
              ? &s_playerSearch
              : (s_typingApiKey
                     ? &s_apiKeyInput
                     : (s_typingAutoGG
                            ? &s_autoGGInput
                            : (s_typingSeraphKey ? &s_seraphKeyInput
                                                 : (s_typingAuroraApiKey
                                                        ? &s_auroraApiKeyInput
                                                        : &s_prefixInput))));
      int cap = (s_typingAutoGG || s_typingUrchinKey || s_typingSeraphKey ||
                 s_typingAuroraApiKey)
                    ? 100
                    : (s_typingPrefix ? 1 : 48);
      if (c == 8) {
        if (!target->empty())
          target->pop_back();
      } else if (c == 13) {
        if (s_typingSearch && !s_playerSearch.empty()) {
          std::string key = s_apiKeyInput;
          if (key.empty() || key == "None")
            key = Config::getApiKey();
          bool keyless = Config::isKeylessModeEnabled();

          if ((key.empty() || key == "None") && !keyless) {
            NotificationManager::getInstance()->add(
                "Hypixel", "Set an API Key or enable Keyless Mode!",
                NotificationType::Error);
          } else {
            s_searching = true;
            s_hasLookup = false;
            std::string searchName = s_playerSearch;
            NotificationManager::getInstance()->add(
                "Hypixel", "Fetching player ID...", NotificationType::Info);

            std::thread([searchName, key, keyless]() {
              SafeGuard::installSehTranslator();
              SafeGuard::run("ClickGUI::statsLookup", [&]() {
                auto uuidOpt = Hypixel::getUuidByName(searchName);
                if (uuidOpt) {
                  NotificationManager::getInstance()->add(
                      "Hypixel", "ID found, fetching stats...",
                      NotificationType::Info);
                  std::optional<Hypixel::PlayerStats> statsOpt;
                  if (keyless) {
                    statsOpt = AbyssService::getPlayerStats(*uuidOpt);
                  } else {
                    statsOpt = Hypixel::getPlayerStats(key, *uuidOpt);
                  }
                  if (statsOpt) {
                    s_lookupResult = *statsOpt;
                    s_lookupName = searchName;
                    s_lookupUrchinTags = std::nullopt;
                    s_lookupSeraphTags = std::nullopt;
                    s_tagsFetched = false;
                    s_hasLookup = true;

                    if (Config::isTagsEnabled()) {
                      std::string uuid = s_lookupResult.uuid;
                      std::thread([searchName, uuid]() {
                        SafeGuard::installSehTranslator();
                        SafeGuard::run("ClickGUI::tagFetch", [&]() {
                          std::string activeS = Config::getActiveTagService();
                          if (activeS == "Urchin" || activeS == "Both") {
                            auto ut = Urchin::getPlayerTags(searchName, true);
                            if (ut)
                              s_lookupUrchinTags = ut;
                          }
                          if (!uuid.empty() &&
                              (activeS == "Seraph" || activeS == "Both")) {
                            auto st =
                                Seraph::getPlayerTags(searchName, uuid, true);
                            if (st)
                              s_lookupSeraphTags = st;
                          }
                          s_tagsFetched = true;
                        });
                      }).detach();
                    }

                    std::string uuid = s_lookupResult.uuid;
                    if (!uuid.empty() && uuid != s_lookupSkinUuid) {
                      s_skinLoading = true;
                      s_skinPendingReady = false;
                      std::thread([searchName, uuid]() {
                        SafeGuard::installSehTranslator();
                        SafeGuard::run("ClickGUI::skinHead", [&]() {
                          std::string url = "https://api.mcheads.org/head/" +
                                            searchName + "/64";
                          std::string body;
                          if (Http::get(url, body) && body.size() > 100) {
                            int w, h, ch;
                            unsigned char *px = stbi_load_from_memory(
                                (const stbi_uc *)body.data(), (int)body.size(),
                                &w, &h, &ch, STBI_rgb_alpha);
                            if (px && w > 0 && h > 0) {
                              s_skinPendingData.assign(px, px + w * h * 4);
                              s_skinPendingW = w;
                              s_skinPendingH = h;
                              s_lookupSkinUuid = uuid;
                              s_skinPendingReady = true;
                            }
                            if (px)
                              stbi_image_free(px);
                          }
                          s_skinLoading = false;
                        });
                      }).detach();
                    }
                  } else {
                    NotificationManager::getInstance()->add(
                        "Hypixel",
                        keyless ? "Abyss API failed"
                                : "Check API-Key or Connectivity",
                        NotificationType::Error);
                  }
                } else {
                  NotificationManager::getInstance()->add(
                      "Hypixel", "Player not found", NotificationType::Warning);
                }
              });
              s_searching = false;
            }).detach();
          }
          s_typingSearch = false;
        }
        if (s_typingApiKey) {
          Config::setApiKey(s_apiKeyInput);
          NotificationManager::getInstance()->add("Settings", "API Key Saved",
                                                  NotificationType::Success);
          s_typingApiKey = false;
        }
        if (s_typingAutoGG) {
          Config::setAutoGGMessage(s_autoGGInput);
          NotificationManager::getInstance()->add(
              "AutoGG", "Custom message saved", NotificationType::Success);
          s_typingAutoGG = false;
        }
        if (s_typingUrchinKey) {
          Config::setUrchinApiKey(s_urchinKeyInput);
          NotificationManager::getInstance()->add("Urchin", "API Key Saved",
                                                  NotificationType::Success);
          s_typingUrchinKey = false;
        }
        if (s_typingSeraphKey) {
          Config::setSeraphApiKey(s_seraphKeyInput);
          NotificationManager::getInstance()->add("Seraph", "API Key Saved",
                                                  NotificationType::Success);
          s_typingSeraphKey = false;
        }
        if (s_typingAuroraApiKey) {
          Config::setAuroraApiKey(s_auroraApiKeyInput);
          Config::save();
          NotificationManager::getInstance()->add(
              "Settings", "Aurora Key Saved", NotificationType::Success);
          s_typingAuroraApiKey = false;
        }
        if (s_typingPrefix) {
          Config::setCommandPrefix(s_prefixInput);
          NotificationManager::getInstance()->add("Settings", "Prefix Updated",
                                                  NotificationType::Success);
          s_typingPrefix = false;
        }
      } else if (c >= 32 && c <= 126) {
        if (target->length() < cap)
          target->push_back(c);
        else {
          static ULONGLONG lastWarn = 0;
          if (GetTickCount64() - lastWarn > 2000) {
            NotificationManager::getInstance()->add("Input", "Text too long!",
                                                    NotificationType::Warning);
            lastWarn = GetTickCount64();
          }
        }
      }
    }
    if (s_cpEditingField > 0) {
      char *buf = (s_cpEditingField == 1) ? s_cpMinBuf : s_cpMaxBuf;
      int &len = (s_cpEditingField == 1) ? s_cpMinLen : s_cpMaxLen;
      if (c == 8) {
        if (len > 0) {
          len--;
          buf[len] = 0;
        }
      } else if ((c >= '0' && c <= '9') || c == '.') {
        if (len < 7) {
          buf[len++] = c;
          buf[len] = 0;
        }
      }
    }
  }
}

void ClickGUI::render(HDC hdc) {
  HWND hwnd = WindowFromDC(hdc);
  updateInput(hwnd);

  if (s_open) {
    static int focusTick = 0;
    if (focusTick++ % 20 == 0) {
      FocusFix::setIngameFocus(false);
      setMouseGrabbed(false);
    }
  }

  float dt = TimeUtil::getDelta();

  float alphaDiff = s_targetAlpha - s_animAlpha;
  s_animAlpha += alphaDiff * 0.15f;
  s_openingScale = lerp(0.95f, 1.0f, s_animAlpha);

  if (s_animAlpha <= 0.001f && !s_open)
    return;

  if (!g_guiFont.isInitialized()) {
    g_guiFont.init(hdc);
  }

  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  RECT cr;
  GetClientRect(hwnd, &cr);
  float sw = (float)cr.right;
  float sh = (float)cr.bottom;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, sw, sh, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd, &pt);
  float mx = (float)pt.x;
  float my = (float)pt.y;
  bool lClick = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  bool clickEvent = lClick && !s_lastLButton;
  s_lastLButton = lClick;

  RenderUtils::drawRect(0, 0, sw, sh, 0xA0000000);

  glPushMatrix();
  float centerX = g_x + g_w / 2;
  float centerY = g_y + g_h / 2;
  glTranslatef(centerX, centerY, 0);
  glScalef(s_openingScale, s_openingScale, 1.0f);
  glTranslatef(-centerX, -centerY, 0);

  if (s_open && s_animAlpha >= 0.95f) {
    if (lClick) {
      if (!s_dragging) {
        if (isHovered(mx, my, g_x, g_y, g_w, 52)) {
          s_dragging = true;
          s_dragOffsetX = mx - g_x;
          s_dragOffsetY = my - g_y;
        }
      } else {
        g_x = mx - s_dragOffsetX;
        g_y = my - s_dragOffsetY;
      }
    } else {
      s_dragging = false;
    }
  }

  if (s_open && s_waitingForKey) {
    for (int k = 1; k < 255; ++k) {
      if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON)
        continue;
      if ((GetAsyncKeyState(k) & 0x8000) != 0) {
        if (k == VK_ESCAPE) {
          s_waitingForKey = false;
        } else {
          Config::setClickGuiKey(k);
          Config::save();
          NotificationManager::getInstance()->add(
              "Settings", "Bind set to " + getKeyName(k),
              NotificationType::Success);
          s_waitingForKey = false;
        }
        break;
      }
    }
  }

  float mainX = g_x;
  float mainY = g_y;

  RenderUtils::drawRoundedRect(mainX - 1, mainY - 1, g_w + 2, g_h + 2, 10.0f,
                               THEME_BORDER, s_animAlpha);
  RenderUtils::drawRoundedRect(mainX, mainY, g_w, g_h, 10.0f, THEME_BG,
                               s_animAlpha);

  RenderUtils::drawRoundedRect(mainX, mainY, 170, g_h, 10.0f, THEME_SIDEBAR,
                               s_animAlpha);
  RenderUtils::drawRect(mainX + 160, mainY, 10, g_h, THEME_SIDEBAR,
                        s_animAlpha);

  RenderUtils::drawGradientRect(mainX + 170, mainY, g_w - 170, 52, 0xFF121214,
                                0x00121214);
  RenderUtils::drawRect(mainX + 170, mainY + 52, g_w - 170, 1, 0xFF202022);

  glEnable(GL_TEXTURE_2D);
  g_guiFont.drawString(mainX + 25, mainY + 24.0f, "OVSON",
                       applyAlpha(THEME_NAVY, s_animAlpha));
  g_guiFont.drawString(mainX + 75, mainY + 38.0f, "CLIENT",
                       applyAlpha(0xFFA0A0A5, s_animAlpha));

  float targetY = 85.0f + (s_targetTab * 45.0f);
  s_tabIndicatorY += (targetY - s_tabIndicatorY) * 0.2f;

  glDisable(GL_TEXTURE_2D);
  RenderUtils::drawRoundedRect(mainX + 15, mainY + s_tabIndicatorY - 12.0f, 140,
                               48, 8.0f, THEME_NAVY & 0x40FFFFFF, s_animAlpha);
  RenderUtils::drawRoundedRect(mainX + 4, mainY + s_tabIndicatorY - 4.0f, 3, 32,
                               1.5f, THEME_NAVY, s_animAlpha);
  glEnable(GL_TEXTURE_2D);

  if (s_activeTab != s_targetTab) {
    s_contentAlpha -= 0.15f;
    if (s_contentAlpha <= 0.0f) {
      s_activeTab = s_targetTab;
      s_contentSlide = 15.0f;
      s_targetScroll = 0.0f;
      s_scrollOffset = 0.0f;
    }
  } else {
    s_contentAlpha += 0.15f;
    if (s_contentAlpha > 1.0f)
      s_contentAlpha = 1.0f;
    s_contentSlide += (0.0f - s_contentSlide) * 0.15f;
  }

  static float s_maxScroll = 0.0f;
  if (s_targetScroll < 0)
    s_targetScroll = 0;
  if (s_targetScroll > s_maxScroll)
    s_targetScroll = s_maxScroll;

  s_scrollOffset += (s_targetScroll - s_scrollOffset) * 0.15f;

  const char *tabs[] = {"Visuals", "Players", "Tags",  "Settings",
                        "Colors",  "Debug",   "Utils", nullptr};
  float ty = mainY + 85;
  for (int i = 0; tabs[i]; ++i) {
    bool hover = isHovered(mx, my, mainX + 15, ty - 12, 140, 40);
    DWORD col =
        (s_targetTab == i) ? 0xFFFFFFFF : (hover ? 0xFFCCCCCC : 0xFF808085);
    g_guiFont.drawString(mainX + 45, ty, tabs[i], applyAlpha(col, s_animAlpha));
    if (clickEvent && hover) {
      s_targetTab = i;
      s_isDropdownOpen = false;
    }
    ty += 45;
  }

  float cx = mainX + 200 + s_contentSlide;
  float startCy = mainY + 85;
  float cy = startCy - s_scrollOffset;
  float alpha = s_animAlpha * s_contentAlpha;

  glEnable(GL_SCISSOR_TEST);
  glScissor((int)(mainX + 170), (int)(sh - (mainY + g_h - 10)),
            (int)(g_w - 170), (int)(g_h - 60));

  if (s_activeTab == 6) {
    g_guiFont.drawString(cx, cy, "Utilities", applyAlpha(0xFFFFFFFF, alpha));
    cy += 40;
    g_guiFont.drawString(cx, cy, "Bed Defense", applyAlpha(0xFFFFFFFF, alpha));
    bool hCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 95);
    glDisable(GL_TEXTURE_2D);
    DWORD bedCol = hCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 95, 8.0f,
                                 bedCol, 0.6f * alpha);
    if (hCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 95, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 18,
                         "X-Ray style outlines for bed defense blocks",
                         applyAlpha(0xFFA0A0A5, alpha));
    g_guiFont.drawString(cx, cy + 42,
                         "WARNING: THIS PROVIDES AN UNFAIR ADVANTAGE.",
                         applyAlpha(0xFFFF5555, alpha), 0.4f);
    g_guiFont.drawString(cx, cy + 54,
                         "YOU WILL BE BLACKLISTED IF CAUGHT. USE AT OWN RISK.",
                         applyAlpha(0xFFFF5555, alpha), 0.4f);

    bool enabled = Config::isBedDefenseEnabled();
    glDisable(GL_TEXTURE_2D);
    float swX = mainX + g_w - 65;
    drawSwitch(0, swX, cy + 15, enabled, hCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hCard) {
      bool newState = !enabled;
      Config::setBedDefenseEnabled(newState);
      if (newState)
        BedDefense::BedDefenseManager::getInstance()->enable();
      else
        BedDefense::BedDefenseManager::getInstance()->disable();

      NotificationManager::getInstance()->add(
          "Module", newState ? "Bed Defense Activated" : "Bed Defense Disabled",
          newState ? NotificationType::Success : NotificationType::Warning);
    }
    cy += 115;

    g_guiFont.drawString(cx, cy, "Chat Bypasser",
                         applyAlpha(0xFFFFFFFF, alpha));

    bool hBypass = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 85);
    glDisable(GL_TEXTURE_2D);
    DWORD bypCol = hBypass ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy + 30, g_w - 210, 85, 6.0f,
                                 bypCol, 0.6f * alpha);
    if (hBypass)
      RenderUtils::drawRect(mainX + 190, cy + 30, 3, 85, THEME_NAVY, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Bypass Chat Filter",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(
        cx, cy + 56, "Allows sending messages that would normally be blocked",
        applyAlpha(0xFFA0A0A5, alpha), 0.45f);

    bool bypassEnabled = Config::isChatBypasserEnabled();
    glDisable(GL_TEXTURE_2D);
    float bypassSwX = mainX + g_w - 65;
    drawSwitch(14, bypassSwX, cy + 40, bypassEnabled, hBypass && (my < cy + 65),
               alpha);
    glEnable(GL_TEXTURE_2D);

    bool hSmart = hBypass && (my >= cy + 65);
    bool smartEnabled = Config::isSmartChatBypassEnabled();
    float smartAlpha = alpha * (bypassEnabled ? 1.0f : 0.4f);

    g_guiFont.drawString(cx + 10, cy + 85, "Smart Mode",
                         applyAlpha(0xFFFFFFFF, smartAlpha), 0.42f);
    glDisable(GL_TEXTURE_2D);
    drawSwitch(25, bypassSwX, cy + 82, smartEnabled, hSmart && bypassEnabled,
               smartAlpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hBypass) {
      if (my < cy + 65) {
        Config::setChatBypasserEnabled(!bypassEnabled);
        NotificationManager::getInstance()->add(
            "Utils", !bypassEnabled ? "Bypasser Enabled" : "Bypasser Disabled",
            !bypassEnabled ? NotificationType::Success
                           : NotificationType::Warning);
      } else if (bypassEnabled) {
        Config::setSmartChatBypassEnabled(!smartEnabled);
      }
    }
    cy += 135;

    g_guiFont.drawString(cx, cy, "Faster Stats", applyAlpha(0xFFFFFFFF, alpha));
    bool hNicked = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD nickCol = hNicked ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy + 30, g_w - 210, 60, 6.0f,
                                 nickCol, 0.6f * alpha);
    if (hNicked)
      RenderUtils::drawRect(mainX + 190, cy + 30, 3, 60, THEME_NAVY, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Direct UUID Fetching",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58, "Use direct game UUIDs for instant stats",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool nickedBypass = Config::isNickedBypass();
    glDisable(GL_TEXTURE_2D);
    float nickSwX = mainX + g_w - 65;
    drawSwitch(20, nickSwX, cy + 40, nickedBypass, hNicked, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hNicked) {
      Config::setNickedBypass(!nickedBypass);
      NotificationManager::getInstance()->add(
          "Utils",
          !nickedBypass ? "Direct UUID Fetching Enabled"
                        : "Direct UUID Fetching Disabled",
          !nickedBypass ? NotificationType::Success
                        : NotificationType::Warning);
    }
    cy += 110;

    g_guiFont.drawString(cx, cy, "Replay Automations",
                         applyAlpha(0xFFFFFFFF, alpha));
    bool hReplay = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD replayCol = hReplay ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy + 30, g_w - 210, 60, 6.0f,
                                 replayCol, 0.6f * alpha);
    if (hReplay)
      RenderUtils::drawRect(mainX + 190, cy + 30, 3, 60, THEME_NAVY, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Replay Report Spammer",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58,
                         "Auto reporting for cheating (requires Anvil menu)",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool replaySpammer = Utils::ReplaySpammer::getInstance().isEnabled();
    glDisable(GL_TEXTURE_2D);
    float replaySwX = mainX + g_w - 65;
    drawSwitch(21, replaySwX, cy + 40, replaySpammer, hReplay, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hReplay) {
      Utils::ReplaySpammer::getInstance().toggle();
    }
    cy += 110;

    g_guiFont.drawString(cx, cy, "Aurora Denicker",
                         applyAlpha(0xFFFFFFFF, alpha));
    bool hDenick = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD denickCol = hDenick ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy + 30, g_w - 210, 60, 6.0f,
                                 denickCol, 0.6f * alpha);
    if (hDenick)
      RenderUtils::drawRect(mainX + 190, cy + 30, 3, 60, THEME_NAVY, alpha);

    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy + 40, "Number Denicker",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 58,
                         "Reveal nicks via game statistics (Powered by Aurora)",
                         applyAlpha(0xFFA0A0A5, alpha));

    bool denickEnabled = Config::isNumberDenickerEnabled();
    glDisable(GL_TEXTURE_2D);
    float denickSwX = mainX + g_w - 65;
    drawSwitch(22, denickSwX, cy + 40, denickEnabled, hDenick, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hDenick) {
      Config::setNumberDenickerEnabled(!denickEnabled);
      Config::save();
      NotificationManager::getInstance()->add(
          "Utils",
          !denickEnabled ? "Number Denicker Enabled"
                         : "Number Denicker Disabled",
          !denickEnabled ? NotificationType::Success
                         : NotificationType::Warning);
    }
    cy += 110;

    g_guiFont.drawString(cx, cy, "Anticheat", applyAlpha(0xFFFFFFFF, alpha));
    const float acCardH = 222.0f;
    bool hAc = isHovered(mx, my, mainX + 190, cy + 30, g_w - 210, acCardH);
    glDisable(GL_TEXTURE_2D);
    DWORD acCol = hAc ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy + 30, g_w - 210, acCardH, 6.0f,
                                 acCol, 0.6f * alpha);
    if (hAc)
      RenderUtils::drawRect(mainX + 190, cy + 30, 3, acCardH, THEME_NAVY,
                            alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy + 40, "Detect Cheaters (BETA)",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(
        cx, cy + 58,
        "Four client-side checks: NoSlow, AutoBlock, Eagle, Scaffold",
        applyAlpha(0xFFA0A0A5, alpha), 0.45f);

    bool acEnabled = Config::isAnticheatEnabled();
    glDisable(GL_TEXTURE_2D);
    float acSwX = mainX + g_w - 65;
    bool hAcMaster = hAc && my < cy + 70;
    drawSwitch(40, acSwX, cy + 40, acEnabled, hAcMaster, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hAcMaster) {
      Config::setAnticheatEnabled(!acEnabled);
      NotificationManager::getInstance()->add(
          "Utils", !acEnabled ? "Anticheat Enabled" : "Anticheat Disabled",
          !acEnabled ? NotificationType::Success : NotificationType::Warning);
    }

    float rowAlpha = alpha * (acEnabled ? 1.0f : 0.45f);
    struct AcSub {
      const char *label;
      bool (*get)();
      void (*set)(bool);
      int switchId;
    };
    static const AcSub kSubs[] = {
        {"NoSlow", &Config::isAnticheatNoSlowEnabled,
         &Config::setAnticheatNoSlowEnabled, 41},
        {"AutoBlock", &Config::isAnticheatAutoBlockEnabled,
         &Config::setAnticheatAutoBlockEnabled, 42},
        {"Eagle", &Config::isAnticheatEagleEnabled,
         &Config::setAnticheatEagleEnabled, 43},
        {"Scaffold", &Config::isAnticheatScaffoldEnabled,
         &Config::setAnticheatScaffoldEnabled, 44},
        {"Check Self", &Config::isAnticheatCheckSelfEnabled,
         &Config::setAnticheatCheckSelfEnabled, 46},
    };
    const float subStartY = cy + 78;
    const float subRowH = 22.0f;
    for (size_t i = 0; i < sizeof(kSubs) / sizeof(kSubs[0]); ++i) {
      float ry = subStartY + (float)i * subRowH;
      bool hSub = hAc && my >= ry - 2 && my < ry + subRowH - 2;
      bool cur = kSubs[i].get();
      g_guiFont.drawString(cx + 10, ry + 4, kSubs[i].label,
                           applyAlpha(0xFFFFFFFF, rowAlpha), 0.42f);
      glDisable(GL_TEXTURE_2D);
      drawSwitch(kSubs[i].switchId, acSwX, ry + 2, cur, hSub && acEnabled,
                 rowAlpha);
      glEnable(GL_TEXTURE_2D);
      if (clickEvent && hSub && acEnabled) {
        kSubs[i].set(!cur);
      }
    }
    cy += (int)acCardH + 25;

  } else if (s_activeTab == 0) {
    g_guiFont.drawString(cx, cy, "Overlays", applyAlpha(0xFFFFFFFF, alpha));
    cy += 40;
    auto drawSettingsCard = [&](const char *title, const char *desc, bool &val,
                                int id, float &cy_ref) {
      bool hover = isHovered(mx, my, mainX + 190, cy_ref - 10, g_w - 210, 62);

      glDisable(GL_TEXTURE_2D);
      DWORD cardCol = hover ? 0xFF323236 : THEME_CARD;
      RenderUtils::drawRoundedRect(mainX + 190, cy_ref - 10, g_w - 210, 62,
                                   8.0f, cardCol, 0.6f * alpha);
      if (hover) {
        RenderUtils::drawRect(mainX + 190, cy_ref - 10, 3, 62, THEME_NAVY,
                              alpha);
      }
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx, cy_ref, title, applyAlpha(0xFFFFFFFF, alpha));
      g_guiFont.drawString(cx, cy_ref + 18, desc, applyAlpha(0xFFA0A0A5, alpha),
                           0.45f);

      float swX = mainX + g_w - 65;
      drawSwitch(id, swX, cy_ref + 5, val, hover, alpha);

      if (clickEvent && hover) {
        val = !val;
      }
      cy_ref += 72;
    };

    bool ovEnabled = StatsOverlay::isEnabled();
    bool oldOv = ovEnabled;
    drawSettingsCard("Stats Overlay",
                     "Display player skill metrics in a clean table", ovEnabled,
                     1, cy);
    if (ovEnabled != oldOv)
      StatsOverlay::setEnabled(ovEnabled);

    bool notifEnabled = Config::isNotificationsEnabled();
    drawSettingsCard("Refined Notifications",
                     "Enable silky smooth toast alerts", notifEnabled, 2, cy);
    Config::setNotificationsEnabled(notifEnabled);

    bool techEnabled = Config::isTechEnabled();
    drawSettingsCard("Tech Overlay", "Show technical JNI and system metrics",
                     techEnabled, 9, cy);
    Config::setTechEnabled(techEnabled);

    bool blurEnabled = Config::isMotionBlurEnabled();
    drawSettingsCard("Motion Blur", "Adds a cinematic trail to camera movement",
                     blurEnabled, 4, cy);
    Config::setMotionBlurEnabled(blurEnabled);
    cy += 10;

    if (blurEnabled) {
      g_guiFont.drawString(cx, cy, "Blur Intensity",
                           applyAlpha(0xFFA0A0A5, alpha));
      cy += 20;
      float sliderW = 200.0f;
      float sliderH = 10.0f;
      float sliderVal = Config::getMotionBlurAmount();

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRect(cx, cy, sliderW, sliderH, 0xFF2A2A2E, alpha);
      RenderUtils::drawRect(cx, cy, sliderW * sliderVal, sliderH,
                            Config::getThemeColor(), alpha);
      glEnable(GL_TEXTURE_2D);

      bool hSlider = isHovered(mx, my, cx, cy - 5, sliderW, sliderH + 10);
      if (hSlider && lClick) { // slider stays drag-friendly with held LMB
        float newVal = (mx - cx) / sliderW;
        if (newVal < 0)
          newVal = 0;
        if (newVal > 1)
          newVal = 1;
        Config::setMotionBlurAmount(newVal);
      }
      cy += 30;
    }

    bool nameTagsEnabled = Config::isNameTagsEnabled();
    drawSettingsCard("NameTags",
                     "Renders the Bedwars stats above every nearby player",
                     nameTagsEnabled, 7, cy);
    Config::setNameTagsEnabled(nameTagsEnabled);
    cy += 10;

    if (nameTagsEnabled) {
      const float panelX = mainX + 190;
      const float panelW = g_w - 210;
      {
        float h = Config::getNameTagHeight();
        const float cardH = 34.0f;
        float rowY = cy - 6;
        bool rowHover = isHovered(mx, my, panelX, rowY, panelW, cardH);

        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(panelX, rowY, panelW, cardH, 6.0f,
                                     rowHover ? 0xFF222226 : THEME_CARD,
                                     0.5f * alpha);
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(cx, cy, "Label height",
                             applyAlpha(0xFFFFFFFF, alpha));

        char hbuf[24];
        sprintf_s(hbuf, "%.1f m", h);

        const float btnW = 22, btnH = 22;
        const float btnY = cy - 1;
        const float upX = panelX + panelW - 12 - btnW;
        const float dnX = upX - 6 - btnW;
        const float valueX = dnX - 50;

        g_guiFont.drawString(valueX, cy, hbuf,
                             applyAlpha(0xFFE6E6EA, alpha));

        bool hUp = isHovered(mx, my, upX, btnY, btnW, btnH);
        bool hDn = isHovered(mx, my, dnX, btnY, btnW, btnH);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(upX, btnY, btnW, btnH, 4.0f,
                                     hUp ? THEME_NAVY : 0xFF2A2A2E,
                                     alpha);
        RenderUtils::drawRoundedRect(dnX, btnY, btnW, btnH, 4.0f,
                                     hDn ? THEME_NAVY : 0xFF2A2A2E,
                                     alpha);
        const float cxU = upX + btnW / 2.0f;
        const float cyU = btnY + btnH / 2.0f;
        const float cxD = dnX + btnW / 2.0f;
        const float cyD = btnY + btnH / 2.0f;
        const float armLen = 9.0f, armThick = 2.0f;
        DWORD sym = 0xFFFFFFFF;
        RenderUtils::drawRect(cxU - armLen / 2, cyU - armThick / 2,
                              armLen, armThick, sym, alpha);
        RenderUtils::drawRect(cxU - armThick / 2, cyU - armLen / 2,
                              armThick, armLen, sym, alpha);
        RenderUtils::drawRect(cxD - armLen / 2, cyD - armThick / 2,
                              armLen, armThick, sym, alpha);
        glEnable(GL_TEXTURE_2D);

        if (hUp && clickEvent && h < 4.0f)
          Config::setNameTagHeight(h + 0.1f);
        if (hDn && clickEvent && h > 0.5f)
          Config::setNameTagHeight(h - 0.1f);

        cy += 30;
      }

      auto slots = Config::getNameTagStats();
      auto labelFor = [](const std::string &k) -> const char * {
        if (k == "star") return "Star";
        if (k == "fkdr") return "FKDR";
        if (k == "fk")   return "Final Kills";
        if (k == "wins") return "Wins";
        if (k == "wlr")  return "WLR";
        if (k == "ws")   return "Winstreak";
        return "?";
      };

      const float rowH = 32.0f;
      const float rowGap = 4.0f;
      const float handleW = 26.0f;

      static int   s_dragIdx     = -1;
      static float s_dragMouseDY = 0.0f;
      static bool  s_dragMoved   = false;

      static std::vector<float> s_rowAnimY(6, 0.0f);
      if (s_rowAnimY.size() != slots.size())
        s_rowAnimY.assign(slots.size(), 0.0f);
      for (auto &v : s_rowAnimY) v *= 0.75f;

      if (clickEvent) {
        for (size_t r = 0; r < slots.size(); ++r) {
          float ry = cy + r * (rowH + rowGap);
          if (isHovered(mx, my, panelX, ry, handleW, rowH)) {
            s_dragIdx = (int)r;
            s_dragMouseDY = my - ry;
            s_dragMoved = false;
            break;
          }
        }
      }

      if (s_dragIdx >= 0 && lClick) {
        int over = (int)((my - cy) / (rowH + rowGap));
        if (over < 0) over = 0;
        if (over >= (int)slots.size()) over = (int)slots.size() - 1;
        if (over != s_dragIdx) {
          int from = s_dragIdx, to = over;
          if (from < to) {
            for (int k = from + 1; k <= to; ++k)
              s_rowAnimY[k] = -(rowH + rowGap);
          } else {
            for (int k = to; k < from; ++k)
              s_rowAnimY[k] = +(rowH + rowGap);
          }
          auto moved = slots[from];
          slots.erase(slots.begin() + from);
          slots.insert(slots.begin() + to, moved);
          Config::setNameTagStats(slots);
          s_dragIdx = to;
          s_dragMoved = true;
        }
      }
      if (!lClick) s_dragIdx = -1;

      for (size_t r = 0; r < slots.size(); ++r) {
        float ry = cy + r * (rowH + rowGap) + s_rowAnimY[r];
        bool isDragging = ((int)r == s_dragIdx);
        bool rowHover = isHovered(mx, my, panelX, ry, panelW, rowH);
        bool en = slots[r].second;

        DWORD card =
            isDragging ? 0xFF2A2A30
            : rowHover ? 0xFF1F1F22
                       : THEME_CARD;
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(panelX, ry, panelW, rowH, 6.0f,
                                     card, 0.55f * alpha);
        if (en) {
          RenderUtils::drawRoundedRect(panelX, ry, 3, rowH, 1.5f,
                                       THEME_NAVY, alpha);
        }
        DWORD gripCol = (rowHover || isDragging) ? 0xFFE0E0E5 : 0xFF6F6F75;
        float gx = panelX + 10, gy = ry + rowH / 2;
        for (int dy = -1; dy <= 1; ++dy) {
          RenderUtils::drawCircle(gx,     gy + dy * 5, 1.4f, gripCol, alpha);
          RenderUtils::drawCircle(gx + 5, gy + dy * 5, 1.4f, gripCol, alpha);
        }
        glEnable(GL_TEXTURE_2D);

        g_guiFont.drawString(panelX + handleW + 14, ry + 9,
                             labelFor(slots[r].first),
                             applyAlpha(0xFFFFFFFF, alpha));

        float swX = panelX + panelW - 50;
        float swY = ry + 7;
        drawSwitch(2000 + (int)r, swX, swY, en, rowHover, alpha);

        bool clickOnSwitch =
            isHovered(mx, my, swX, swY, 34.0f, 18.0f);
        bool clickOnRowBody =
            rowHover && !isHovered(mx, my, panelX, ry, handleW, rowH);
        if (clickEvent && (clickOnSwitch || clickOnRowBody) &&
            !s_dragMoved && s_dragIdx < 0) {
          slots[r].second = !en;
          Config::setNameTagStats(slots);
        }
      }
      if (!lClick) s_dragMoved = false;

      cy += slots.size() * (rowH + rowGap) + 10;
    }

    cy += 20;
    g_guiFont.drawString(cx, cy, "Table Customization",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 35;

    g_guiFont.drawString(cx, cy, "Sort By:", applyAlpha(0xFFA0A0A5, alpha));
    const char *sModes[] = {"Team", "Star", "FK", "FKDR", "Wins", "WLR", "WS"};
    std::string curSort = Config::getSortMode();

    float bx = cx + 80;
    for (int i = 0; i < 7; ++i) {
      bool hov = isHovered(mx, my, bx, cy - 5, 50, 25);
      bool sel = (curSort == sModes[i]);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(
          bx, cy - 5, 50, 25, 4.0f,
          sel ? THEME_NAVY : (hov ? 0xFF323236 : THEME_CARD), alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(bx + 5, cy - 2, sModes[i],
                           applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha),
                           0.45f);
      if (clickEvent && hov) {
        Config::setSortMode(sModes[i]);
        NotificationManager::getInstance()->add(
            "Sort", "Sorting by " + std::string(sModes[i]),
            NotificationType::Info);
      }
      bx += 58;
    }

    cy += 40;
    g_guiFont.drawString(cx, cy,
                         "Tab List Display:", applyAlpha(0xFFA0A0A5, alpha));
    const char *dModes[] = {"fk", "fkdr", "wins", "wlr", "ws"};
    std::string curMode = Config::getTabDisplayMode();

    float dbx = cx + 170;
    for (int i = 0; i < 5; ++i) {
      bool hov = isHovered(mx, my, dbx, cy - 5, 45, 25);
      bool sel = (curMode == dModes[i]);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(
          dbx, cy - 5, 45, 25, 4.0f,
          sel ? THEME_NAVY : (hov ? 0xFF323236 : THEME_CARD), alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(
          dbx + (strlen(dModes[i]) > 3 ? 2 : 10), cy - 2, dModes[i],
          applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha), 0.4f);
      if (clickEvent && hov) {
        Config::setTabDisplayMode(dModes[i]);
        NotificationManager::getInstance()->add(
            "Tab", "Display set to " + std::string(dModes[i]),
            NotificationType::Info);
      }
      dbx += 50;
    }
    cy += 45;

    {
      bool isDesc = Config::isTabSortDescending();
      std::string currentOrder = isDesc ? "Descending" : "Ascending";
      const char *orders[] = {"Ascending", "Descending"};

      g_guiFont.drawString(cx, cy + 8,
                           "Sort Order:", applyAlpha(0xFFA0A0A5, alpha));

      float dropX = cx + 100;
      float dropW = 160.0f;
      float dropH = 32.0f;
      bool hovDrop = isHovered(mx, my, dropX, cy, dropW, dropH);

      s_sortOrderDropdownAnim +=
          (s_isSortOrderDropdownOpen ? 1.0f - s_sortOrderDropdownAnim
                                     : 0.0f - s_sortOrderDropdownAnim) *
          0.15f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(dropX, cy, dropW, dropH, 6.0f, THEME_CARD,
                                   0.8f * alpha);
      if (hovDrop)
        RenderUtils::drawRoundedRect(dropX, cy + dropH - 3, dropW, 3.0f, 1.5f,
                                     0xFF808085, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(dropX + 10, cy + 6, currentOrder,
                           applyAlpha(0xFFFFFFFF, alpha), 0.45f);
      g_guiFont.drawString(dropX + dropW - 18, cy + 10,
                           s_isSortOrderDropdownOpen ? "-" : "+",
                           applyAlpha(0xFFA0A0A5, alpha), 0.45f);

      if (clickEvent && hovDrop) {
        s_isSortOrderDropdownOpen = !s_isSortOrderDropdownOpen;
      }

      if (s_sortOrderDropdownAnim > 0.01f) {
        float listY = cy + dropH + 2;
        for (int i = 0; i < 2; ++i) {
          float itemY = listY + (i * dropH);
          bool hItem = isHovered(mx, my, dropX, itemY, dropW, dropH);

          glDisable(GL_TEXTURE_2D);
          DWORD itCol = hItem ? 0xFF323236 : THEME_CARD;
          RenderUtils::drawRoundedRect(dropX, itemY, dropW, dropH, 4.0f, itCol,
                                       0.95f * alpha * s_sortOrderDropdownAnim);
          if (hItem)
            RenderUtils::drawRect(dropX, itemY, 3, dropH, THEME_NAVY,
                                  alpha * s_sortOrderDropdownAnim);
          glEnable(GL_TEXTURE_2D);

          g_guiFont.drawString(
              dropX + 15, itemY + 12, orders[i],
              applyAlpha(currentOrder == orders[i] ? 0xFFFFFFFF : 0xFFA0A0A5,
                         alpha * s_sortOrderDropdownAnim),
              0.45f);

          if (clickEvent && hItem && (s_sortOrderDropdownAnim > 0.8f)) {
            Config::setTabSortDescending(i == 1);
            s_isSortOrderDropdownOpen = false;
            NotificationManager::getInstance()->add(
                "Sort", std::string("Order set to: ") + orders[i],
                NotificationType::Info);
          }
        }
        cy += (2 * dropH) * s_sortOrderDropdownAnim;
      }
    }
    cy += 50;
    g_guiFont.drawString(cx, cy,
                         "Visible Columns:", applyAlpha(0xFFA0A0A5, alpha));
    cy += 30;

    {
      const char *targets[] = {"GUI Overlay", "Better Tab"};
      g_guiFont.drawString(cx, cy + 8,
                           "Target:", applyAlpha(0xFFA0A0A5, alpha));

      float dropX = cx + 60;
      float dropW = 140.0f;
      float dropH = 30.0f;
      bool hovDrop = isHovered(mx, my, dropX, cy, dropW, dropH);

      s_columnTargetDropdownAnim +=
          (s_isColumnTargetDropdownOpen ? 1.0f - s_columnTargetDropdownAnim
                                        : 0.0f - s_columnTargetDropdownAnim) *
          0.15f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(dropX, cy, dropW, dropH, 6.0f, THEME_CARD,
                                   0.8f * alpha);
      if (hovDrop)
        RenderUtils::drawRoundedRect(dropX, cy + dropH - 2, dropW, 2.0f, 1.0f,
                                     0xFF808085, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(dropX + 10, cy + 6, targets[s_columnTargetMode],
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);

      if (clickEvent && hovDrop)
        s_isColumnTargetDropdownOpen = !s_isColumnTargetDropdownOpen;

      if (s_columnTargetDropdownAnim > 0.01f) {
        float listY = cy + dropH + 2;
        for (int i = 0; i < 2; ++i) {
          float itemY = listY + (i * dropH);
          bool hItem = isHovered(mx, my, dropX, itemY, dropW, dropH);
          glDisable(GL_TEXTURE_2D);
          DWORD itCol = hItem ? 0xFF323236 : THEME_CARD;
          RenderUtils::drawRoundedRect(dropX, itemY, dropW, dropH, 4.0f, itCol,
                                       0.95f * alpha *
                                           s_columnTargetDropdownAnim);
          glEnable(GL_TEXTURE_2D);
          g_guiFont.drawString(
              dropX + 10, itemY + 10, targets[i],
              applyAlpha(s_columnTargetMode == i ? 0xFFFFFFFF : 0xFFA0A0A5,
                         alpha * s_columnTargetDropdownAnim),
              0.42f);
          if (clickEvent && hItem && s_columnTargetDropdownAnim > 0.8f) {
            s_columnTargetMode = i;
            s_isColumnTargetDropdownOpen = false;
          }
        }
        cy += (2 * dropH) * s_columnTargetDropdownAnim;
      }
    }
    cy += 45;

    struct ColToggle {
      std::string name;
      bool enabled;
      std::function<void(bool)> setter;
    };

    std::vector<ColToggle> toggles;
    if (s_columnTargetMode == 0) {
      toggles = {
          {"Star", Config::isOvShowStar(),
           [](bool b) { Config::setOvShowStar(b); }},
          {"FK", Config::isOvShowFk(), [](bool b) { Config::setOvShowFk(b); }},
          {"FKDR", Config::isOvShowFkdr(),
           [](bool b) { Config::setOvShowFkdr(b); }},
          {"Wins", Config::isOvShowWins(),
           [](bool b) { Config::setOvShowWins(b); }},
          {"WLR", Config::isOvShowWlr(),
           [](bool b) { Config::setOvShowWlr(b); }},
          {"WS", Config::isOvShowWs(), [](bool b) { Config::setOvShowWs(b); }},
          {"Kills", Config::isOvShowKills(),
           [](bool b) { Config::setOvShowKills(b); }},
          {"KDR", Config::isOvShowKdr(),
           [](bool b) { Config::setOvShowKdr(b); }},
          {"Beds", Config::isOvShowBeds(),
           [](bool b) { Config::setOvShowBeds(b); }},
          {"BLR", Config::isOvShowBlr(),
           [](bool b) { Config::setOvShowBlr(b); }},
          {"Ping", Config::isOvShowPing(),
           [](bool b) { Config::setOvShowPing(b); }},
          {"Tags", Config::isOvShowTags(),
           [](bool b) { Config::setOvShowTags(b); }}};
    } else {
      toggles = {{"Star", Config::isProShowStar(),
                  [](bool b) { Config::setProShowStar(b); }},
                 {"FK", Config::isProShowFk(),
                  [](bool b) { Config::setProShowFk(b); }},
                 {"FKDR", Config::isProShowFkdr(),
                  [](bool b) { Config::setProShowFkdr(b); }},
                 {"Wins", Config::isProShowWins(),
                  [](bool b) { Config::setProShowWins(b); }},
                 {"WLR", Config::isProShowWlr(),
                  [](bool b) { Config::setProShowWlr(b); }},
                 {"WS", Config::isProShowWs(),
                  [](bool b) { Config::setProShowWs(b); }},
                 {"Kills", Config::isProShowKills(),
                  [](bool b) { Config::setProShowKills(b); }},
                 {"KDR", Config::isProShowKdr(),
                  [](bool b) { Config::setProShowKdr(b); }},
                 {"Beds", Config::isProShowBeds(),
                  [](bool b) { Config::setProShowBeds(b); }},
                 {"BLR", Config::isProShowBlr(),
                  [](bool b) { Config::setProShowBlr(b); }},
                 {"Ping", Config::isProShowPing(),
                  [](bool b) { Config::setProShowPing(b); }},
                 {"Tags", Config::isProShowTags(),
                  [](bool b) { Config::setProShowTags(b); }},
                 {"HP", Config::isProShowHp(),
                  [](bool b) { Config::setProShowHp(b); }}};
    }

    float tx = cx;
    for (size_t i = 0; i < toggles.size(); ++i) {
      float cardW = 125.0f;
      float cardH = 36.0f;
      bool hov = isHovered(mx, my, tx, cy, cardW, cardH);

      glDisable(GL_TEXTURE_2D);
      DWORD baseCol = hov ? 0xFF323236 : THEME_CARD;
      RenderUtils::drawRoundedRect(tx, cy, cardW, cardH, 5.0f, baseCol,
                                   0.7f * alpha);
      if (hov)
        RenderUtils::drawRect(tx, cy, 3, cardH, THEME_NAVY, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(
          tx + 12, cy + cardH / 2.0f - 9.0f, toggles[i].name,
          applyAlpha(toggles[i].enabled ? 0xFFFFFFFF : 0xFF808085, alpha),
          0.45f);

      glDisable(GL_TEXTURE_2D);
      drawSwitch(100 + (int)i, tx + cardW - 45, cy + (cardH - 18.0f) / 2.0f,
                 toggles[i].enabled, hov, alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hov) {
        toggles[i].setter(!toggles[i].enabled);
      }

      if (i % 3 == 2) {
        tx = cx;
        cy += cardH + 10;
      } else {
        tx += cardW + 12;
      }
    }
    cy += 60;

    g_guiFont.drawString(cx, cy, "Tab List & Chat Features",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 35;

    {
      bool hTab = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 85);
      glDisable(GL_TEXTURE_2D);
      DWORD tabCol = hTab ? 0xFF323236 : THEME_CARD;
      RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 85, 8.0f,
                                   tabCol, 0.6f * alpha);
      if (hTab) {
        RenderUtils::drawRect(mainX + 190, cy - 10, 3, 85, THEME_NAVY, alpha);
      }
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx, cy, "Tab List Overlay",
                           applyAlpha(0xFFFFFFFF, alpha));
      g_guiFont.drawString(cx, cy + 18,
                           "Show stats next to names in player TAB list",
                           applyAlpha(0xFFA0A0A5, alpha), 0.45f);

      bool tabEnabled = Config::isTabEnabled();
      glDisable(GL_TEXTURE_2D);
      float tabSwX = mainX + g_w - 65;
      drawSwitch(30, tabSwX, cy + 5, tabEnabled, hTab && (my < cy + 30), alpha);
      glEnable(GL_TEXTURE_2D);

      bool hPro = hTab && (my >= cy + 30);
      bool proEnabled = Config::isBetterTabModeEnabled();
      float proAlpha = alpha * (tabEnabled ? 1.0f : 0.4f);

      g_guiFont.drawString(cx + 10, cy + 45, "BetterTab",
                           applyAlpha(0xFFFFFFFF, proAlpha), 0.42f);
      glDisable(GL_TEXTURE_2D);
      drawSwitch(33, tabSwX, cy + 42, proEnabled, hPro && tabEnabled, proAlpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hTab) {
        if (my < cy + 30) {
          Config::setTabEnabled(!tabEnabled);
          NotificationManager::getInstance()->add(
              "Tab", !tabEnabled ? "Tab List Enabled" : "Tab List Disabled",
              !tabEnabled ? NotificationType::Success
                          : NotificationType::Warning);
        } else if (tabEnabled) {
          Config::setBetterTabModeEnabled(!proEnabled);
          NotificationManager::getInstance()->add(
              "Tab",
              !proEnabled ? "Better Mode Enabled" : "Better Mode Disabled",
              !proEnabled ? NotificationType::Success
                          : NotificationType::Warning);
        }
      }
      cy += 95;

      bool hChatStats = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 62);
      glDisable(GL_TEXTURE_2D);
      DWORD csCol = hChatStats ? 0xFF323236 : THEME_CARD;
      RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 62, 8.0f,
                                   csCol, 0.6f * alpha);
      if (hChatStats) {
        RenderUtils::drawRect(mainX + 190, cy - 10, 3, 62, THEME_NAVY, alpha);
      }
      glEnable(GL_TEXTURE_2D);

      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy, "Pre-Game Chat Stats",
                           applyAlpha(0xFFFFFFFF, alpha * s_contentAlpha));
      g_guiFont.drawString(
          cx, cy + 18, "Auto-fetch stats when players speak in pre-game lobby",
          applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

      bool chatStatsEnabled = Config::isPreGameChatStatsEnabled();
      glDisable(GL_TEXTURE_2D);
      float csSwX = mainX + g_w - 65;
      drawSwitch(31, csSwX, cy + 5, chatStatsEnabled, hChatStats, alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hChatStats) {
        Config::setPreGameChatStatsEnabled(!chatStatsEnabled);
        NotificationManager::getInstance()->add(
            "Chat",
            !chatStatsEnabled ? "Pre-Game Stats Enabled"
                              : "Pre-Game Stats Disabled",
            !chatStatsEnabled ? NotificationType::Success
                              : NotificationType::Warning);
      }
      cy += 72;
    }

    {
      bool hReport = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 82);

      bool hovAnyChannel = false;
      float tempChX = cx + 80;
      for (int i = 0; i < 3; ++i) {
        if (isHovered(mx, my, tempChX, cy + 37, 55, 25))
          hovAnyChannel = true;
        tempChX += 62;
      }

      glDisable(GL_TEXTURE_2D);
      DWORD repCol = hReport ? 0xFF323236 : THEME_CARD;
      RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 82, 8.0f,
                                   repCol, 0.6f * alpha);
      if (hReport) {
        RenderUtils::drawRect(mainX + 190, cy - 10, 3, 82, THEME_NAVY, alpha);
      }
      glEnable(GL_TEXTURE_2D);

      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(cx, cy, "Team Stats Report",
                           applyAlpha(0xFFFFFFFF, alpha * s_contentAlpha));
      g_guiFont.drawString(
          cx, cy + 18, "Auto-report team averages to chat (.teamreport)",
          applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

      bool reportEnabled = Config::isTeamReportEnabled();
      glDisable(GL_TEXTURE_2D);
      float repSwX = mainX + g_w - 65;
      drawSwitch(32, repSwX, cy + 5, reportEnabled, hReport && !hovAnyChannel,
                 alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hReport && !hovAnyChannel) {
        Config::setTeamReportEnabled(!reportEnabled);
        NotificationManager::getInstance()->add(
            "Team Report",
            !reportEnabled ? "Team Report Enabled" : "Team Report Disabled",
            !reportEnabled ? NotificationType::Success
                           : NotificationType::Warning);
      }

      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(
          cx, cy + 42,
          "Channel:", applyAlpha(0xFFA0A0A5, alpha * s_contentAlpha), 0.45f);

      const char *channels[] = {"/pc", "/ac", "/shout"};
      std::string curChannel = Config::getTeamReportChannel();
      float chX = cx + 80;
      for (int i = 0; i < 3; ++i) {
        bool hov = isHovered(mx, my, chX, cy + 37, 55, 25);
        bool sel = (curChannel == channels[i]);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(chX, cy + 37, 55, 25, 4.0f,
                                     sel ? THEME_NAVY
                                         : (hov ? 0xFF424246 : THEME_CARD),
                                     alpha * s_contentAlpha);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(
            chX + 8, cy + 44, channels[i],
            applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha * s_contentAlpha),
            0.4f);
        if (clickEvent && hov) {
          Config::setTeamReportChannel(channels[i]);
          NotificationManager::getInstance()->add(
              "Team Report", std::string("Channel set to ") + channels[i],
              NotificationType::Info);
        }
        chX += 62;
      }
      cy += 92;
    }
    cy += 20;
  } else if (s_activeTab == 1) {
    g_guiFont.drawString(cx, cy, "Player Search",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 24;

    float searchW = g_w - 200;
    float searchH = 36.0f;
    float searchX = mainX + 186;
    bool hSearch = isHovered(mx, my, searchX, cy, searchW, searchH);
    glDisable(GL_TEXTURE_2D);
    DWORD searchBg =
        s_typingSearch ? 0xFF1E1E22 : (hSearch ? 0xFF222226 : THEME_CARD);
    RenderUtils::drawRoundedRect(searchX, cy, searchW, searchH, 8.0f, searchBg,
                                 0.8f * alpha);
    if (s_typingSearch)
      RenderUtils::drawRoundedOutline(searchX, cy, searchW, searchH, 8.0f, 1.5f,
                                      THEME_NAVY, 0.8f * alpha);
    glEnable(GL_TEXTURE_2D);

    std::string dispSearch = s_playerSearch;
    if (s_typingSearch && (GetTickCount64() / 500) % 2 == 0)
      dispSearch += "|";
    if (dispSearch.empty() && !s_typingSearch)
      dispSearch = "Search player...";

    g_guiFont.drawString(
        searchX + 12, cy + 7, dispSearch,
        applyAlpha(s_typingSearch ? 0xFFFFFFFF : 0xFF606065, alpha));

    if (clickEvent && hSearch) {
      s_typingSearch = true;
      s_typingApiKey = s_typingAutoGG = s_typingUrchinKey = false;
    } else if (clickEvent && !hSearch)
      s_typingSearch = false;

    cy += searchH + 10;

    if (s_searching) {
      float loadCardW = g_w - 210;
      float loadCardH = 50.0f;
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(mainX + 190, cy, loadCardW, loadCardH, 8.0f,
                                   THEME_CARD, 0.6f * alpha);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_skinPendingW, s_skinPendingH,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, s_skinPendingData.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        s_skinPendingReady = false;
        s_skinPendingData.clear();
      }

      float cardW = g_w - 210;
      float cardX = mainX + 190;

      auto getRankDisplay = [&]() -> std::string {
        const auto &r = s_lookupResult;
        if (!r.prefix.empty())
          return r.prefix;
        if (r.rank == "ADMIN")
          return "\xC2\xA7"
                 "c[ADMIN]";
        if (r.rank == "MODERATOR")
          return "\xC2\xA7"
                 "2[MOD]";
        if (r.rank == "HELPER")
          return "\xC2\xA7"
                 "9[HELPER]";
        if (r.rank == "YOUTUBER")
          return "\xC2\xA7"
                 "c[\xC2\xA7"
                 "fYOUTUBE\xC2\xA7"
                 "c]";
        if (r.monthlyPackageRank == "SUPERSTAR") {
          std::string plusCol = "\xC2\xA7"
                                "c";
          if (r.rankPlusColor == "GOLD")
            plusCol = "\xC2\xA7"
                      "6";
          else if (r.rankPlusColor == "AQUA")
            plusCol = "\xC2\xA7"
                      "b";
          else if (r.rankPlusColor == "GREEN")
            plusCol = "\xC2\xA7"
                      "a";
          else if (r.rankPlusColor == "LIGHT_PURPLE")
            plusCol = "\xC2\xA7"
                      "d";
          else if (r.rankPlusColor == "WHITE")
            plusCol = "\xC2\xA7"
                      "f";
          else if (r.rankPlusColor == "BLUE")
            plusCol = "\xC2\xA7"
                      "9";
          else if (r.rankPlusColor == "DARK_RED")
            plusCol = "\xC2\xA7"
                      "4";
          else if (r.rankPlusColor == "DARK_AQUA")
            plusCol = "\xC2\xA7"
                      "3";
          else if (r.rankPlusColor == "DARK_GREEN")
            plusCol = "\xC2\xA7"
                      "2";
          else if (r.rankPlusColor == "DARK_PURPLE")
            plusCol = "\xC2\xA7"
                      "5";
          else if (r.rankPlusColor == "YELLOW")
            plusCol = "\xC2\xA7"
                      "e";
          return "\xC2\xA7"
                 "6[MVP" +
                 plusCol + "++" +
                 "\xC2\xA7"
                 "6]";
        }
        if (r.newPackageRank == "MVP_PLUS") {
          std::string plusCol = "\xC2\xA7"
                                "c";
          if (r.rankPlusColor == "GOLD")
            plusCol = "\xC2\xA7"
                      "6";
          else if (r.rankPlusColor == "AQUA")
            plusCol = "\xC2\xA7"
                      "b";
          else if (r.rankPlusColor == "GREEN")
            plusCol = "\xC2\xA7"
                      "a";
          else if (r.rankPlusColor == "LIGHT_PURPLE")
            plusCol = "\xC2\xA7"
                      "d";
          else if (r.rankPlusColor == "WHITE")
            plusCol = "\xC2\xA7"
                      "f";
          else if (r.rankPlusColor == "BLUE")
            plusCol = "\xC2\xA7"
                      "9";
          else if (r.rankPlusColor == "DARK_RED")
            plusCol = "\xC2\xA7"
                      "4";
          else if (r.rankPlusColor == "DARK_AQUA")
            plusCol = "\xC2\xA7"
                      "3";
          else if (r.rankPlusColor == "DARK_GREEN")
            plusCol = "\xC2\xA7"
                      "2";
          else if (r.rankPlusColor == "DARK_PURPLE")
            plusCol = "\xC2\xA7"
                      "5";
          else if (r.rankPlusColor == "YELLOW")
            plusCol = "\xC2\xA7"
                      "e";
          return "\xC2\xA7"
                 "b[MVP" +
                 plusCol + "+" +
                 "\xC2\xA7"
                 "b]";
        }
        if (r.newPackageRank == "MVP")
          return "\xC2\xA7"
                 "b[MVP]";
        if (r.newPackageRank == "VIP_PLUS")
          return "\xC2\xA7"
                 "a[VIP\xC2\xA7"
                 "6+\xC2\xA7"
                 "a]";
        if (r.newPackageRank == "VIP")
          return "\xC2\xA7"
                 "a[VIP]";
        return "\xC2\xA7"
               "7";
      };

      float headerH = 48.0f;
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(cardX, cy, cardW, headerH, 8.0f, THEME_CARD,
                                   0.7f * alpha);
      RenderUtils::drawRect(cardX, cy + 4, 3, headerH - 8, THEME_NAVY, alpha);
      glEnable(GL_TEXTURE_2D);

      float textOffX = 0;
      if (s_lookupSkinTexId) {
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glBindTexture(GL_TEXTURE_2D, s_lookupSkinTexId);
        glBegin(GL_QUADS);
        float sw = 32.0f;
        float sh = 32.0f;
        float sx = cardX + 10;
        float sy = cy + (headerH - sh) / 2.0f;
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(sx, sy);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(sx, sy + sh);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(sx + sw, sy + sh);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(sx + sw, sy);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        textOffX = 42.0f;
      }

      std::string rankStr = getRankDisplay();
      std::string nameWithRank = rankStr + " " + s_lookupName;
      g_guiFont.drawString(cardX + 14 + textOffX, cy + 8, nameWithRank,
                           applyAlpha(0xFFFFFFFF, alpha));

      std::string starFormatted =
          BedwarsStars::GetFormattedLevel(s_lookupResult.bedwarsStar);
      g_guiFont.drawString(cardX + 14 + textOffX, cy + 26, starFormatted,
                           applyAlpha(0xFFFFFFFF, alpha));

      std::string nlText =
          "Level " + std::to_string(s_lookupResult.networkLevel);
      g_guiFont.drawString(cardX + cardW - 120, cy + 8, nlText,
                           applyAlpha(0xFF808085, alpha), 0.45f);
      if (!s_lookupResult.uuid.empty()) {
        std::string shortUuid = s_lookupResult.uuid.substr(0, 8) + "...";
        g_guiFont.drawString(cardX + cardW - 120, cy + 22, shortUuid,
                             applyAlpha(0xFF505055, alpha), 0.4f);
      }

      cy += headerH + 8;

      struct StatEntry {
        const char *label;
        std::string value;
        uint32_t color;
      };

      double fkdr = (s_lookupResult.bedwarsFinalDeaths == 0)
                        ? (double)s_lookupResult.bedwarsFinalKills
                        : (double)s_lookupResult.bedwarsFinalKills /
                              s_lookupResult.bedwarsFinalDeaths;
      double kdr = (s_lookupResult.bedwarsDeaths == 0)
                       ? (double)s_lookupResult.bedwarsKills
                       : (double)s_lookupResult.bedwarsKills /
                             s_lookupResult.bedwarsDeaths;
      double blr = (s_lookupResult.bedwarsBedsLost == 0)
                       ? (double)s_lookupResult.bedwarsBedsBroken
                       : (double)s_lookupResult.bedwarsBedsBroken /
                             s_lookupResult.bedwarsBedsLost;
      double wlr = (s_lookupResult.bedwarsLosses == 0)
                       ? (double)s_lookupResult.bedwarsWins
                       : (double)s_lookupResult.bedwarsWins /
                             s_lookupResult.bedwarsLosses;

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

      StatEntry stats[] = {
          {"Final Kills", fmtK(s_lookupResult.bedwarsFinalKills),
           StatColors::getColor(StatColors::StatType::FinalKills,
                                s_lookupResult.bedwarsFinalKills)},
          {"FKDR", fmtR(fkdr),
           StatColors::getColor(StatColors::StatType::FKDR, fkdr)},
          {"Kills", fmtK(s_lookupResult.bedwarsKills),
           StatColors::getColor(StatColors::StatType::Kills,
                                s_lookupResult.bedwarsKills)},
          {"KDR", fmtR(kdr),
           StatColors::getColor(StatColors::StatType::KDR, kdr)},
          {"Beds Broken", fmtK(s_lookupResult.bedwarsBedsBroken),
           StatColors::getColor(StatColors::StatType::Beds,
                                s_lookupResult.bedwarsBedsBroken)},
          {"BLR", fmtR(blr),
           StatColors::getColor(StatColors::StatType::BLR, blr)},
          {"Wins", fmtK(s_lookupResult.bedwarsWins),
           StatColors::getColor(StatColors::StatType::Wins,
                                s_lookupResult.bedwarsWins)},
          {"WLR", fmtR(wlr),
           StatColors::getColor(StatColors::StatType::WLR, wlr)},
          {"Winstreak", std::to_string(s_lookupResult.winstreak),
           StatColors::getColor(StatColors::StatType::WS,
                                s_lookupResult.winstreak)},
      };
      int statCount = sizeof(stats) / sizeof(stats[0]);

      float colW = (cardW - 8) / 2.0f;
      float statRowH = 36.0f;
      int rowsNeeded = (statCount + 1) / 2;

      glDisable(GL_TEXTURE_2D);
      float gridH = rowsNeeded * statRowH + 8;
      RenderUtils::drawRoundedRect(cardX, cy, cardW, gridH, 8.0f, THEME_CARD,
                                   0.6f * alpha);
      glEnable(GL_TEXTURE_2D);

      for (int i = 0; i < statCount; i++) {
        int col = i % 2;
        int row = i / 2;
        float sx = cardX + 14 + col * colW;
        float sy = cy + 6 + row * statRowH;

        g_guiFont.drawString(sx, sy, stats[i].label,
                             applyAlpha(0xFF808085, alpha), 0.4f);
        g_guiFont.drawString(sx, sy + 14, stats[i].value,
                             applyAlpha(stats[i].color, alpha));
      }

      {
        float sx0 = cardX + 14;
        g_guiFont.drawString(
            sx0 + g_guiFont.getStringWidth(stats[0].value) + 6, cy + 6 + 14 + 2,
            "/ " + std::to_string(s_lookupResult.bedwarsFinalDeaths) + " FD",
            applyAlpha(0xFF505055, alpha), 0.35f);
        g_guiFont.drawString(
            sx0 + g_guiFont.getStringWidth(stats[2].value) + 6,
            cy + 6 + statRowH + 14 + 2,
            "/ " + std::to_string(s_lookupResult.bedwarsDeaths) + " D",
            applyAlpha(0xFF505055, alpha), 0.35f);
        g_guiFont.drawString(
            sx0 + g_guiFont.getStringWidth(stats[4].value) + 6,
            cy + 6 + 2 * statRowH + 14 + 2,
            "/ " + std::to_string(s_lookupResult.bedwarsBedsLost) + " BL",
            applyAlpha(0xFF505055, alpha), 0.35f);
        g_guiFont.drawString(
            sx0 + g_guiFont.getStringWidth(stats[6].value) + 6,
            cy + 6 + 3 * statRowH + 14 + 2,
            "/ " + std::to_string(s_lookupResult.bedwarsLosses) + " L",
            applyAlpha(0xFF505055, alpha), 0.35f);
      }

      cy += gridH + 8;

      if (Config::isTagsEnabled()) {
        std::string activeS = Config::getActiveTagService();

        bool hasUrchin = s_lookupUrchinTags &&
                         !s_lookupUrchinTags->tags.empty() &&
                         (activeS == "Urchin" || activeS == "Both");
        bool hasSeraph = s_lookupSeraphTags &&
                         !s_lookupSeraphTags->tags.empty() &&
                         (activeS == "Seraph" || activeS == "Both");

        auto measureWrappedLines = [&](const std::string &text) -> int {
          int lines = 0;
          std::string line;
          std::string word;
          std::stringstream ss(text);
          while (ss >> word) {
            if (g_guiFont.getStringWidth(line + word) > cardW - 40) {
              lines++;
              line = "";
            }
            line += (line.empty() ? "" : " ") + word;
          }
          if (!line.empty())
            lines++;
          return lines;
        };

        auto drawWrapped = [&](const std::string &text, uint32_t color,
                               float &currY) {
          std::string line;
          std::string word;
          std::stringstream ss(text);
          while (ss >> word) {
            if (g_guiFont.getStringWidth(line + word) > cardW - 40) {
              g_guiFont.drawString(cardX + 14, currY, line,
                                   applyAlpha(color, alpha));
              currY += 16.0f;
              line = "";
            }
            line += (line.empty() ? "" : " ") + word;
          }
          if (!line.empty()) {
            g_guiFont.drawString(cardX + 14, currY, line,
                                 applyAlpha(color, alpha));
            currY += 16.0f;
          }
        };

        if (hasUrchin) {
          int totalLines = 0;
          for (const auto &t : s_lookupUrchinTags->tags) {
            std::string tstr = t.type;
            if (tstr.empty())
              continue;
            std::string tagText = "[" + t.type + "]";
            if (!t.reason.empty())
              tagText += " - " + t.reason;
            totalLines += measureWrappedLines(tagText);
          }
          float tagCardH = 34.0f + totalLines * 16.0f;

          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(cardX, cy, cardW, tagCardH, 8.0f,
                                       THEME_CARD, 0.6f * alpha);
          RenderUtils::drawRect(cardX, cy + 4, 3, tagCardH - 8, 0xFF55FFFF,
                                alpha);
          glEnable(GL_TEXTURE_2D);
          g_guiFont.drawString(cardX + 14, cy + 6, "Urchin Tags",
                               applyAlpha(0xFF55FFFF, alpha), 0.45f);
          float tagY = cy + 24;
          for (const auto &t : s_lookupUrchinTags->tags) {
            std::string tagText = "\xC2\xA7"
                                  "e[" +
                                  t.type + "]";
            if (!t.reason.empty())
              tagText += " \xC2\xA7"
                         "7- " +
                         t.reason;
            drawWrapped(tagText, 0xFFCCCCCC, tagY);
          }
          cy += tagCardH + 6;
        }

        if (hasSeraph) {
          int totalLines = 0;
          for (const auto &t : s_lookupSeraphTags->tags) {
            std::string tstr = t.type;
            if (tstr.empty())
              continue;
            std::string tagText = "[" + t.type + "]";
            if (!t.reason.empty())
              tagText += " - " + t.reason;
            totalLines += measureWrappedLines(tagText);
          }
          float tagCardH = 34.0f + totalLines * 16.0f;

          glDisable(GL_TEXTURE_2D);
          RenderUtils::drawRoundedRect(cardX, cy, cardW, tagCardH, 8.0f,
                                       THEME_CARD, 0.6f * alpha);
          RenderUtils::drawRect(cardX, cy + 4, 3, tagCardH - 8, 0xFFFF5555,
                                alpha);
          glEnable(GL_TEXTURE_2D);
          g_guiFont.drawString(cardX + 14, cy + 6, "Seraph Blacklist",
                               applyAlpha(0xFFFF5555, alpha), 0.45f);
          float tagY = cy + 24;
          for (const auto &t : s_lookupSeraphTags->tags) {
            std::string tagText = "\xC2\xA7"
                                  "c[" +
                                  t.type + "]";
            if (!t.reason.empty())
              tagText += " \xC2\xA7"
                         "7- " +
                         t.reason;
            drawWrapped(tagText, 0xFFCCCCCC, tagY);
          }
          cy += tagCardH + 6;
        }
      }
    }
    cy += 20;

  } else if (s_activeTab == 2) {
    g_guiFont.drawString(cx, cy, "Tagging Services",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 40;

    bool tagsEnabled = Config::isTagsEnabled();
    bool hMasterCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD mastCol = hMasterCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 mastCol, 0.6f * alpha);
    if (hMasterCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "Enable Tags", applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Master switch for all tagging services",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    drawSwitch(10, mainX + g_w - 65, cy, tagsEnabled, hMasterCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hMasterCard) {
      Config::setTagsEnabled(!tagsEnabled);
      NotificationManager::getInstance()->add(
          "Tags", tagsEnabled ? "Tags Disabled" : "Tags Enabled",
          !tagsEnabled ? NotificationType::Success : NotificationType::Warning);
    }
    cy += 80;

    if (tagsEnabled) {
      g_guiFont.drawString(cx, cy, "Active Service",
                           applyAlpha(0xFFFFFFFF, alpha));
      cy += 30;

      std::string currentService = Config::getActiveTagService();
      const char *services[] = {"Urchin", "Seraph", "Both"};

      float dropW = 220.0f;
      float dropH = 35.0f;
      bool hovDrop = isHovered(mx, my, cx, cy, dropW, dropH);

      s_tagsDropdownAnim += (s_isTagsDropdownOpen ? 1.0f - s_tagsDropdownAnim
                                                  : 0.0f - s_tagsDropdownAnim) *
                            0.15f;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(cx, cy, dropW, dropH, 6.0f, THEME_CARD,
                                   0.8f * alpha);
      if (hovDrop)
        RenderUtils::drawRoundedRect(cx, cy + dropH - 3, dropW, 3.0f, 1.5f,
                                     THEME_NAVY, alpha);
      glEnable(GL_TEXTURE_2D);

      g_guiFont.drawString(cx + 10, cy + 6, currentService,
                           applyAlpha(0xFFFFFFFF, alpha));
      g_guiFont.drawString(cx + dropW - 20, cy + 10,
                           s_isTagsDropdownOpen ? "-" : "+",
                           applyAlpha(0xFFA0A0A5, alpha));

      if (clickEvent && hovDrop) {
        s_isTagsDropdownOpen = !s_isTagsDropdownOpen;
      }

      if (s_tagsDropdownAnim > 0.01f) {
        float listY = cy + dropH + 2;
        for (int i = 0; i < 3; ++i) {
          float itemY = listY + (i * dropH);
          bool hItem = isHovered(mx, my, cx, itemY, dropW, dropH);

          glDisable(GL_TEXTURE_2D);
          DWORD itCol = hItem ? 0xFF222226 : THEME_CARD;
          RenderUtils::drawRoundedRect(cx, itemY, dropW, dropH, 4.0f, itCol,
                                       0.95f * alpha * s_tagsDropdownAnim);
          if (hItem)
            RenderUtils::drawRect(cx, itemY, 3, dropH, THEME_NAVY,
                                  alpha * s_tagsDropdownAnim);
          glEnable(GL_TEXTURE_2D);

          g_guiFont.drawString(cx + 15, itemY + 8, services[i],
                               applyAlpha(currentService == services[i]
                                              ? 0xFFFFFFFF
                                              : 0xFFA0A0A5,
                                          alpha * s_tagsDropdownAnim));

          if (clickEvent && hItem && (s_tagsDropdownAnim > 0.8f)) {
            Config::setActiveTagService(services[i]);
            s_isTagsDropdownOpen = false;
            NotificationManager::getInstance()->add(
                "Tags", "Active service set to: " + std::string(services[i]),
                NotificationType::Info);
          }
        }
        cy += (3 * dropH) * s_tagsDropdownAnim;
      }
      cy += 50;

      g_guiFont.drawString(cx, cy, "Command Prefix",
                           applyAlpha(0xFFA0A0A5, alpha));
      cy += 20;
      RenderUtils::drawRect(cx, cy, 100, 35, THEME_CARD, 0.6f * alpha);
      if (s_typingPrefix)
        RenderUtils::drawRect(cx, cy, 2, 35, THEME_NAVY, alpha);

      std::string dispPrefix =
          s_typingPrefix ? s_prefixInput : Config::getCommandPrefix();
      if (s_typingPrefix && (GetTickCount64() / 500) % 2 == 0)
        dispPrefix += "|";

      g_guiFont.drawString(cx + 10, cy + 8, dispPrefix,
                           applyAlpha(0xFFFFFFFF, alpha));

      if (clickEvent && isHovered(mx, my, cx, cy, 100, 35)) {
        s_typingPrefix = true;
        s_typingSeraphKey = s_typingUrchinKey = s_typingSearch =
            s_typingApiKey = s_typingAutoGG = false;
        s_prefixInput = Config::getCommandPrefix();
      } else if (clickEvent && s_typingPrefix) {
        Config::setCommandPrefix(s_prefixInput);
        s_typingPrefix = false;
      }
      cy += 50;

      g_guiFont.drawString(cx, cy, "Urchin API Key",
                           applyAlpha(0xFFA0A0A5, alpha));
      cy += 20;
      RenderUtils::drawRect(cx, cy, 350, 35, THEME_CARD, 0.6f * alpha);
      if (s_typingUrchinKey)
        RenderUtils::drawRect(cx, cy, 2, 35, THEME_NAVY, alpha);
      std::string dispUrchinKey =
          s_typingUrchinKey
              ? s_urchinKeyInput
              : (Config::getUrchinApiKey().empty() ? "None (Rate-limited)"
                                                   : "********************");
      if (s_typingUrchinKey && (GetTickCount64() / 500) % 2 == 0)
        dispUrchinKey += "|";
      g_guiFont.drawString(cx + 10, cy + 8, dispUrchinKey,
                           applyAlpha(0xFFFFFFFF, alpha));
      if (clickEvent && isHovered(mx, my, cx, cy, 350, 35)) {
        s_typingUrchinKey = true;
        s_typingSeraphKey = s_typingSearch = s_typingApiKey = s_typingAutoGG =
            false;
        s_urchinKeyInput = Config::getUrchinApiKey();
      } else if (clickEvent && s_typingUrchinKey) {
        Config::setUrchinApiKey(s_urchinKeyInput);
        s_typingUrchinKey = false;
      }
      cy += 55;

      g_guiFont.drawString(cx, cy, "Seraph API Key",
                           applyAlpha(0xFFA0A0A5, alpha));
      cy += 20;
      RenderUtils::drawRect(cx, cy, 350, 35, THEME_CARD, 0.6f * alpha);
      if (s_typingSeraphKey)
        RenderUtils::drawRect(cx, cy, 2, 35, THEME_NAVY, alpha);
      std::string dispSeraphKey =
          s_typingSeraphKey
              ? s_seraphKeyInput
              : (Config::getSeraphApiKey().empty() ? "None"
                                                   : "********************");
      if (s_typingSeraphKey && (GetTickCount64() / 500) % 2 == 0)
        dispSeraphKey += "|";
      g_guiFont.drawString(cx + 10, cy + 8, dispSeraphKey,
                           applyAlpha(0xFFFFFFFF, alpha));
      if (clickEvent && isHovered(mx, my, cx, cy, 350, 35)) {
        s_typingSeraphKey = true;
        s_typingUrchinKey = s_typingSearch = s_typingApiKey = s_typingAutoGG =
            false;
        NotificationManager::getInstance()->add("Input", "Seraph Key focused",
                                                NotificationType::Info);
      } else if (clickEvent) {
        if (s_typingSeraphKey) {
          Config::setSeraphApiKey(s_seraphKeyInput);
          NotificationManager::getInstance()->add("Seraph", "API Key Saved",
                                                  NotificationType::Success);
        }
        s_typingSeraphKey = false;
      }
      cy += 70;
    }

    g_guiFont.drawString(cx, cy, "Players in Current Game",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 35;

    std::lock_guard<std::mutex> stLock(OVson::g_statsMutex);
    if (OVson::g_playerStatsMap.empty()) {
      g_guiFont.drawString(cx, cy, "No players detected in this session.",
                           applyAlpha(0xFF808085, alpha));
      cy += 30;
    } else {
      g_guiFont.drawString(cx, cy, "Player", applyAlpha(0xFFA0A0A5, alpha));
      g_guiFont.drawString(cx + 140, cy, "FK", applyAlpha(0xFFA0A0A5, alpha));
      g_guiFont.drawString(cx + 200, cy, "FKDR", applyAlpha(0xFFA0A0A5, alpha));
      g_guiFont.drawString(cx + 280, cy, "Urchin",
                           applyAlpha(0xFFA0A0A5, alpha));
      g_guiFont.drawString(cx + 420, cy, "Seraph",
                           applyAlpha(0xFFA0A0A5, alpha));
      cy += 25;

      for (const auto &pair : OVson::g_playerStatsMap) {
        const std::string &name = pair.first;
        const auto &stats = pair.second;

        bool rowHov = isHovered(mx, my, cx - 10, cy - 5, g_w - 220, 35);
        uint32_t nameCol = 0xFFFFFFFF;
        auto itT = OVson::g_playerTeamColor.find(name);
        if (itT != OVson::g_playerTeamColor.end()) {
          if (itT->second == "Red")
            nameCol = 0xFFFF5555;
          else if (itT->second == "Blue")
            nameCol = 0xFF5555FF;
          else if (itT->second == "Green")
            nameCol = 0xFF55FF55;
          else if (itT->second == "Yellow")
            nameCol = 0xFFFFFF55;
          else if (itT->second == "Pink")
            nameCol = 0xFFFF55FF;
          else if (itT->second == "Aqua")
            nameCol = 0xFF55FFFF;
        }
        g_guiFont.drawString(cx, cy, name, applyAlpha(nameCol, alpha));

        g_guiFont.drawString(cx + 140, cy,
                             std::to_string(stats.bedwarsFinalKills),
                             applyAlpha(0xFFCCCCCC, alpha));
        double fkdr =
            (stats.bedwarsFinalDeaths == 0)
                ? stats.bedwarsFinalKills
                : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
        char fBuf[16];
        sprintf_s(fBuf, "%.2f", fkdr);
        g_guiFont.drawString(cx + 200, cy, fBuf, applyAlpha(0xFFCCCCCC, alpha));

        if (Config::isTagsEnabled()) {
          std::string activeS = Config::getActiveTagService();

          if (activeS == "Urchin" || activeS == "Both") {
            auto uTagsRes = Urchin::getPlayerTags(name);
            if (uTagsRes && !uTagsRes->tags.empty()) {
              std::string tS;
              for (auto &t : uTagsRes->tags) {
                if (!tS.empty())
                  tS += ", ";
                tS += t.type;
              }
              if (tS.length() > 25)
                tS = tS.substr(0, 22) + "...";
              g_guiFont.drawString(cx + 280, cy, tS,
                                   applyAlpha(0xFFE0E0E0, alpha));
            } else
              g_guiFont.drawString(cx + 280, cy, "-",
                                   applyAlpha(0xFF505055, alpha));
          } else
            g_guiFont.drawString(cx + 280, cy, "Disabled",
                                 applyAlpha(0xFF505055, alpha));

          if (activeS == "Seraph" || activeS == "Both") {
            auto sTagsRes = Seraph::getPlayerTags(name, stats.uuid);
            if (sTagsRes && !sTagsRes->tags.empty()) {
              std::string tS;
              for (auto &t : sTagsRes->tags) {
                if (!tS.empty())
                  tS += ", ";
                tS += t.type;
              }
              if (tS.length() > 25)
                tS = tS.substr(0, 22) + "...";
              uint32_t sCol = 0xFFFF5555;
              if (tS.find("Confirmed") != std::string::npos)
                sCol = 0xFFFF55FF;
              g_guiFont.drawString(cx + 420, cy, tS, applyAlpha(sCol, alpha));
            } else
              g_guiFont.drawString(cx + 420, cy, "-",
                                   applyAlpha(0xFF505055, alpha));
          } else
            g_guiFont.drawString(cx + 420, cy, "Disabled",
                                 applyAlpha(0xFF505055, alpha));
        } else {
          g_guiFont.drawString(cx + 280, cy, "Disabled",
                               applyAlpha(0xFF505055, alpha));
        }
        cy += 35;
      }
    }
  } else if (s_activeTab == 3) {
    g_guiFont.drawString(cx, cy, "Configuration",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 55;

    g_guiFont.drawString(cx, cy, "Hypixel API Key",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 25;
    float keyW = g_w - 210;
    float keyX = mainX + 190;
    bool hKeyBox = isHovered(mx, my, keyX, cy, keyW, 35);

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(keyX, cy, keyW, 35, 6.0f, THEME_CARD,
                                 0.6f * alpha);
    if (s_typingApiKey)
      RenderUtils::drawRect(keyX, cy, 2, 35, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingApiKey)
      s_apiKeyInput = Config::getApiKey();

    std::string dispKey =
        s_typingApiKey
            ? s_apiKeyInput
            : (s_apiKeyInput.empty() ? "None" : "********************");
    if (s_typingApiKey && (GetTickCount64() / 500) % 2 == 0)
      dispKey += "|";

    g_guiFont.drawString(keyX + 10, cy + 12, dispKey,
                         applyAlpha(0xFFFFFFFF, alpha));

    if (clickEvent && isHovered(mx, my, keyX, cy, keyW, 35)) {
      s_typingApiKey = true;
      s_typingSearch = s_typingAutoGG = s_typingUrchinKey = false;
      NotificationManager::getInstance()->add("Input", "API Key focused",
                                              NotificationType::Info);
    } else if (clickEvent) {
      if (s_typingApiKey) {
        Config::setApiKey(s_apiKeyInput);
        NotificationManager::getInstance()->add("Settings", "API Key Saved",
                                                NotificationType::Success);
      }
      s_typingApiKey = false;
    }

    cy += 65;
    g_guiFont.drawString(cx, cy, "Aurora API Key",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 25;
    bool hAuroraKey = isHovered(mx, my, keyX, cy, keyW, 35);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(keyX, cy, keyW, 35, 6.0f, THEME_CARD,
                                 0.6f * alpha);
    if (s_typingAuroraApiKey)
      RenderUtils::drawRect(keyX, cy, 2, 35, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    if (!s_typingAuroraApiKey)
      s_auroraApiKeyInput = Config::getAuroraApiKey();
    std::string dispAurora =
        s_typingAuroraApiKey
            ? s_auroraApiKeyInput
            : (s_auroraApiKeyInput.empty() ? "None" : "********************");
    if (s_typingAuroraApiKey && (GetTickCount64() / 500) % 2 == 0)
      dispAurora += "|";
    g_guiFont.drawString(keyX + 10, cy + 12, dispAurora,
                         applyAlpha(0xFFFFFFFF, alpha));

    if (clickEvent && hAuroraKey) {
      s_typingAuroraApiKey = true;
      s_typingApiKey = s_typingSearch = s_typingAutoGG = s_typingUrchinKey =
          s_typingSeraphKey = false;
      NotificationManager::getInstance()->add("Input", "Aurora Key focused",
                                              NotificationType::Info);
    } else if (clickEvent && s_typingAuroraApiKey) {
      Config::setAuroraApiKey(s_auroraApiKeyInput);
      Config::save();
      NotificationManager::getInstance()->add("Settings", "Aurora Key Saved",
                                              NotificationType::Success);
      s_typingAuroraApiKey = false;
    }

    cy += 65;
    g_guiFont.drawString(cx, cy, "Ping History Mode",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 30;

    const char *pingModes[] = {"Current (Live)", "Aurora Latest",
                               "Aurora Average"};
    int currentPingMode = Config::getPingDisplayMode();

    float pDropW = 220.0f;
    float pDropH = 35.0f;
    bool hovPDrop = isHovered(mx, my, cx, cy, pDropW, pDropH);

    s_pingModeDropdownAnim +=
        (s_isPingModeDropdownOpen ? 1.0f - s_pingModeDropdownAnim
                                  : 0.0f - s_pingModeDropdownAnim) *
        0.15f;

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(cx, cy, pDropW, pDropH, 6.0f, THEME_CARD,
                                 0.8f * alpha);
    if (hovPDrop)
      RenderUtils::drawRoundedRect(cx, cy + pDropH - 3, pDropW, 3.0f, 1.5f,
                                   THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx + 10, cy + 6, pingModes[currentPingMode % 3],
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx + pDropW - 20, cy + 10,
                         s_isPingModeDropdownOpen ? "-" : "+",
                         applyAlpha(0xFFA0A0A5, alpha));

    if (clickEvent && hovPDrop)
      s_isPingModeDropdownOpen = !s_isPingModeDropdownOpen;

    if (s_pingModeDropdownAnim > 0.01f) {
      float listY = cy + pDropH + 2;
      for (int i = 0; i < 3; ++i) {
        float itemY = listY + (i * pDropH);
        bool hItem = isHovered(mx, my, cx, itemY, pDropW, pDropH);
        glDisable(GL_TEXTURE_2D);
        DWORD itCol = hItem ? 0xFF222226 : THEME_CARD;
        RenderUtils::drawRoundedRect(cx, itemY, pDropW, pDropH, 4.0f, itCol,
                                     0.95f * alpha * s_pingModeDropdownAnim);
        if (hItem)
          RenderUtils::drawRect(cx, itemY, 3, pDropH, THEME_NAVY,
                                alpha * s_pingModeDropdownAnim);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(
            cx + 15, itemY + 8, pingModes[i],
            applyAlpha(currentPingMode == i ? 0xFFFFFFFF : 0xFFA0A0A5,
                       alpha * s_pingModeDropdownAnim));
        if (clickEvent && hItem && (s_pingModeDropdownAnim > 0.8f)) {
          Config::setPingDisplayMode(i);
          s_isPingModeDropdownOpen = false;
          NotificationManager::getInstance()->add(
              "Settings", "Ping Mode: " + std::string(pingModes[i]),
              NotificationType::Info);
        }
      }
      cy += (3 * pDropH) * s_pingModeDropdownAnim;
    }

    cy += 65;
    bool keylessEnabled = Config::isKeylessModeEnabled();
    bool hKeylessCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD keylessCol = hKeylessCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 keylessCol, 0.6f * alpha);
    if (hKeylessCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "API Keyless Mode",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Fetch stats without an API key",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    drawSwitch(11, mainX + g_w - 65, cy, keylessEnabled, hKeylessCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hKeylessCard) {
      Config::setKeylessModeEnabled(!keylessEnabled);
      NotificationManager::getInstance()->add(
          "Settings",
          keylessEnabled ? "Keyless Mode Disabled" : "Keyless Mode Enabled",
          !keylessEnabled ? NotificationType::Success
                          : NotificationType::Warning);
    }

    cy += 85;
    g_guiFont.drawString(cx, cy, "AutoGG Settings",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 35;
    bool aggEnabled = Config::isAutoGGEnabled();
    bool hAggCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD aggCol = hAggCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 aggCol, 0.6f * alpha);
    if (hAggCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "AutoGG Module",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18,
                         "Automatically send a message when game ends",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float aggSwX = mainX + g_w - 65;
    drawSwitch(3, aggSwX, cy, aggEnabled, hAggCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hAggCard)
      Config::setAutoGGEnabled(!aggEnabled);

    cy += 85;
    g_guiFont.drawString(cx, cy, "Custom GG Message",
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 25;
    float ggW = g_w - 210;
    float ggX = mainX + 190;
    bool hGG = isHovered(mx, my, ggX, cy, ggW, 35);

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(ggX, cy, ggW, 35, 6.0f, THEME_CARD,
                                 0.6f * alpha);
    if (s_typingAutoGG)
      RenderUtils::drawRect(ggX, cy, 2, 35, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    if (s_autoGGInput.empty() && !s_typingAutoGG)
      s_autoGGInput = Config::getAutoGGMessage();
    std::string dispGG = s_autoGGInput;
    if (s_typingAutoGG && (GetTickCount64() / 500) % 2 == 0)
      dispGG += "|";
    if (dispGG.empty() && !s_typingAutoGG)
      dispGG = "Enter GG message...";

    g_guiFont.drawString(ggX + 10, cy + 4, dispGG,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && hGG) {
      s_typingAutoGG = true;
      s_typingApiKey = s_typingSearch = s_typingUrchinKey = false;
      NotificationManager::getInstance()->add("Input", "AutoGG message focused",
                                              NotificationType::Info);
    } else if (clickEvent) {
      if (s_typingAutoGG) {
        Config::setAutoGGMessage(s_autoGGInput);
        NotificationManager::getInstance()->add(
            "AutoGG", "Custom message saved", NotificationType::Success);
      }
      s_typingAutoGG = false;
    }

    cy += 70;
    g_guiFont.drawString(cx, cy, "Command Settings",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 30;
    bool cmdEnabled = Config::isCommandsEnabled();
    bool hCmdCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD cmdCol = hCmdCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 cmdCol, 0.6f * alpha);
    if (hCmdCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "Command Interception",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18,
                         "Enable client commands starting with '.'",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    drawSwitch(4, mainX + g_w - 65, cy, cmdEnabled, hCmdCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hCmdCard) {
      Config::setCommandsEnabled(!cmdEnabled);
      NotificationManager::getInstance()->add(
          "Settings", !cmdEnabled ? "Commands Enabled" : "Commands Disabled",
          !cmdEnabled ? NotificationType::Success : NotificationType::Warning);
    }
    cy += 75;

    g_guiFont.drawString(cx, cy, "Discord Rich Presence",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 30;
    bool discordEnabled = Config::isDiscordRpcEnabled();
    bool hDiscordCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);
    glDisable(GL_TEXTURE_2D);
    DWORD discCol = hDiscordCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 discCol, 0.6f * alpha);
    if (hDiscordCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx, cy, "Broadcasting Status",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Show your activity on Discord",
                         applyAlpha(0xFFA0A0A5, alpha));
    glDisable(GL_TEXTURE_2D);
    float discordSwX = mainX + g_w - 65;
    drawSwitch(15, discordSwX, cy, discordEnabled, hDiscordCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hDiscordCard) {
      Config::setDiscordRpcEnabled(!discordEnabled);
      NotificationManager::getInstance()->add(
          "Discord",
          discordEnabled ? "Rich Presence Disabled" : "Rich Presence Enabled",
          !discordEnabled ? NotificationType::Success
                          : NotificationType::Warning);
    }

    cy += 70;
    float saveBtnW = 160.0f;
    bool hover = isHovered(mx, my, cx, cy, saveBtnW, 35);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(cx, cy, saveBtnW, 35, 6.0f,
                                 hover ? THEME_NAVY : 0xFF2A2A2E, alpha);
    glEnable(GL_TEXTURE_2D);

    std::string saveText = "SAVE CONFIG";
    float saveTextX =
        cx + (saveBtnW - g_guiFont.getStringWidth(saveText)) / 2.0f;
    g_guiFont.drawString(saveTextX, cy + 4, saveText,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && hover) {
      Config::save();
      NotificationManager::getInstance()->add(
          "Cloud", "Settings synchronized successfully!",
          NotificationType::Success);
    }
    cy += 50;

    g_guiFont.drawString(cx, cy, "Menu Toggle Key",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 25;

    std::string keyText =
        s_waitingForKey ? "Press any key... (ESC to cancel)"
                        : ("Current: " + getKeyName(Config::getClickGuiKey()));
    if (s_waitingForKey && (GetTickCount64() / 300) % 2 == 0)
      keyText = "> " + keyText + " <";

    bool hBind = isHovered(mx, my, cx, cy, 250, 35);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(
        cx, cy, 250, 35, 6.0f,
        s_waitingForKey ? THEME_NAVY : (hBind ? 0xFF35353A : THEME_CARD),
        alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(
        cx + 20, cy + 10, keyText,
        applyAlpha(s_waitingForKey ? 0xFFFFFFFF : 0xFFA0A0A5, alpha));

    if (clickEvent && hBind && !s_waitingForKey) {
      s_waitingForKey = true;
      s_typingApiKey = s_typingSearch = false;
    }
    cy += 70;

    g_guiFont.drawString(cx, cy, "Accent Color", applyAlpha(0xFFFFFFFF, alpha));
    cy += 25;
    const DWORD presets[] = {0xFF0055A4, 0xFFD32F2F, 0xFF388E3C, 0xFFFFC107,
                             0xFF8E24AA, 0xFF00ACC1, 0xFFFF5722};
    const char *presetNames[] = {"Navy", "Ruby", "Emerald", "Gold",
                                 "Iris", "Cyan", "Flame"};
    int presetCount = sizeof(presets) / sizeof(presets[0]);
    DWORD currentTheme = Config::getThemeColor();

    float presetBoxSize = 35.0f;
    float presetGap = 10.0f;
    for (int i = 0; i < presetCount; ++i) {
      float px = cx + i * (presetBoxSize + presetGap);
      bool selected = (currentTheme == presets[i]);
      bool hPre = isHovered(mx, my, px, cy, presetBoxSize, presetBoxSize);

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(px, cy, presetBoxSize, presetBoxSize, 4.0f,
                                   presets[i], alpha);
      if (selected)
        RenderUtils::drawRoundedRect(px, cy + presetBoxSize - 3, presetBoxSize,
                                     4.0f, 2.0f, 0xFFFFFFFF, alpha);
      if (hPre && !selected)
        RenderUtils::drawRoundedRect(px, cy, presetBoxSize, 2, 2.0f, 0xFFFFFFFF,
                                     alpha * 0.6f);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hPre) {
        Config::setThemeColor(presets[i]);
        NotificationManager::getInstance()->add(
            "Theme", "Accent set to " + std::string(presetNames[i]),
            NotificationType::Info);
      }
    }
    cy += 60;

    cy += 50;
    cy += 50;
  } else if (s_activeTab == 4) {
    g_guiFont.drawString(cx, cy, "Stat Color Ranges",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 35;

    const int statCount = (int)StatColors::StatType::COUNT;
    float btnW = 55.0f;
    float btnH = 26.0f;
    float btnX = cx;
    for (int i = 0; i < statCount; ++i) {
      if ((StatColors::StatType)i == StatColors::StatType::Star) {
        if (s_colorSelectedStat == i)
          s_colorSelectedStat = 1;
        continue;
      }
      const char *sName = StatColors::getStatName((StatColors::StatType)i);
      bool sel = (s_colorSelectedStat == i);
      bool hov = isHovered(mx, my, btnX, cy, btnW, btnH);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(
          btnX, cy, btnW, btnH, 4.0f,
          sel ? THEME_NAVY : (hov ? 0xFF323236 : THEME_CARD), alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(btnX + 5, cy + 4, sName,
                           applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha),
                           0.4f);
      if (clickEvent && hov) {
        s_colorSelectedStat = i;
        s_colorPickerOpen = false;
        s_cpEditRangeIdx = -1;
      }
      btnX += btnW + 6;
      if (btnX + btnW > mainX + g_w - 30) {
        btnX = cx;
        cy += btnH + 6;
      }
    }
    cy += btnH + 20;

    auto &cfg =
        StatColors::getConfig((StatColors::StatType)s_colorSelectedStat);
    g_guiFont.drawString(cx, cy,
                         (std::string(cfg.name) + " Color Ranges:").c_str(),
                         applyAlpha(0xFFA0A0A5, alpha));
    cy += 25;

    for (int ri = 0; ri < (int)cfg.ranges.size(); ++ri) {
      const auto &r = cfg.ranges[ri];
      float rowY = cy;
      float rowW = g_w - 230;

      bool hRow = isHovered(mx, my, cx, rowY, rowW, 28);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(cx, rowY, rowW, 28, 4.0f,
                                   hRow ? 0xFF2A2A2E : THEME_CARD,
                                   0.7f * alpha);
      RenderUtils::drawRoundedRect(cx + 4, rowY + 4, 20, 20, 3.0f, r.color,
                                   alpha);
      glEnable(GL_TEXTURE_2D);

      char rangeBuf[64];
      bool isRatio = (s_colorSelectedStat == (int)StatColors::StatType::FKDR ||
                      s_colorSelectedStat == (int)StatColors::StatType::KDR ||
                      s_colorSelectedStat == (int)StatColors::StatType::WLR ||
                      s_colorSelectedStat == (int)StatColors::StatType::BLR);

      if (r.maxVal >= 1e300) {
        if (isRatio)
          snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - INF", r.minVal);
        else
          snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - INF", r.minVal);
      } else {
        if (isRatio)
          snprintf(rangeBuf, sizeof(rangeBuf), "%.2f - %.2f", r.minVal,
                   r.maxVal);
        else
          snprintf(rangeBuf, sizeof(rangeBuf), "%.0f - %.0f", r.minVal,
                   r.maxVal);
      }
      g_guiFont.drawString(cx + 30, rowY + 5, rangeBuf,
                           applyAlpha(0xFFFFFFFF, alpha), 0.4f);

      char hexBuf[12];
      snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", (r.color >> 16) & 0xFF,
               (r.color >> 8) & 0xFF, r.color & 0xFF);
      g_guiFont.drawString(cx + 160, rowY + 5, hexBuf,
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);

      const char *mcName = StatColors::rgbToMcColor(r.color);
      g_guiFont.drawString(cx + 240, rowY + 5, mcName,
                           applyAlpha(0xFF808085, alpha), 0.35f);

      float editX = cx + rowW - 65;
      bool hEdit = isHovered(mx, my, editX, rowY + 2, 32, 24);
      glDisable(GL_TEXTURE_2D);
      if (hEdit || s_cpEditRangeIdx == ri)
        RenderUtils::drawRoundedRect(editX, rowY + 2, 32, 24, 3.0f, THEME_NAVY,
                                     alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(editX + 4, rowY + 3, "Edit",
                           applyAlpha(0xFFFFFFFF, alpha), 0.35f);
      if (clickEvent && hEdit) {
        s_cpEditRangeIdx = ri;
        s_colorPickerOpen = true;
        bool isRatio =
            (s_colorSelectedStat == (int)StatColors::StatType::FKDR ||
             s_colorSelectedStat == (int)StatColors::StatType::KDR ||
             s_colorSelectedStat == (int)StatColors::StatType::WLR ||
             s_colorSelectedStat == (int)StatColors::StatType::BLR);

        if (isRatio)
          snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.2f", r.minVal);
        else
          snprintf(s_cpMinBuf, sizeof(s_cpMinBuf), "%.0f", r.minVal);

        s_cpMinLen = (int)strlen(s_cpMinBuf);
        if (r.maxVal >= 1e300) {
          s_cpMaxBuf[0] = 0;
          s_cpMaxLen = 0;
        } else {
          if (isRatio)
            snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.2f", r.maxVal);
          else
            snprintf(s_cpMaxBuf, sizeof(s_cpMaxBuf), "%.0f", r.maxVal);
          s_cpMaxLen = (int)strlen(s_cpMaxBuf);
        }
        uint8_t mr = (r.color >> 16) & 0xFF;
        uint8_t mg = (r.color >> 8) & 0xFF;
        uint8_t mb = r.color & 0xFF;
        float rf = mr / 255.0f, gf = mg / 255.0f, bf = mb / 255.0f;
        float cmax = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
        float cmin = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
        float delta = cmax - cmin;
        s_cpVal = cmax;
        s_cpSat = (cmax > 0) ? delta / cmax : 0;
        if (delta < 0.001f)
          s_cpHue = 0;
        else if (cmax == rf)
          s_cpHue = fmodf((gf - bf) / delta, 6.0f) / 6.0f;
        else if (cmax == gf)
          s_cpHue = ((bf - rf) / delta + 2.0f) / 6.0f;
        else
          s_cpHue = ((rf - gf) / delta + 4.0f) / 6.0f;
        if (s_cpHue < 0)
          s_cpHue += 1.0f;
      }

      float delX = cx + rowW - 28;
      bool hDel = isHovered(mx, my, delX, rowY + 2, 24, 24);
      glDisable(GL_TEXTURE_2D);
      if (hDel)
        RenderUtils::drawRoundedRect(delX, rowY + 2, 24, 24, 3.0f, 0xFF991111,
                                     alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(delX + 7, rowY + 3, "X",
                           applyAlpha(hDel ? 0xFFFFFFFF : 0xFFFF5555, alpha),
                           0.4f);
      if (clickEvent && hDel) {
        StatColors::removeRange((StatColors::StatType)s_colorSelectedStat, ri);
        Config::save();
        NotificationManager::getInstance()->add("Colors", "Range removed",
                                                NotificationType::Info);
        if (s_cpEditRangeIdx == ri) {
          s_cpEditRangeIdx = -1;
          s_colorPickerOpen = false;
        }
        break;
      }

      cy += 32;
    }

    cy += 15;

    float addBtnW = 160.0f;
    bool hAdd = isHovered(mx, my, cx, cy, addBtnW, 30);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(
        cx, cy, addBtnW, 30, 5.0f,
        s_colorPickerOpen ? THEME_NAVY : (hAdd ? 0xFF323236 : THEME_CARD),
        alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(cx + 10, cy + 6,
                         s_colorPickerOpen ? "- Close Picker" : "+ Add Range",
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    if (clickEvent && hAdd) {
      s_colorPickerOpen = !s_colorPickerOpen;
      s_cpEditingField = 0;
      if (!s_colorPickerOpen)
        s_cpEditRangeIdx = -1;
    }

    float rstX = cx + addBtnW + 15;
    float rstW = 140.0f;
    bool hRst = isHovered(mx, my, rstX, cy, rstW, 30);
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(rstX, cy, rstW, 30, 5.0f,
                                 hRst ? 0xFF991111 : THEME_CARD, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(rstX + 8, cy + 6, "Reset Defaults",
                         applyAlpha(0xFFFFFFFF, alpha), 0.42f);
    if (clickEvent && hRst) {
      StatColors::resetToDefaults((StatColors::StatType)s_colorSelectedStat);
      Config::save();
      NotificationManager::getInstance()->add("Colors", "Reset to defaults",
                                              NotificationType::Success);
    }
    cy += 40;

    if (s_colorPickerOpen) {
      float popX = mainX + 185;
      float popY = cy;
      float popW = g_w - 205;
      float popH = 230;

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(popX - 1, popY - 1, popW + 2, popH + 2, 8.0f,
                                   THEME_BORDER, alpha);
      RenderUtils::drawRoundedRect(popX, popY, popW, popH, 8.0f, 0xFF111113,
                                   alpha);

      float svX = popX + 12;
      float svY = popY + 12;
      float svSize = 140.0f;

      glDisable(GL_TEXTURE_2D);
      glDisable(GL_LIGHTING);
      glDisable(GL_CULL_FACE);
      glDisable(GL_ALPHA_TEST);
      glDisable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glShadeModel(GL_SMOOTH);

      {
        float hr = 1, hg = 1, hb = 1;
        float h6 = s_cpHue * 6.0f;
        int hi = (int)h6 % 6;
        float f = h6 - (int)h6;
        switch (hi) {
        case 0:
          hr = 1;
          hg = f;
          hb = 0;
          break;
        case 1:
          hr = 1 - f;
          hg = 1;
          hb = 0;
          break;
        case 2:
          hr = 0;
          hg = 1;
          hb = f;
          break;
        case 3:
          hr = 0;
          hg = 1 - f;
          hb = 1;
          break;
        case 4:
          hr = f;
          hg = 0;
          hb = 1;
          break;
        case 5:
          hr = 1;
          hg = 0;
          hb = 1 - f;
          break;
        }

        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glVertex2f(svX, svY);
        glColor4f(hr, hg, hb, alpha);
        glVertex2f(svX + svSize, svY);
        glColor4f(hr, hg, hb, alpha);
        glVertex2f(svX + svSize, svY + svSize);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glVertex2f(svX, svY + svSize);
        glEnd();
      }

      {
        glBegin(GL_QUADS);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex2f(svX, svY);
        glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
        glVertex2f(svX + svSize, svY);
        glColor4f(0.0f, 0.0f, 0.0f, alpha);
        glVertex2f(svX + svSize, svY + svSize);
        glColor4f(0.0f, 0.0f, 0.0f, alpha);
        glVertex2f(svX, svY + svSize);
        glEnd();
      }

      float cursorX = svX + s_cpSat * svSize;
      float cursorY = svY + (1.0f - s_cpVal) * svSize;
      glShadeModel(GL_FLAT);
      glColor4f(1.0f, 1.0f, 1.0f, alpha);
      glLineWidth(1.5f);
      glBegin(GL_LINE_LOOP);
      for (int a = 0; a < 16; ++a) {
        float angle = a * 6.2831853f / 16.0f;
        glVertex2f(cursorX + cosf(angle) * 4, cursorY + sinf(angle) * 4);
      }
      glEnd();
      glLineWidth(1.0f);
      glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

      if (isHovered(mx, my, svX, svY, svSize, svSize)) {
        if (lClick) {
          s_cpDraggingSV = true;
        }
      }
      if (s_cpDraggingSV) {
        if (lClick) {
          s_cpSat = (mx - svX) / svSize;
          s_cpVal = 1.0f - (my - svY) / svSize;
          if (s_cpSat < 0)
            s_cpSat = 0;
          if (s_cpSat > 1)
            s_cpSat = 1;
          if (s_cpVal < 0)
            s_cpVal = 0;
          if (s_cpVal > 1)
            s_cpVal = 1;
        } else {
          s_cpDraggingSV = false;
        }
      }

      float hueX = svX + svSize + 15;
      float hueY = svY;
      float hueW = 20.0f;
      float hueH = svSize;
      int hueSteps = 24;
      float stepH = hueH / hueSteps;
      for (int i = 0; i < hueSteps; ++i) {
        float h1 = (float)i / hueSteps;
        float h2 = (float)(i + 1) / hueSteps;
        float r1, g1, b1, r2, g2, b2;
        auto hsvRgb = [](float h, float &r, float &g, float &b) {
          float h6 = h * 6.0f;
          int hi = (int)h6 % 6;
          float f = h6 - (int)h6;
          switch (hi) {
          case 0:
            r = 1;
            g = f;
            b = 0;
            break;
          case 1:
            r = 1 - f;
            g = 1;
            b = 0;
            break;
          case 2:
            r = 0;
            g = 1;
            b = f;
            break;
          case 3:
            r = 0;
            g = 1 - f;
            b = 1;
            break;
          case 4:
            r = f;
            g = 0;
            b = 1;
            break;
          case 5:
            r = 1;
            g = 0;
            b = 1 - f;
            break;
          }
        };
        hsvRgb(h1, r1, g1, b1);
        hsvRgb(h2, r2, g2, b2);
        glBegin(GL_QUADS);
        glColor4f(r1, g1, b1, alpha);
        glVertex2f(hueX, hueY + i * stepH);
        glColor4f(r1, g1, b1, alpha);
        glVertex2f(hueX + hueW, hueY + i * stepH);
        glColor4f(r2, g2, b2, alpha);
        glVertex2f(hueX + hueW, hueY + (i + 1) * stepH);
        glColor4f(r2, g2, b2, alpha);
        glVertex2f(hueX, hueY + (i + 1) * stepH);
        glEnd();
      }

      float hueCurY = hueY + s_cpHue * hueH;
      glColor4f(1, 1, 1, alpha);
      glBegin(GL_LINES);
      glVertex2f(hueX - 2, hueCurY);
      glVertex2f(hueX + hueW + 2, hueCurY);
      glEnd();

      if (isHovered(mx, my, hueX - 4, hueY, hueW + 8, hueH) &&
          !s_cpDraggingSV) {
        if (lClick)
          s_cpDraggingHue = true;
      }
      if (s_cpDraggingHue) {
        if (lClick) {
          s_cpHue = (my - hueY) / hueH;
          if (s_cpHue < 0)
            s_cpHue = 0;
          if (s_cpHue > 0.999f)
            s_cpHue = 0.999f;
        } else {
          s_cpDraggingHue = false;
        }
      }

      glEnable(GL_TEXTURE_2D);

      float rpX = hueX + hueW + 20;
      float rpY = popY + 12;
      auto hsvToRgb32 = [](float h, float s, float v) -> uint32_t {
        float h6 = h * 6.0f;
        int hi = (int)h6 % 6;
        float f = h6 - (int)h6;
        float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
        float r, g, b;
        switch (hi) {
        case 0:
          r = v;
          g = t;
          b = p;
          break;
        case 1:
          r = q;
          g = v;
          b = p;
          break;
        case 2:
          r = p;
          g = v;
          b = t;
          break;
        case 3:
          r = p;
          g = q;
          b = v;
          break;
        case 4:
          r = t;
          g = p;
          b = v;
          break;
        default:
          r = v;
          g = p;
          b = q;
          break;
        }
        return 0xFF000000 | ((uint8_t)(r * 255) << 16) |
               ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
      };

      uint32_t previewColor = hsvToRgb32(s_cpHue, s_cpSat, s_cpVal);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(rpX, rpY, 60, 30, 4.0f, previewColor, alpha);
      glEnable(GL_TEXTURE_2D);
      char rgbBuf[32];
      snprintf(rgbBuf, sizeof(rgbBuf), "R:%d G:%d B:%d",
               (previewColor >> 16) & 0xFF, (previewColor >> 8) & 0xFF,
               previewColor & 0xFF);
      g_guiFont.drawString(rpX, rpY + 38, rgbBuf, applyAlpha(0xFFA0A0A5, alpha),
                           0.38f);
      char hexBuf2[12];
      snprintf(hexBuf2, sizeof(hexBuf2), "#%02X%02X%02X",
               (previewColor >> 16) & 0xFF, (previewColor >> 8) & 0xFF,
               previewColor & 0xFF);
      g_guiFont.drawString(rpX + 70, rpY + 8, hexBuf2,
                           applyAlpha(0xFFFFFFFF, alpha), 0.42f);
      rpY += 58;
      g_guiFont.drawString(rpX, rpY,
                           "MC Colors:", applyAlpha(0xFF808085, alpha), 0.38f);
      rpY += 18;
      struct McPreset {
        const char *name;
        uint32_t color;
      };
      McPreset mcPresets[] = {
          {"0", 0xFF000000}, {"1", 0xFF0000AA}, {"2", 0xFF00AA00},
          {"3", 0xFF00AAAA}, {"4", 0xFFAA0000}, {"5", 0xFFAA00AA},
          {"6", 0xFFFFAA00}, {"7", 0xFFAAAAAA}, {"8", 0xFF555555},
          {"9", 0xFF5555FF}, {"a", 0xFF55FF55}, {"b", 0xFF55FFFF},
          {"c", 0xFFFF5555}, {"d", 0xFFFF55FF}, {"e", 0xFFFFFF55},
          {"f", 0xFFFFFFFF},
      };
      float mcX = rpX;
      for (int i = 0; i < 16; ++i) {
        bool hMc = isHovered(mx, my, mcX, rpY, 14, 14);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(mcX, rpY, 14, 14, 2.0f, mcPresets[i].color,
                                     alpha);
        if (hMc)
          RenderUtils::drawRoundedRect(mcX - 1, rpY - 1, 16, 16, 2.0f,
                                       0xFFFFFFFF, 0.5f * alpha);
        glEnable(GL_TEXTURE_2D);
        if (clickEvent && hMc) {
          uint8_t mr = (mcPresets[i].color >> 16) & 0xFF;
          uint8_t mg = (mcPresets[i].color >> 8) & 0xFF;
          uint8_t mb = mcPresets[i].color & 0xFF;
          float rf = mr / 255.0f, gf = mg / 255.0f, bf = mb / 255.0f;
          float cmax =
              (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
          float cmin =
              (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
          float delta = cmax - cmin;
          s_cpVal = cmax;
          s_cpSat = (cmax > 0) ? delta / cmax : 0;
          if (delta < 0.001f)
            s_cpHue = 0;
          else if (cmax == rf)
            s_cpHue = fmodf((gf - bf) / delta, 6.0f) / 6.0f;
          else if (cmax == gf)
            s_cpHue = ((bf - rf) / delta + 2.0f) / 6.0f;
          else
            s_cpHue = ((rf - gf) / delta + 4.0f) / 6.0f;
          if (s_cpHue < 0)
            s_cpHue += 1.0f;
        }
        mcX += 17;
        if (i == 7) {
          mcX = rpX;
          rpY += 17;
        }
      }

      rpY += 22;

      bool showCursor = (GetTickCount64() / 500) % 2 == 0;

      g_guiFont.drawString(rpX, rpY, "Min:", applyAlpha(0xFFA0A0A5, alpha),
                           0.38f);
      float minBoxX = rpX + 30;
      bool hMinBox = isHovered(mx, my, minBoxX, rpY - 3, 55, 20);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(
          minBoxX, rpY - 3, 55, 20, 3.0f,
          s_cpEditingField == 1 ? 0xFF2A2A2E : THEME_CARD, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(minBoxX + 4, rpY - 1, s_cpMinBuf,
                           applyAlpha(0xFFFFFFFF, alpha), 0.38f);
      if (s_cpEditingField == 1 && showCursor) {
        float tw = (g_guiFont.getStringWidth(s_cpMinBuf) / 0.5f) * 0.38f;
        glDisable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, alpha);
        glBegin(GL_LINES);
        glVertex2f(minBoxX + 4 + tw, rpY - 1);
        glVertex2f(minBoxX + 4 + tw, rpY + 13);
        glEnd();
        glEnable(GL_TEXTURE_2D);
      }
      if (clickEvent && hMinBox)
        s_cpEditingField = 1;

      g_guiFont.drawString(rpX + 95, rpY, "Max:", applyAlpha(0xFFA0A0A5, alpha),
                           0.38f);
      float maxBoxX = rpX + 125;
      bool hMaxBox = isHovered(mx, my, maxBoxX, rpY - 3, 55, 20);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(
          maxBoxX, rpY - 3, 55, 20, 3.0f,
          s_cpEditingField == 2 ? 0xFF2A2A2E : THEME_CARD, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(maxBoxX + 4, rpY - 1, s_cpMaxBuf,
                           applyAlpha(0xFFFFFFFF, alpha), 0.38f);
      if (s_cpEditingField == 2 && showCursor) {
        float tw = (g_guiFont.getStringWidth(s_cpMaxBuf) / 0.5f) * 0.38f;
        glDisable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, alpha);
        glBegin(GL_LINES);
        glVertex2f(maxBoxX + 4 + tw, rpY - 1);
        glVertex2f(maxBoxX + 4 + tw, rpY + 13);
        glEnd();
        glEnable(GL_TEXTURE_2D);
      }
      if (clickEvent && hMaxBox)
        s_cpEditingField = 2;

      if (clickEvent && !hMinBox && !hMaxBox)
        s_cpEditingField = 0;

      rpY += 28;

      float addW2 = 120.0f;
      bool hAdd2 = isHovered(mx, my, rpX, rpY, addW2, 26);
      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(rpX, rpY, addW2, 26, 4.0f,
                                   hAdd2 ? THEME_NAVY : 0xFF2A2A2E, alpha);
      glEnable(GL_TEXTURE_2D);
      g_guiFont.drawString(rpX + 12, rpY + 4,
                           s_cpEditRangeIdx >= 0 ? "Save Changes" : "Add Range",
                           applyAlpha(0xFFFFFFFF, alpha), 0.4f);

      if (clickEvent && hAdd2) {
        double minV = atof(s_cpMinBuf);
        double maxV = atof(s_cpMaxBuf);
        if (maxV <= 0 || strlen(s_cpMaxBuf) == 0)
          maxV = 1e308;

        bool success = false;
        if (s_cpEditRangeIdx >= 0) {
          success = StatColors::updateRange(
              (StatColors::StatType)s_colorSelectedStat, s_cpEditRangeIdx, minV,
              maxV, previewColor);
        } else {
          success =
              StatColors::addRange((StatColors::StatType)s_colorSelectedStat,
                                   minV, maxV, previewColor);
        }

        if (success) {
          Config::save();
          NotificationManager::getInstance()->add(
              "Colors",
              s_cpEditRangeIdx >= 0 ? "Range updated!" : "Range added!",
              NotificationType::Success);
          s_cpEditRangeIdx = -1;
          if (s_cpEditRangeIdx >= 0)
            s_colorPickerOpen = false;
        } else {
          NotificationManager::getInstance()->add(
              "Colors", "Overlap! Check existing ranges.",
              NotificationType::Error);
        }
      }

      cy += popH + 10;
    }

    cy += 30;
  } else if (s_activeTab == 5) {
    g_guiFont.drawString(cx, cy, "Debug Console Settings",
                         applyAlpha(0xFFFFFFFF, alpha));
    cy += 40;

    bool dbgGlobal = Config::isGlobalDebugEnabled();
    bool hDbgCard = isHovered(mx, my, mainX + 190, cy - 10, g_w - 210, 60);

    glDisable(GL_TEXTURE_2D);
    DWORD dbgCol = hDbgCard ? 0xFF222226 : THEME_CARD;
    RenderUtils::drawRoundedRect(mainX + 190, cy - 10, g_w - 210, 60, 6.0f,
                                 dbgCol, 0.6f * alpha);
    if (hDbgCard)
      RenderUtils::drawRect(mainX + 190, cy - 10, 3, 60, THEME_NAVY, alpha);
    glEnable(GL_TEXTURE_2D);

    g_guiFont.drawString(cx, cy, "Master Debug Switch",
                         applyAlpha(0xFFFFFFFF, alpha));
    g_guiFont.drawString(cx, cy + 18, "Master toggle for all client debug logs",
                         applyAlpha(0xFFA0A0A5, alpha));

    float dbgSwX = mainX + g_w - 65;
    glDisable(GL_TEXTURE_2D);
    drawSwitch(5, dbgSwX, cy, dbgGlobal, hDbgCard, alpha);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hDbgCard)
      Config::setGlobalDebugEnabled(!dbgGlobal);

    cy += 75;

    if (Config::isGlobalDebugEnabled()) {
      auto renderDebugToggle = [&](const char *title, int id,
                                   Config::DebugCategory cat) {
        bool enabled = Config::isDebugEnabled(cat);
        bool hov = isHovered(mx, my, cx, cy - 5, 240, 30);

        g_guiFont.drawString(cx, cy, title,
                             applyAlpha(hov ? 0xFFFFFFFF : 0xFF808085, alpha));
        float toggleX = cx + 180;

        glDisable(GL_TEXTURE_2D);
        drawSwitch(id, toggleX, cy - 5, enabled, hov, alpha);
        glEnable(GL_TEXTURE_2D);

        if (clickEvent && hov)
          Config::setDebugEnabled(cat, !enabled);
        cy += 35;
      };

      renderDebugToggle("Game Detection", 7,
                        Config::DebugCategory::GameDetection);
      renderDebugToggle("Bed Detection", 8,
                        Config::DebugCategory::BedDetection);
      renderDebugToggle("Urchin Service", 9, Config::DebugCategory::Urchin);
      renderDebugToggle("Bed Defense Sys", 10,
                        Config::DebugCategory::BedDefense);
      renderDebugToggle("GUI Internals", 11, Config::DebugCategory::GUI);
      renderDebugToggle("General / Other", 12, Config::DebugCategory::General);
      cy += 15;
    }

    g_guiFont.drawString(cx, cy, "Logs are sent to OutputDebugString",
                         applyAlpha(0xFF808085, alpha));
    cy += 20;
    g_guiFont.drawString(cx, cy, "Use DbgView to see live output.",
                         applyAlpha(0xFF808085, alpha));
    cy += 40;

    glDisable(GL_TEXTURE_2D);
    bool hTest = isHovered(mx, my, cx, cy, 200, 35);
    RenderUtils::drawRoundedRect(cx, cy, 200, 35, 6.0f,
                                 hTest ? THEME_NAVY : THEME_CARD, alpha);
    glEnable(GL_TEXTURE_2D);

    std::string testText = "SEND TEST TOAST";
    float testTextX = cx + (200.0f - g_guiFont.getStringWidth(testText)) / 2.0f;
    g_guiFont.drawString(testTextX, cy + 4, testText,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && hTest) {
      NotificationManager::getInstance()->add(
          "System", "Toast notifications are working!",
          NotificationType::Success);
    }
    cy += 50;

    glDisable(GL_TEXTURE_2D);
    bool hClear = isHovered(mx, my, cx, cy, 200, 35);
    RenderUtils::drawRoundedRect(cx, cy, 200, 35, 6.0f,
                                 hClear ? 0xFF991111 : THEME_CARD, alpha);
    glEnable(GL_TEXTURE_2D);

    std::string clearText = "CLEAR CACHE";
    float clearTextX =
        cx + (200.0f - g_guiFont.getStringWidth(clearText)) / 2.0f;
    g_guiFont.drawString(clearTextX, cy + 4, clearText,
                         applyAlpha(0xFFFFFFFF, alpha));
    if (clickEvent && hClear) {
      OVson::clearAllCaches();
      NotificationManager::getInstance()->add(
          "System", "All caches cleared and reset!", NotificationType::Success);
    }
    cy += 50;
  }

  float contentHeight = (cy + s_scrollOffset) - startCy;
  float visibleHeight = g_h - 100.0f;
  s_maxScroll = (contentHeight > visibleHeight)
                    ? (contentHeight - visibleHeight + 40.0f)
                    : 0.0f;

  glDisable(GL_SCISSOR_TEST);

  glPopMatrix();

  glPopAttrib();
  glPopMatrix();
}
} // namespace Render