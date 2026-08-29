#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ClickGUI.h"
#include "ClickGUI_Bridge.h"
#include "State.h"
#include "Theme.h"
#include "Helpers.h"
#include "Tabs/Tabs.h"
#include "../Render/RenderUtils.h"
#include "../Render/RenderHook.h"
#include "LiquidGlass.h"
#include "../Render/NotificationManager.h"
#include "../Render/StatsOverlay.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Logic/Bedwars/BedwarsConfig.h"
#include "../Logic/Bedwars/BedwarsCore.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"
#include "../Render/BedwarsOverlay.h"
#include "../Logic/BlockHitSound.h"
#include "../Config/Config.h"
#include "../Utils/GlGuard.h"
#include "../Utils/SensitivityFix.h"
#include "../Utils/Timer.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <gl/GL.h>

namespace Render {

using namespace ClickGUIState;
using ClickGUITheme::Style;
using ClickGUITheme::style;
using ClickGUITheme::accent;
using ClickGUITheme::panelRadius;
using ClickGUITheme::textPrimary;
using ClickGUITheme::textSecondary;
using ClickGUITheme::textMuted;
using ClickGUITheme::easeOut;

static std::string getServerIp() {
  static std::string cached = "Singleplayer";
  static ULONGLONG last = 0;
  ULONGLONG now = GetTickCount64();
  if (now - last < 1000) return cached;
  last = now;
  JNIEnv *env = lc ? lc->getEnv() : nullptr;
  if (!env) return cached;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) return cached;
  static jfieldID f_theMc = nullptr;
  if (!f_theMc) {
    f_theMc = lc->GetStaticFieldID(
        mcCls, "theMinecraft", "Lnet/minecraft/client/Minecraft;",
        "field_71432_P", "S", "Lave;");
    if (!f_theMc && env->ExceptionCheck()) env->ExceptionClear();
  }
  if (!f_theMc) return cached;
  jobject mc = env->GetStaticObjectField(mcCls, f_theMc);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!mc) return cached;
  static jfieldID f_csd = nullptr;
  if (!f_csd) {
    f_csd = lc->GetFieldID(
        mcCls, "currentServerData",
        "Lnet/minecraft/client/multiplayer/ServerData;", "field_71422_O",
        "Q", "Lbha;");
    if (!f_csd && env->ExceptionCheck()) env->ExceptionClear();
  }
  if (!f_csd) { env->DeleteLocalRef(mc); return cached; }
  jobject sd = env->GetObjectField(mc, f_csd);
  env->DeleteLocalRef(mc);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!sd) { cached = "Singleplayer"; return cached; }
  jclass sdCls = env->GetObjectClass(sd);
  static jfieldID f_ip = nullptr;
  if (!f_ip) {
    f_ip = lc->GetFieldID(sdCls, "serverIP", "Ljava/lang/String;",
                          "field_78845_b", "b", "Ljava/lang/String;");
    if (!f_ip && env->ExceptionCheck()) env->ExceptionClear();
  }
  if (f_ip) {
    jstring jip = (jstring)env->GetObjectField(sd, f_ip);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (jip) {
      const char *c = env->GetStringUTFChars(jip, nullptr);
      if (c && *c) { cached = c; }
      if (c) env->ReleaseStringUTFChars(jip, c);
      env->DeleteLocalRef(jip);
    }
  }
  env->DeleteLocalRef(sdCls);
  env->DeleteLocalRef(sd);
  return cached;
}

static void drawTabIcon(int idx, float x, float y, float s, DWORD col,
                        float a) {
  float r = ((col >> 16) & 0xFF) / 255.0f;
  float g = ((col >> 8) & 0xFF) / 255.0f;
  float b = (col & 0xFF) / 255.0f;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glLineWidth(1.5f);
  glColor4f(r, g, b, a);
  float cx = x + s * 0.5f, cy = y + s * 0.5f;
  auto ring = [&](float X, float Y, float rad) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 18; ++i) {
      float an = i * 6.2831853f / 18.0f;
      glVertex2f(X + cosf(an) * rad, Y + sinf(an) * rad);
    }
    glEnd();
  };
  auto disc = [&](float X, float Y, float rad) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(X, Y);
    for (int i = 0; i <= 18; ++i) {
      float an = i * 6.2831853f / 18.0f;
      glVertex2f(X + cosf(an) * rad, Y + sinf(an) * rad);
    }
    glEnd();
  };
  switch (idx) {
  case 0:
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i <= 12; ++i) {
      float t = -1.0f + 2.0f * i / 12.0f;
      glVertex2f(cx + t * s * 0.44f, cy - (1.0f - t * t) * s * 0.26f);
    }
    for (int i = 0; i <= 12; ++i) {
      float t = 1.0f - 2.0f * i / 12.0f;
      glVertex2f(cx + t * s * 0.44f, cy + (1.0f - t * t) * s * 0.26f);
    }
    glEnd();
    disc(cx, cy, s * 0.12f);
    break;
  case 1:
    ring(cx - s * 0.17f, cy - s * 0.13f, s * 0.12f);
    ring(cx + s * 0.17f, cy - s * 0.13f, s * 0.12f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(cx - s * 0.36f, cy + s * 0.32f);
    glVertex2f(cx - s * 0.30f, cy + s * 0.08f);
    glVertex2f(cx + s * 0.30f, cy + s * 0.08f);
    glVertex2f(cx + s * 0.36f, cy + s * 0.32f);
    glEnd();
    break;
  case 2:
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx + s * 0.02f, cy - s * 0.36f);
    glVertex2f(cx + s * 0.38f, cy);
    glVertex2f(cx + s * 0.02f, cy + s * 0.36f);
    glVertex2f(cx - s * 0.34f, cy);
    glEnd();
    disc(cx - s * 0.10f, cy - s * 0.10f, s * 0.05f);
    break;
  case 3:
    ring(cx, cy, s * 0.18f);
    glBegin(GL_LINES);
    for (int i = 0; i < 8; ++i) {
      float an = i * 6.2831853f / 8.0f;
      glVertex2f(cx + cosf(an) * s * 0.22f, cy + sinf(an) * s * 0.22f);
      glVertex2f(cx + cosf(an) * s * 0.34f, cy + sinf(an) * s * 0.34f);
    }
    glEnd();
    break;
  case 4:
    ring(cx, cy, s * 0.34f);
    disc(cx - s * 0.14f, cy - s * 0.06f, s * 0.06f);
    disc(cx + s * 0.12f, cy - s * 0.12f, s * 0.06f);
    disc(cx + s * 0.06f, cy + s * 0.14f, s * 0.06f);
    break;
  case 5:
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - s * 0.36f, cy - s * 0.28f);
    glVertex2f(cx + s * 0.36f, cy - s * 0.28f);
    glVertex2f(cx + s * 0.36f, cy + s * 0.16f);
    glVertex2f(cx - s * 0.36f, cy + s * 0.16f);
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(cx, cy + s * 0.16f);
    glVertex2f(cx, cy + s * 0.30f);
    glVertex2f(cx - s * 0.14f, cy + s * 0.30f);
    glVertex2f(cx + s * 0.14f, cy + s * 0.30f);
    glEnd();
    break;
  case 6:
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    glVertex2f(cx - s * 0.28f, cy + s * 0.30f);
    glVertex2f(cx + s * 0.12f, cy - s * 0.10f);
    glEnd();
    ring(cx + s * 0.20f, cy - s * 0.20f, s * 0.13f);
    break;
  case 8:
    // A bed, read left to right: pillow, mattress, blanket seam, two legs.
    // Bedwars and Plugins both used to fall through to the two-squares default
    // glyph, so the tab that matters most in this client had no icon of its own.
    glBegin(GL_LINE_LOOP);  // mattress
    glVertex2f(cx - s * 0.40f, cy - s * 0.02f);
    glVertex2f(cx + s * 0.40f, cy - s * 0.02f);
    glVertex2f(cx + s * 0.40f, cy + s * 0.20f);
    glVertex2f(cx - s * 0.40f, cy + s * 0.20f);
    glEnd();
    glBegin(GL_LINE_LOOP);  // pillow
    glVertex2f(cx - s * 0.34f, cy - s * 0.24f);
    glVertex2f(cx - s * 0.06f, cy - s * 0.24f);
    glVertex2f(cx - s * 0.06f, cy - s * 0.02f);
    glVertex2f(cx - s * 0.34f, cy - s * 0.02f);
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(cx + s * 0.04f, cy - s * 0.02f);  // blanket seam
    glVertex2f(cx + s * 0.04f, cy + s * 0.20f);
    glVertex2f(cx - s * 0.34f, cy + s * 0.20f);  // legs
    glVertex2f(cx - s * 0.34f, cy + s * 0.34f);
    glVertex2f(cx + s * 0.34f, cy + s * 0.20f);
    glVertex2f(cx + s * 0.34f, cy + s * 0.34f);
    glEnd();
    break;
  default:
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - s * 0.30f, cy - s * 0.30f);
    glVertex2f(cx + s * 0.05f, cy - s * 0.30f);
    glVertex2f(cx + s * 0.05f, cy + s * 0.05f);
    glVertex2f(cx - s * 0.30f, cy + s * 0.05f);
    glEnd();
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - s * 0.05f, cy - s * 0.05f);
    glVertex2f(cx + s * 0.30f, cy - s * 0.05f);
    glVertex2f(cx + s * 0.30f, cy + s * 0.30f);
    glVertex2f(cx - s * 0.05f, cy + s * 0.30f);
    glEnd();
    disc(cx, cy, s * 0.08f);
    break;
  }
  glLineWidth(1.0f);
  glEnable(GL_TEXTURE_2D);
}

static void drawCaret(float cx, float cy, float s, bool open, uint32_t col, float alpha) {
  float r = ((col >> 16) & 0xFF) / 255.0f;
  float g = ((col >> 8) & 0xFF) / 255.0f;
  float b = (col & 0xFF) / 255.0f;
  float a = (((col >> 24) & 0xFF) / 255.0f) * alpha;
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4f(r, g, b, a);
  glBegin(GL_TRIANGLES);
  if (open) {  // pointing up
    glVertex2f(cx - s, cy + s * 0.4f);
    glVertex2f(cx + s, cy + s * 0.4f);
    glVertex2f(cx, cy - s * 0.6f);
  } else {     // pointing down
    glVertex2f(cx - s, cy - s * 0.4f);
    glVertex2f(cx + s, cy - s * 0.4f);
    glVertex2f(cx, cy + s * 0.6f);
  }
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

enum class LbKind { Toggle, Slider, Choice, Label, Input, Action };
struct LbSub {
  const char *name;
  LbKind kind = LbKind::Toggle;
  std::function<bool()> bget;
  std::function<void(bool)> bset;
  std::function<float()> fget;
  std::function<void(float)> fset;
  float fmin = 0.0f, fmax = 1.0f;
  const char *unit = "";
  bool pct = false;
  std::vector<const char *> choices;
  std::function<const char *()> cget;
  std::function<void(const char *)> cset;
  bool open = false;
  bool *typingStatePtr = nullptr;
  std::string *inputBufPtr = nullptr;
  std::function<std::string()> sget;
  std::function<void(const std::string &)> sset;
  std::function<std::string()> labelGet;
  std::function<void()> action;
  bool isPassword = false;
};
struct LbModule {
  const char *name;
  std::function<bool()> get;
  std::function<void(bool)> set;
  std::vector<LbSub> subs;   // right-click to reveal
  bool expanded = false;     // sub-settings shown?
  // Bedwars children expose their saved preference separately so the legacy
  // layout can render them inactive while the master gate is off without
  // making that saved preference impossible to turn back off.
  std::function<bool()> savedGet;
};
struct LbWindow {
  const char *title;
  float x, y;
  bool open;
  std::vector<LbModule> mods;
  float scrollOffset = 0.0f;
  float targetScroll = 0.0f;
  float maxScroll = 0.0f;
};

static bool getNameTagStat(const std::string &key) {
  auto slots = Config::getNameTagStats();
  for (const auto &p : slots) {
    if (p.first == key) return p.second;
  }
  return false;
}
static void setNameTagStat(const std::string &key, bool val) {
  auto slots = Config::getNameTagStats();
  for (auto &p : slots) {
    if (p.first == key) {
      p.second = val;
      break;
    }
  }
  Config::setNameTagStats(slots);
}

static std::vector<LbWindow> s_lbWins;
static ULONGLONG s_bedwarsResetArmedAt = 0;
static bool s_lbInit = false;

static LbSub subToggle(const char *n, std::function<bool()> g,
                       std::function<void(bool)> s) {
  LbSub sb; sb.name = n; sb.kind = LbKind::Toggle; sb.bget = g; sb.bset = s;
  return sb;
}
static LbSub subSlider(const char *n, std::function<float()> g,
                       std::function<void(float)> s, float mn, float mx,
                       const char *unit = "", bool pct = false) {
  LbSub sb; sb.name = n; sb.kind = LbKind::Slider; sb.fget = g; sb.fset = s;
  sb.fmin = mn; sb.fmax = mx; sb.unit = unit; sb.pct = pct; return sb;
}
static LbSub subChoice(const char *n, std::vector<const char *> opts,
                       std::function<const char *()> g,
                       std::function<void(const char *)> s) {
  LbSub sb; sb.name = n; sb.kind = LbKind::Choice; sb.choices = std::move(opts);
  sb.cget = g; sb.cset = s; return sb;
}
static LbSub subLabel(const char *n) {
  LbSub sb; sb.name = n; sb.kind = LbKind::Label; sb.open = true; return sb;
}
static LbSub subDynamicLabel(const char *n,
                             std::function<std::string()> get) {
  LbSub sb = subLabel(n);
  sb.labelGet = std::move(get);
  return sb;
}
static LbSub subInput(const char *n, bool *typingState, std::string *buf,
                      std::function<std::string()> g,
                      std::function<void(const std::string &)> s,
                      bool isPwd = false) {
  LbSub sb;
  sb.name = n;
  sb.kind = LbKind::Input;
  sb.typingStatePtr = typingState;
  sb.inputBufPtr = buf;
  sb.sget = g;
  sb.sset = s;
  sb.isPassword = isPwd;
  return sb;
}
static LbSub subAction(const char *n, std::function<void()> action,
                       std::function<std::string()> value = {}) {
  LbSub sb;
  sb.name = n;
  sb.kind = LbKind::Action;
  sb.action = std::move(action);
  sb.sget = std::move(value);
  return sb;
}

static std::string shortBlockHitFilename() {
  const std::string &filename = Config::getBlockHitSoundFilename();
  constexpr std::size_t kMaximumDisplayLength = 20U;
  if (filename.size() <= kMaximumDisplayLength) return filename;
  return filename.substr(0U, kMaximumDisplayLength - 3U) + "...";
}

static const float kDdOptH = 26.0f;  // dropdown option row height

static float lbSubH(const LbSub &s) {
  switch (s.kind) {
    case LbKind::Label:  return 26.0f;
    case LbKind::Slider: return 52.0f;
    case LbKind::Choice:
      return 60.0f + (s.open ? (float)s.choices.size() * kDdOptH : 0.0f);
    case LbKind::Input:  return 60.0f;
    default:             return 32.0f;
  }
}

static float lbWinHeight(const LbWindow &w, float titleH, float rowH) {
  float wh = titleH;
  if (w.open) {
    for (const auto &m : w.mods) {
      wh += rowH;
      if (m.expanded) {
        wh += 18.0f;  // group padding (6 top + 12 bottom)
        bool showSub = true;
        for (const auto &s : m.subs) {
          if (s.kind == LbKind::Label) {
            wh += lbSubH(s);
            showSub = s.open;
          } else if (showSub) {
            wh += lbSubH(s);
          }
        }
      }
    }
    wh += 6.0f;
  }
  return wh;
}

static void saveLayoutB(const std::vector<LbWindow> &wins) {
  std::string s;
  for (const auto &w : wins) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%.0f,%.0f,%d;", w.x, w.y, w.open ? 1 : 0);
    s += buf;
  }
  Config::setLayoutBData(s);  // writes config.json
}
static void loadLayoutB(std::vector<LbWindow> &wins) {
  const std::string &s = Config::getLayoutBData();
  if (s.empty()) return;
  const std::size_t storedCount =
      static_cast<std::size_t>(std::count(s.begin(), s.end(), ';'));
  const bool legacyWithoutBedwars = storedCount + 1 == wins.size();
  size_t i = 0, wi = 0;
  while (i < s.size() && wi < wins.size()) {
    size_t semi = s.find(';', i);
    if (semi == std::string::npos) break;
    float x, y; int o;
    if (sscanf(s.substr(i, semi - i).c_str(), "%f,%f,%d", &x, &y, &o) == 3) {
      const std::size_t target =
          legacyWithoutBedwars && wi >= 2 ? wi + 1 : wi;
      if (target < wins.size()) {
        wins[target].x = x;
        wins[target].y = y;
        wins[target].open = (o != 0);
      }
    }
    i = semi + 1;
    ++wi;
  }
}

static void ensureLbWindows() {
  if (s_lbInit) return;
  s_lbInit = true;
    {
      LbWindow w{"VISUALS", 30, 44, true, {}};
      w.mods.push_back({"Stats Overlay", &::StatsOverlay::isEnabled,
                        &::StatsOverlay::setEnabled,
                        {subLabel("COLUMNS"),
                         subToggle("Star", &Config::isOvShowStar, &Config::setOvShowStar),
                         subToggle("Final Kills", &Config::isOvShowFk, &Config::setOvShowFk),
                         subToggle("FKDR", &Config::isOvShowFkdr, &Config::setOvShowFkdr),
                         subToggle("Wins", &Config::isOvShowWins, &Config::setOvShowWins),
                         subToggle("WLR", &Config::isOvShowWlr, &Config::setOvShowWlr),
                         subToggle("Winstreak", &Config::isOvShowWs, &Config::setOvShowWs),
                         subToggle("Kills", &Config::isOvShowKills, &Config::setOvShowKills),
                         subToggle("KDR", &Config::isOvShowKdr, &Config::setOvShowKdr),
                         subToggle("Beds", &Config::isOvShowBeds, &Config::setOvShowBeds),
                         subToggle("BLR", &Config::isOvShowBlr, &Config::setOvShowBlr),
                         subToggle("Ping", &Config::isOvShowPing, &Config::setOvShowPing),
                         subToggle("Tags", &Config::isOvShowTags, &Config::setOvShowTags),
                         subLabel("TABLE"),
                         subChoice("Sort By",
                            {"Team", "Star", "FK", "FKDR", "Wins", "WLR", "WS"},
                            [] () -> const char * { return Config::getSortMode().c_str(); },
                            [](const char *v) { Config::setSortMode(v); }),
                         subChoice("Tab Display",
                            {"fk", "fkdr", "wins", "wlr", "ws"},
                            [] () -> const char * { return Config::getTabDisplayMode().c_str(); },
                            [](const char *v) { Config::setTabDisplayMode(v); }),
                         subChoice("Order", {"Ascending", "Descending"},
                            [] () -> const char * { return Config::isTabSortDescending() ? "Descending" : "Ascending"; },
                            [](const char *v) { Config::setTabSortDescending(strcmp(v, "Descending") == 0); })}});
      w.mods.push_back({"NameTags", &Config::isNameTagsEnabled,
                        &Config::setNameTagsEnabled,
                        {subSlider("Height", &Config::getNameTagHeight,
                                   &Config::setNameTagHeight, 0.5f, 4.0f, "m"),
                         subLabel("COLUMNS"),
                         subToggle("Star", []{ return getNameTagStat("star"); }, [](bool b){ setNameTagStat("star", b); }),
                         subToggle("FKDR", []{ return getNameTagStat("fkdr"); }, [](bool b){ setNameTagStat("fkdr", b); }),
                         subToggle("Final Kills", []{ return getNameTagStat("fk"); }, [](bool b){ setNameTagStat("fk", b); }),
                         subToggle("Wins", []{ return getNameTagStat("wins"); }, [](bool b){ setNameTagStat("wins", b); }),
                         subToggle("WLR", []{ return getNameTagStat("wlr"); }, [](bool b){ setNameTagStat("wlr", b); }),
                         subToggle("Winstreak", []{ return getNameTagStat("ws"); }, [](bool b){ setNameTagStat("ws", b); })}});
      w.mods.push_back({"Motion Blur", &Config::isMotionBlurEnabled,
                        &Config::setMotionBlurEnabled,
                        {subSlider("Intensity", &Config::getMotionBlurAmount,
                                   &Config::setMotionBlurAmount, 0.0f, 1.0f, "", true)}});
      w.mods.push_back({"Tab List", &Config::isTabEnabled,
                        &Config::setTabEnabled,
                        {subToggle("BetterTab", &Config::isBetterTabModeEnabled,
                                   &Config::setBetterTabModeEnabled),
                         subLabel("COLUMNS"),
                         subToggle("Star", &Config::isProShowStar, &Config::setProShowStar),
                         subToggle("Final Kills", &Config::isProShowFk, &Config::setProShowFk),
                         subToggle("FKDR", &Config::isProShowFkdr, &Config::setProShowFkdr),
                         subToggle("Wins", &Config::isProShowWins, &Config::setProShowWins),
                         subToggle("WLR", &Config::isProShowWlr, &Config::setProShowWlr),
                         subToggle("Winstreak", &Config::isProShowWs, &Config::setProShowWs),
                         subToggle("Kills", &Config::isProShowKills, &Config::setProShowKills),
                         subToggle("KDR", &Config::isProShowKdr, &Config::setProShowKdr),
                         subToggle("Beds", &Config::isProShowBeds, &Config::setProShowBeds),
                         subToggle("BLR", &Config::isProShowBlr, &Config::setProShowBlr),
                         subToggle("Ping", &Config::isProShowPing, &Config::setProShowPing),
                         subToggle("Tags", &Config::isProShowTags, &Config::setProShowTags),
                         subToggle("Health", &Config::isProShowHp, &Config::setProShowHp)}});
      w.mods.push_back({"Pre-Game Chat Stats", &Config::isPreGameChatStatsEnabled,
                        &Config::setPreGameChatStatsEnabled, {}});
      w.mods.push_back({"Team Stats Report", &Config::isTeamReportEnabled,
                        &Config::setTeamReportEnabled,
                        {subChoice("Channel", {"/pc", "/ac", "/shout"},
                                   [] () -> const char * { return Config::getTeamReportChannel().c_str(); },
                                   [](const char *v) { Config::setTeamReportChannel(v); })}});
      w.mods.push_back({"Tech Overlay", &Config::isTechEnabled,
                        &Config::setTechEnabled, {}});
      w.mods.push_back({"Notifications", &Config::isNotificationsEnabled,
                        &Config::setNotificationsEnabled, {}});
      w.mods.push_back({"Lobby Mention Stats", &Config::isLobbyMentionStatsEnabled,
                        &Config::setLobbyMentionStatsEnabled, {}});
      s_lbWins.push_back(std::move(w));
    }
    {
      LbWindow w{"UTILS", 264, 44, true, {}};
      w.mods.push_back({"Bed Defense", &Config::isBedDefenseEnabled,
                        [](bool b) {
                          Config::setBedDefenseEnabled(b);
                          auto *bd = BedDefense::BedDefenseManager::getInstance();
                          if (b) bd->enable(); else bd->disable();
                        }, {}});
      w.mods.push_back(
          {"Block-Hit Sound (Client Heuristic)", &Config::isBlockHitSoundEnabled,
           &Config::setBlockHitSoundEnabled,
           {subChoice("Sound Source", {"Default", "Custom"},
                      [] { return Config::getBlockHitSoundSource().c_str(); },
                      [](const char *value) {
                        Config::setBlockHitSoundSource(value);
                      }),
            subSlider("Volume", &Config::getBlockHitSoundVolume,
                      &Config::setBlockHitSoundVolume, 0.0f, 100.0f, "%"),
            subAction("Next WAV", &BlockHitSound::requestSelectNextCustomSound,
                      &shortBlockHitFilename),
            subAction("Reload WAV", &BlockHitSound::requestCustomSoundReload),
            subAction("Preview", &BlockHitSound::requestPreview),
            subAction("Open sounds folder", [] {
              if (!BlockHitSound::openSoundsDirectory()) {
                NotificationManager::getInstance()->add(
                    "Block-Hit Sound", "Could not open the sounds folder",
                    NotificationType::Warning);
              }
            }),
            subToggle("Debug trigger/rejection reasons",
                      &Config::isBlockHitSoundDebugEnabled,
                      &Config::setBlockHitSoundDebugEnabled)}});
      w.mods.push_back({"Anticheat", &Config::isAnticheatEnabled,
                        &Config::setAnticheatEnabled,
                        {subToggle("NoSlow", &Config::isAnticheatNoSlowEnabled,
                                   &Config::setAnticheatNoSlowEnabled),
                         subToggle("AutoBlock", &Config::isAnticheatAutoBlockEnabled,
                                   &Config::setAnticheatAutoBlockEnabled),
                         subToggle("Eagle", &Config::isAnticheatEagleEnabled,
                                   &Config::setAnticheatEagleEnabled),
                         subToggle("Scaffold", &Config::isAnticheatScaffoldEnabled,
                                   &Config::setAnticheatScaffoldEnabled),
                         subToggle("Check Self", &Config::isAnticheatCheckSelfEnabled,
                                   &Config::setAnticheatCheckSelfEnabled)}});
      w.mods.push_back({"Chat Bypass", &Config::isChatBypasserEnabled,
                        &Config::setChatBypasserEnabled,
                        {subToggle("Smart Mode", &Config::isSmartChatBypassEnabled,
                                   &Config::setSmartChatBypassEnabled)}});
      w.mods.push_back({"Direct UUID", &Config::isNickedBypass,
                        &Config::setNickedBypass, {}});
      w.mods.push_back({"Number Denicker", &Config::isNumberDenickerEnabled,
                        &Config::setNumberDenickerEnabled, {}});
      s_lbWins.push_back(std::move(w));
    }
    {
      namespace BwConfig = OVson::Bedwars::Configuration;
      using OVson::Bedwars::Module;
      using OVson::Bedwars::HudId;
      using OVson::Bedwars::HudLayout;
      using OVson::Bedwars::VisibilityMode;
      using OVson::Bedwars::AlertOutput;
      LbWindow w{"Bedwars", 30, 360, true, {}};
      w.mods.push_back({"Bedwars Tools", &BwConfig::isMasterEnabled,
                        &BwConfig::setMasterEnabled,
                        {subToggle("Alert Sounds", &BwConfig::areSoundsEnabled,
                                   &BwConfig::setSoundsEnabled),
                         subToggle("Debug Diagnostics", &BwConfig::isDebugEnabled,
                                   &BwConfig::setDebugEnabled),
                         subToggle("Arm Settings Reset", [] { return false; },
                                   [](bool enabled) {
                                     if (!enabled) return;
                                     const ULONGLONG now = GetTickCount64();
                                     if (s_bedwarsResetArmedAt != 0 &&
                                         now - s_bedwarsResetArmedAt < 8000) {
                                       BwConfig::save(BwConfig::Settings{});
                                       s_bedwarsResetArmedAt = 0;
                                     } else {
                                       s_bedwarsResetArmedAt = now;
                                     }
                                   }),
                         subDynamicLabel("Reset confirmation", [] {
                           const ULONGLONG now = GetTickCount64();
                           return s_bedwarsResetArmedAt != 0 &&
                                          now - s_bedwarsResetArmedAt < 8000
                                      ? std::string("Click reset again within 8 seconds")
                                      : std::string("Reset requires two clicks within 8 seconds");
                         })}});

      w.mods.push_back({
          "Player Alert Settings", [] {
            return BwConfig::isMasterEnabled();
          }, [](bool) {},
          {subChoice(
               "Visibility Mode", {"Range Only", "Line of Sight", "Camera View"},
               []() -> const char * {
                 switch (BwConfig::getVisibilityMode()) {
                 case VisibilityMode::RangeOnly: return "Range Only";
                 case VisibilityMode::CameraView: return "Camera View";
                 default: return "Line of Sight";
                 }
               },
               [](const char *value) {
                 if (strcmp(value, "Range Only") == 0)
                   BwConfig::setVisibilityMode(VisibilityMode::RangeOnly);
                 else if (strcmp(value, "Camera View") == 0)
                   BwConfig::setVisibilityMode(VisibilityMode::CameraView);
                 else
                   BwConfig::setVisibilityMode(VisibilityMode::LineOfSight);
               }),
           subChoice(
               "Alert Output", {"Alert", "Chat", "Chat + Alert"},
               []() -> const char * {
                 return OVson::Bedwars::alertOutputName(
                     BwConfig::getAlertOutput());
               },
               [](const char *value) {
                 if (strcmp(value, "Chat") == 0)
                   BwConfig::setAlertOutput(AlertOutput::Chat);
                 else if (strcmp(value, "Chat + Alert") == 0)
                   BwConfig::setAlertOutput(AlertOutput::Both);
                 else
                   BwConfig::setAlertOutput(AlertOutput::Overlay);
               }),
           subSlider("Player Range", &BwConfig::getPlayerAlertRange,
                     &BwConfig::setPlayerAlertRange, 4.0f, 256.0f, "m")}});

      w.mods.push_back({"Notification Sounds", &BwConfig::areSoundsEnabled,
                        &BwConfig::setSoundsEnabled});

      LbModule hudSettings{
          "HUD and Layout",
          [] { return Render::BedwarsOverlay::isLayoutMode(); },
          [](bool enabled) { Render::BedwarsOverlay::setLayoutMode(enabled); },
          {}};
      for (std::size_t hudIndex = 0;
           hudIndex < OVson::Bedwars::kHudCount; ++hudIndex) {
        const HudId hud = static_cast<HudId>(hudIndex);
        hudSettings.subs.push_back(subLabel(OVson::Bedwars::hudName(hud)));
        hudSettings.subs.push_back(subToggle(
            "Visible",
            [hud] { return BwConfig::getHudLayout(hud).visible; },
            [hud](bool enabled) {
              HudLayout layout = BwConfig::getHudLayout(hud);
              layout.visible = enabled;
              BwConfig::setHudLayout(hud, layout);
            }));
        hudSettings.subs.push_back(subSlider(
            "X", [hud] { return BwConfig::getHudLayout(hud).x; },
            [hud](float value) {
              HudLayout layout = BwConfig::getHudLayout(hud);
              layout.x = value;
              BwConfig::setHudLayout(hud, layout);
            },
            0.0f, 1.0f, "", true));
        hudSettings.subs.push_back(subSlider(
            "Y", [hud] { return BwConfig::getHudLayout(hud).y; },
            [hud](float value) {
              HudLayout layout = BwConfig::getHudLayout(hud);
              layout.y = value;
              BwConfig::setHudLayout(hud, layout);
            },
            0.0f, 1.0f, "", true));
        hudSettings.subs.push_back(subSlider(
            "Scale", [hud] { return BwConfig::getHudLayout(hud).scale; },
            [hud](float value) {
              HudLayout layout = BwConfig::getHudLayout(hud);
              layout.scale = value;
              BwConfig::setHudLayout(hud, layout);
            },
            0.5f, 2.5f));
        hudSettings.subs.push_back(subToggle(
            "Reset Position", [] { return false; },
            [hud](bool enabled) {
              if (enabled) BwConfig::resetHudLayout(hud);
            }));
      }
      hudSettings.subs.push_back(subLabel("All HUDs"));
      hudSettings.subs.push_back(subToggle(
          "Reset All Positions", [] { return false; }, [](bool enabled) {
            if (enabled) BwConfig::resetAllHudLayouts();
          }));
      w.mods.push_back(std::move(hudSettings));

      for (std::size_t i = 0; i < OVson::Bedwars::kModuleCount; ++i) {
        const Module module = static_cast<Module>(i);
        LbModule item;
        item.name = OVson::Bedwars::moduleName(module);
        item.get = [module] {
          return OVson::Bedwars::isModuleAvailable(module) &&
                 BwConfig::isMasterEnabled() &&
                 BwConfig::isModuleEnabled(module);
        };
        item.savedGet = [module] { return BwConfig::isModuleEnabled(module); };
        item.set = [module](bool enabled) {
          if (OVson::Bedwars::isModuleAvailable(module))
            BwConfig::setModuleEnabled(module, enabled);
        };
        if (!OVson::Bedwars::isModuleAvailable(module)) {
          item.subs.push_back(subLabel("Unavailable: missing safe hook"));
        } else if (module == Module::EventTimers) {
          item.subs.push_back(subToggle("Only Next Event",
                                        &BwConfig::isOnlyNextEvent,
                                        &BwConfig::setOnlyNextEvent));
          item.subs.push_back(subSlider("HUD X", &BwConfig::getTimerX,
                                        &BwConfig::setTimerX, 0.0f, 1.0f, "",
                                        true));
          item.subs.push_back(subSlider("HUD Y", &BwConfig::getTimerY,
                                        &BwConfig::setTimerY, 0.0f, 1.0f, "",
                                        true));
          item.subs.push_back(subSlider("Scale", &BwConfig::getTimerScale,
                                        &BwConfig::setTimerScale, 0.5f, 2.5f));
        } else if (module == Module::HeightOverlay) {
          item.subs.push_back(subSlider("HUD X", &BwConfig::getHeightX,
                                        &BwConfig::setHeightX, 0.0f, 1.0f, "",
                                        true));
          item.subs.push_back(subSlider("HUD Y", &BwConfig::getHeightY,
                                        &BwConfig::setHeightY, 0.0f, 1.0f, "",
                                        true));
          item.subs.push_back(subSlider("Scale", &BwConfig::getHeightScale,
                                        &BwConfig::setHeightScale, 0.5f, 2.5f));
          item.subs.push_back(subSlider(
              "Limit Override",
              [] { return (float)BwConfig::getHeightLimitOverride(); },
              [](float value) {
                BwConfig::setHeightLimitOverride((int)(value + 0.5f));
              },
              0.0f, 256.0f));
        } else if (module == Module::TrapNotifier) {
          item.subs.push_back(subSlider(
              "Reminder Interval",
              [] { return (float)BwConfig::getTrapReminderSeconds(); },
              [](float value) {
                BwConfig::setTrapReminderSeconds((int)(value + 0.5f));
              },
              15.0f, 600.0f, "s"));
        } else if (module == Module::ResourceTracker) {
          item.subs.push_back(subToggle("Resource HUD",
                                        &BwConfig::isResourceHudEnabled,
                                        &BwConfig::setResourceHudEnabled));
          for (std::size_t resourceIndex = 0;
               resourceIndex < OVson::Bedwars::kResourceCount;
               ++resourceIndex) {
            const auto resource =
                static_cast<OVson::Bedwars::Resource>(resourceIndex);
            item.subs.push_back(subToggle(
                OVson::Bedwars::resourceName(resource),
                [resource] { return BwConfig::isResourceEnabled(resource); },
                [resource](bool enabled) {
                  BwConfig::setResourceEnabled(resource, enabled);
                }));
          }
        }
        w.mods.push_back(std::move(item));
      }
      s_lbWins.push_back(std::move(w));
    }
    {
      LbWindow w{"SETTINGS", 498, 44, true, {}};
      w.mods.push_back({"Auto GG", &Config::isAutoGGEnabled,
                        &Config::setAutoGGEnabled,
                        {subInput("Custom GG Message", &s_typingAutoGG, &s_autoGGInput,
                                  [] { return Config::getAutoGGMessage(); },
                                  [](const std::string &v) { Config::setAutoGGMessage(v); })}});
      w.mods.push_back({"Commands", &Config::isCommandsEnabled,
                        &Config::setCommandsEnabled, {}});
      w.mods.push_back({"Discord RPC", &Config::isDiscordRpcEnabled,
                        &Config::setDiscordRpcEnabled, {}});
      w.mods.push_back({"Keyless Mode", &Config::isKeylessModeEnabled,
                        &Config::setKeylessModeEnabled, {}});
      w.mods.push_back({"Hypixel API Key", [] { return !Config::getApiKey().empty() && Config::getApiKey() != "None"; },
                        [](bool) {},
                        {subInput("API Key", &s_typingApiKey, &s_apiKeyInput,
                                  [] { return Config::getApiKey(); },
                                  [](const std::string &v) { Config::setApiKey(v); }, true)}});
      w.mods.push_back({"Aurora API Key", [] { return !Config::getAuroraApiKey().empty() && Config::getAuroraApiKey() != "None"; },
                        [](bool) {},
                        {subInput("API Key", &s_typingAuroraApiKey, &s_auroraApiKeyInput,
                                  [] { return Config::getAuroraApiKey(); },
                                  [](const std::string &v) { Config::setAuroraApiKey(v); Config::save(); }, true)}});
      w.mods.push_back({"Glass Wiggle", &Config::isLiquidGlassWiggleEnabled,
                        &Config::setLiquidGlassWiggleEnabled, {}});
      w.mods.push_back({"Glass Glow", &Config::isLiquidGlassGlowEnabled,
                        &Config::setLiquidGlassGlowEnabled, {}});
      w.mods.push_back({"Ping Display", []{ return true; }, [](bool){},
                        {subChoice("Mode", {"Current (Live)", "Aurora Latest", "Aurora Average"},
                                   [] () -> const char * {
                                     int mode = Config::getPingDisplayMode();
                                     if (mode == 1) return "Aurora Latest";
                                     if (mode == 2) return "Aurora Average";
                                     return "Current (Live)";
                                   },
                                   [](const char *v) {
                                     if (strcmp(v, "Aurora Latest") == 0) Config::setPingDisplayMode(1);
                                     else if (strcmp(v, "Aurora Average") == 0) Config::setPingDisplayMode(2);
                                     else Config::setPingDisplayMode(0);
                                   })}});
      w.mods.push_back({"Accent Color", []{ return true; }, [](bool){},
                        {subChoice("Preset", {"Navy", "Ruby", "Emerald", "Gold", "Iris", "Cyan", "Flame"},
                                   [] () -> const char * {
                                     DWORD currentTheme = Config::getThemeColor();
                                     if (currentTheme == 0xFFD32F2F) return "Ruby";
                                     if (currentTheme == 0xFF388E3C) return "Emerald";
                                     if (currentTheme == 0xFFFFC107) return "Gold";
                                     if (currentTheme == 0xFF8E24AA) return "Iris";
                                     if (currentTheme == 0xFF00ACC1) return "Cyan";
                                     if (currentTheme == 0xFFFF5722) return "Flame";
                                     return "Navy";
                                   },
                                   [](const char *v) {
                                     if (strcmp(v, "Ruby") == 0) Config::setThemeColor(0xFFD32F2F);
                                     else if (strcmp(v, "Emerald") == 0) Config::setThemeColor(0xFF388E3C);
                                     else if (strcmp(v, "Gold") == 0) Config::setThemeColor(0xFFFFC107);
                                     else if (strcmp(v, "Iris") == 0) Config::setThemeColor(0xFF8E24AA);
                                     else if (strcmp(v, "Cyan") == 0) Config::setThemeColor(0xFF00ACC1);
                                     else if (strcmp(v, "Flame") == 0) Config::setThemeColor(0xFFFF5722);
                                     else Config::setThemeColor(0xFF0055A4);
                                   })}});
      w.mods.push_back({"Glass Theme", []{ return Config::getClickGuiTheme() == "LiquidGlass"; },
                        [](bool b) { Config::setClickGuiTheme(b ? "LiquidGlass" : "Solid"); },
                        {subSlider("Refraction", &Config::getLiquidGlassRefractStrength,
                                   &Config::setLiquidGlassRefractStrength, 0.0f, 1.0f),
                         subSlider("Edge Width", &Config::getLiquidGlassEdgeWidth,
                                   &Config::setLiquidGlassEdgeWidth, 0.0f, 1.0f),
                         subSlider("Card Edge", &Config::getLiquidGlassCardEdgeWidth,
                                   &Config::setLiquidGlassCardEdgeWidth, 0.0f, 1.0f),
                         subSlider("Opacity", &Config::getLiquidGlassDarkness,
                                   &Config::setLiquidGlassDarkness, 0.0f, 1.0f)}});
      w.mods.push_back({"Chroma Accent", []{ return Config::isChromaEnabled(); },
                        [](bool b) { Config::setChromaEnabled(b); },
                        {subSlider("Chroma Speed", []{ return Config::getChromaSpeed(); },
                                   [](float v) { Config::setChromaSpeed(v); }, 10.0f, 180.0f, "/s")}});
      w.mods.push_back({"GUI Keybind", []{ return false; },
                        [](bool) { ClickGUIState::s_waitingForKey = true; }, {}});
      s_lbWins.push_back(std::move(w));
    }
    {
      LbWindow w{"TAGS", 732, 44, true, {}};
      w.mods.push_back({"Enable Tags", &Config::isTagsEnabled,
                        &Config::setTagsEnabled,
                        {subChoice("Active Service", {"Urchin", "Seraph", "Both", "Khadow"},
                                   [] () -> const char * { return Config::getActiveTagService().c_str(); },
                                   [](const char *v) { Config::setActiveTagService(v); }),
                         subInput("Command Prefix", &s_typingPrefix, &s_prefixInput,
                                  [] { return Config::getCommandPrefix(); },
                                  [](const std::string &v) { Config::setCommandPrefix(v); })}});
      w.mods.push_back({"Urchin", &Config::isUrchinEnabled,
                        &Config::setUrchinEnabled,
                        {subInput("Urchin API Key", &s_typingUrchinKey, &s_urchinKeyInput,
                                  [] { return Config::getUrchinApiKey(); },
                                  [](const std::string &v) { Config::setUrchinApiKey(v); }, true)}});
      w.mods.push_back({"Seraph", &Config::isSeraphEnabled,
                        &Config::setSeraphEnabled,
                        {subInput("Seraph API Key", &s_typingSeraphKey, &s_seraphKeyInput,
                                  [] { return Config::getSeraphApiKey(); },
                                  [](const std::string &v) { Config::setSeraphApiKey(v); }, true)}});
      s_lbWins.push_back(std::move(w));
    }
    {
      using DC = Config::DebugCategory;
      LbWindow w{"DEBUG", 732, 232, true, {}};
      w.mods.push_back({"Master Debug", &Config::isGlobalDebugEnabled,
                        &Config::setGlobalDebugEnabled,
                        {subToggle("General",
                            [] { return Config::isDebugEnabled(DC::General); },
                            [](bool b) { Config::setDebugEnabled(DC::General, b); }),
                         subToggle("Game Detect",
                            [] { return Config::isDebugEnabled(DC::GameDetection); },
                            [](bool b) { Config::setDebugEnabled(DC::GameDetection, b); }),
                         subToggle("Bed Detect",
                            [] { return Config::isDebugEnabled(DC::BedDetection); },
                            [](bool b) { Config::setDebugEnabled(DC::BedDetection, b); }),
                         subToggle("Urchin",
                            [] { return Config::isDebugEnabled(DC::Urchin); },
                            [](bool b) { Config::setDebugEnabled(DC::Urchin, b); }),
                         subToggle("Seraph",
                            [] { return Config::isDebugEnabled(DC::Seraph); },
                            [](bool b) { Config::setDebugEnabled(DC::Seraph, b); }),
                         subToggle("GUI",
                            [] { return Config::isDebugEnabled(DC::GUI); },
                            [](bool b) { Config::setDebugEnabled(DC::GUI, b); }),
                         subToggle("Bed Defense",
                            [] { return Config::isDebugEnabled(DC::BedDefense); },
                            [](bool b) { Config::setDebugEnabled(DC::BedDefense, b); })}});
      s_lbWins.push_back(std::move(w));
    }

    [&]() {
      const auto &javaMods = ClickGUIBridge::getCachedModules();
      for (const auto &mod : javaMods) {
        LbWindow *targetWin = nullptr;
        for (auto &w : s_lbWins) {
          if (w.title && std::string(w.title) == mod.category) {
            targetWin = &w;
            break;
          }
        }

        if (!targetWin) {
          float nx = 100.0f + s_lbWins.size() * 60.0f;
          float ny = 100.0f;
          if (nx > 900.0f) nx = 100.0f;
          
          LbWindow nw;
          nw.title = mod.category.c_str();
          nw.x = nx;
          nw.y = ny;
          nw.open = true;
          s_lbWins.push_back(std::move(nw));
          targetWin = &s_lbWins.back();
        }

        LbModule lm;
        lm.name = mod.name.c_str();
        lm.get = [moduleObj = mod.moduleObj]() {
          return ClickGUIBridge::getToggleValue(moduleObj);
        };
        lm.set = [moduleObj = mod.moduleObj](bool b) {
          ClickGUIBridge::setModuleEnabled(moduleObj, b);
        };

        for (const auto &set : mod.settings) {
          LbSub sub;
          sub.name = set.name.c_str();
          if (set.kind == "toggle") {
            sub.kind = LbKind::Toggle;
            sub.bget = [settingObj = set.settingObj]() {
              return ClickGUIBridge::getToggleValue(settingObj);
            };
            sub.bset = [settingObj = set.settingObj](bool b) {
              ClickGUIBridge::setToggleValue(settingObj, b);
            };
          } else if (set.kind == "slider") {
            sub.kind = LbKind::Slider;
            sub.fmin = set.minVal;
            sub.fmax = set.maxVal;
            sub.fget = [settingObj = set.settingObj]() {
              return ClickGUIBridge::getSliderValue(settingObj);
            };
            sub.fset = [settingObj = set.settingObj](float f) {
              ClickGUIBridge::setSliderValue(settingObj, f);
            };
          } else if (set.kind == "choice") {
            sub.kind = LbKind::Choice;
            for (const auto &ch : set.choices) {
              sub.choices.push_back(ch.c_str());
            }
            sub.cget = [settingObj = set.settingObj]() -> const char* {
              static thread_local std::string s_val;
              s_val = ClickGUIBridge::getChoiceValue(settingObj);
              return s_val.c_str();
            };
            sub.cset = [settingObj = set.settingObj](const char* s) {
              ClickGUIBridge::setChoiceValue(settingObj, s);
            };
          } else if (set.kind == "input") {
            sub.kind = LbKind::Input;
            sub.typingStatePtr = const_cast<bool*>(&set.typingState);
            sub.inputBufPtr = const_cast<std::string*>(&set.inputBuf);
            sub.sget = [settingObj = set.settingObj]() {
              return ClickGUIBridge::getInputValue(settingObj);
            };
            sub.sset = [settingObj = set.settingObj](const std::string &s) {
              ClickGUIBridge::setInputValue(settingObj, s);
            };
          }
          lm.subs.push_back(sub);
        }
        targetWin->mods.push_back(lm);
      }
    }();

    loadLayoutB(s_lbWins);  // restore saved window positions
}

static void renderLayoutB(float mx, float my, bool lClick, bool clickEvent,
                          bool rClickEvent, float sw, float sh) {
  using namespace ClickGUITheme;
  ensureLbWindows();

  static int s_dragWin = -1;
  static float s_dragOX = 0, s_dragOY = 0;
  static int s_bringFront = -1;
  static int s_prevDrag = -1;
  const float ww = 252.0f, titleH = 42.0f, rowH = 38.0f;

  if (!lClick) s_dragWin = -1;
  if (s_prevDrag >= 0 && s_dragWin < 0) saveLayoutB(s_lbWins);
  s_prevDrag = s_dragWin;

  if (s_bringFront >= 0 && s_bringFront < (int)s_lbWins.size()) {
    LbWindow w = s_lbWins[s_bringFront];
    s_lbWins.erase(s_lbWins.begin() + s_bringFront);
    s_lbWins.push_back(w);
    if (s_dragWin == s_bringFront)
      s_dragWin = (int)s_lbWins.size() - 1;
    else if (s_dragWin > s_bringFront)
      s_dragWin--;
    s_bringFront = -1;
  }

  int hitWin = -1;
  for (int wi = (int)s_lbWins.size() - 1; wi >= 0 && hitWin < 0; --wi) {
    LbWindow &w = s_lbWins[wi];
    float wh = lbWinHeight(w, titleH, rowH);
    float drawnH = (wh > 580.0f && w.open) ? 580.0f : wh;
    if (isHovered(mx, my, w.x, w.y, ww, drawnH)) hitWin = wi;
  }

  for (size_t wi = 0; wi < s_lbWins.size(); ++wi) {
    LbWindow &win = s_lbWins[wi];
    bool isHit = ((int)wi == hitWin);
    float wh = lbWinHeight(win, titleH, rowH);

    float drawnH = wh;
    const float maxWinH = 580.0f;
    bool needsScroll = (wh > maxWinH && win.open);
    if (needsScroll) drawnH = maxWinH;

    float contentHeight = wh - titleH;
    float visibleHeight = drawnH - titleH;
    win.maxScroll = (contentHeight > visibleHeight) ? (contentHeight - visibleHeight + 10.0f) : 0.0f;

    if (win.targetScroll > win.maxScroll) win.targetScroll = win.maxScroll;
    if (win.targetScroll < 0.0f) win.targetScroll = 0.0f;
    win.scrollOffset += (win.targetScroll - win.scrollOffset) * 0.18f;

    if (clickEvent && isHit) s_bringFront = (int)wi;

    if (clickEvent && isHit && s_dragWin < 0 &&
        isHovered(mx, my, win.x, win.y, ww - 30, titleH)) {
      s_dragWin = (int)wi;
      s_dragOX = mx - win.x;
      s_dragOY = my - win.y;
    }
    if (s_dragWin == (int)wi && lClick) {
      win.x = mx - s_dragOX;
      win.y = my - s_dragOY;
    }

    float A = s_animAlpha;  // fade everything with the open animation
    glDisable(GL_TEXTURE_2D);
    drawThemePanel(win.x, win.y, ww, drawnH, A);
    glDisable(GL_TEXTURE_2D);

    if (win.open) {
      DWORD dv = hairline();
      RenderUtils::drawRect(win.x + 1.0f, win.y + titleH - 1.0f, ww - 2.0f, 1.0f,
                            dv, (((dv >> 24) & 0xFF) / 255.0f) * A);
    }
    RenderUtils::drawCircle(win.x + 15.0f, win.y + titleH * 0.5f, 3.5f, accent(), A);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(win.x + 28.0f, win.y + titleH * 0.5f - 6.0f, win.title,
                         applyAlpha(textPrimary(), s_animAlpha), 0.52f);
    drawCaret(win.x + ww - 16.0f, win.y + titleH * 0.5f, 3.5f, win.open,
              textMuted(), s_animAlpha);

    if (clickEvent && isHit && isHovered(mx, my, win.x + ww - 30, win.y, 30, titleH))
      win.open = !win.open;

    if (win.open) {
      glEnable(GL_SCISSOR_TEST);
      glScissor((int)win.x, (int)(sh - (win.y + drawnH)), (int)ww, (int)(drawnH - titleH));

      float ry = win.y + titleH - win.scrollOffset;
      for (size_t mi = 0; mi < win.mods.size(); ++mi) {
        LbModule &m = win.mods[mi];
        if (strcmp(m.name, "GUI Keybind") == 0 ||
            strncmp(m.name, "Bind: ", 6) == 0 ||
            strcmp(m.name, "Press a key...") == 0) {
          static char keyBindBuf[64] = "";
          if (ClickGUIState::s_waitingForKey) {
            m.name = "Press a key...";
          } else {
            snprintf(keyBindBuf, sizeof(keyBindBuf), "Bind: %s", ClickGUI::getKeyName(Config::getClickGuiKey()).c_str());
            m.name = keyBindBuf;
          }
        }
        bool on = m.get();
        bool hasSubs = !m.subs.empty();

        bool inVisibleArea = (my >= win.y + titleH && my <= win.y + drawnH);
        bool hov = inVisibleArea && isHovered(mx, my, win.x, ry, ww, rowH) && isHit;

        static std::unordered_map<int, float> s_rowHov;
        int rk = (int)(win.x * 2) * 100003 + (int)(ry * 2);
        float &rhv = s_rowHov[rk];
        rhv += ((hov ? 1.0f : 0.0f) - rhv) * 0.2f;
        glDisable(GL_TEXTURE_2D);
        if (m.expanded) {
          DWORD eb = surface2();
          RenderUtils::drawRoundedRect(win.x + 4, ry + 1, ww - 8, rowH - 2, 6.0f,
                                       eb, (((eb >> 24) & 0xFF) / 255.0f) * A);
          RenderUtils::drawRoundedOutline(win.x + 4, ry + 1, ww - 8, rowH - 2,
                                          6.0f, 1.0f, accent(), 0.32f * A);
        } else if (rhv > 0.01f) {
          DWORD hb = surface2();
          RenderUtils::drawRoundedRect(win.x + 4, ry + 1, ww - 8, rowH - 2, 6.0f,
                                       hb,
                                       (((hb >> 24) & 0xFF) / 255.0f) * 0.7f *
                                           rhv * A);
        }
        float dcx = win.x + ww - 20.0f, dcy = ry + rowH * 0.5f;
        if (on) {
          RenderUtils::drawCircle(dcx, dcy, 3.0f, accent(), A);
        } else {
          RenderUtils::drawCircle(dcx, dcy, 3.0f, 0xFF505058, A);
        }

        if (hasSubs)
          drawChevron(win.x + ww - 36, dcy, 4.0f, m.expanded,
                      m.expanded ? accent() : textMuted(), s_animAlpha);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(win.x + 16, ry + rowH * 0.5f - 6.5f, m.name,
                             applyAlpha(on ? textPrimary() : textSecondary(),
                                        s_animAlpha), 0.5f);
        if (clickEvent && isHit && hov && s_dragWin < 0) {
          const bool toggleValue = m.savedGet ? m.savedGet() : on;
          m.set(!toggleValue);
        }
        if (rClickEvent && isHit && hov && hasSubs)
          m.expanded = !m.expanded;
        ry += rowH;
        if (!m.expanded) continue;

        float blockTop = ry;

        float subsSum = 0.0f;
        bool sectionVisible = true;
        for (const auto &s : m.subs) {
          if (s.kind == LbKind::Label) {
            sectionVisible = s.open;
            subsSum += lbSubH(s);
          } else if (sectionVisible) {
            subsSum += lbSubH(s);
          }
        }

        float subFade = s_animAlpha * (on ? 1.0f : 0.5f);
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRoundedRect(win.x + 4.0f, blockTop, ww - 8.0f, subsSum + 18.0f, 6.0f, 0x000000, 0.15f * A);

        DWORD divCol = hairline();
        RenderUtils::drawRect(win.x + 12.0f, blockTop, ww - 24.0f, 1.0f, divCol,
                              (((divCol >> 24) & 0xFF) / 255.0f) * 0.7f * A);
        ry += 10.0f;
        const float lx = win.x + 16.0f;
        const float rx = win.x + ww - 16.0f;

        bool sectionRender = true;
        for (size_t si = 0; si < m.subs.size(); ++si) {
          LbSub &sb = m.subs[si];
          if (sb.kind == LbKind::Label) {
            sectionRender = sb.open;
          } else if (!sectionRender) {
            continue;
          }

          float subHeight = lbSubH(sb);
          int sid = 9000 + (int)(unsigned char)win.title[0] * 97 +
                    (int)mi * 17 + (int)si;

          if (sb.kind == LbKind::Label) {
            const std::string dynamicLabel =
                sb.labelGet ? sb.labelGet() : std::string(sb.name);
            bool lhov = inVisibleArea && isHovered(mx, my, win.x + 10, ry, ww - 20, subHeight) && isHit;
            glEnable(GL_TEXTURE_2D);
            DWORD labelColor = lhov ? accent() : textMuted();
            g_guiFont.drawString(lx, ry + subHeight - 13.0f,
                                 dynamicLabel,
                                 applyAlpha(labelColor, 0.85f * s_animAlpha),
                                 0.4f);

            drawChevron(rx - 10.0f, ry + subHeight * 0.5f - 1.0f, 3.5f, sb.open, labelColor, s_animAlpha);
            glDisable(GL_TEXTURE_2D);

            if ((clickEvent || rClickEvent) && isHit && lhov && s_dragWin < 0) {
              sb.open = !sb.open;
            }

            ry += subHeight;
            continue;
          }

          if (sb.kind == LbKind::Toggle) {
            bool son = sb.bget();
            bool shov = inVisibleArea && isHovered(mx, my, win.x + 10, ry, ww - 20, subHeight) && isHit;
            glDisable(GL_TEXTURE_2D);
            if (shov) {
              DWORD hb = surface2();
              RenderUtils::drawRoundedRect(win.x + 10, ry + 2, ww - 20, subHeight - 4,
                                           5.0f, hb,
                                           (((hb >> 24) & 0xFF) / 255.0f) *
                                               0.55f * A);
            }

            float sdx = rx - 5.0f, sdy = ry + subHeight * 0.5f;
            if (son) {
              RenderUtils::drawCircle(sdx, sdy, 3.0f, accent(), subFade);
            } else {
              RenderUtils::drawCircle(sdx, sdy, 3.0f, 0xFF505058, subFade);
            }

            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(lx, ry + subHeight * 0.5f - 6.0f, sb.name,
                                 applyAlpha(son ? textPrimary() : textSecondary(),
                                            subFade), 0.45f);
            glDisable(GL_TEXTURE_2D);
            if (clickEvent && isHit && shov && s_dragWin < 0)
              sb.bset(!son);
            ry += subHeight;
            continue;
          }

          if (sb.kind == LbKind::Slider) {
            float sval = sb.fget();
            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(lx, ry + 8.0f, sb.name,
                                 applyAlpha(textSecondary(), subFade), 0.42f);
            glDisable(GL_TEXTURE_2D);

            bool sch = drawSlider(sid, lx, ry + 28.0f, rx - lx, 8.0f, sval,
                                  sb.fmin, sb.fmax, mx, my,
                                  lClick && isHit && inVisibleArea, subFade);
            glEnable(GL_TEXTURE_2D);
            float displayed = sb.pct ? sval * 100.0f : sval;
            const float displayMin = sb.pct ? sb.fmin * 100.0f : sb.fmin;
            const float displayMax = sb.pct ? sb.fmax * 100.0f : sb.fmax;
            const char *unit = sb.pct ? "%" : sb.unit;
            const bool typed = drawNumericInput(
                sid, rx - 62.0f, ry + 1.0f, 62.0f, 25.0f, displayed,
                displayMin, displayMax, sb.pct ? 0 : 1, unit, mx, my,
                clickEvent && isHit && inVisibleArea, subFade);
            glDisable(GL_TEXTURE_2D);
            if (typed) {
              sval = sb.pct ? displayed / 100.0f : displayed;
              sch = true;
            }
            if (sch) sb.fset(sval);
            ry += subHeight;
            continue;
          }

          if (sb.kind == LbKind::Choice) {
            const char *cur = sb.cget ? sb.cget() : "";
            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(lx, ry + 8.0f, sb.name,
                                 applyAlpha(textSecondary(), subFade), 0.42f);
            glDisable(GL_TEXTURE_2D);
            float boxX = lx, boxY = ry + 28.0f, boxW = rx - lx, boxH = 24.0f;
            bool bhov = inVisibleArea && isHovered(mx, my, boxX, boxY, boxW, boxH) && isHit;
            DWORD bf = (bhov || sb.open) ? surface2() : surface1();
            RenderUtils::drawRoundedRect(boxX, boxY, boxW, boxH, 5.0f, bf,
                                         (((bf >> 24) & 0xFF) / 255.0f) * subFade);
            RenderUtils::drawRoundedOutline(boxX, boxY, boxW, boxH, 5.0f, 1.0f,
                                            sb.open ? accent() : 0x10FFFFFF,
                                            (sb.open ? 0.55f : 0.3f) * subFade);
            drawCaret(boxX + boxW - 13.0f, boxY + boxH * 0.5f, 3.5f, sb.open,
                      textSecondary(), subFade);
            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(boxX + 10, boxY + boxH * 0.5f - 6.0f,
                                 cur ? cur : "",
                                 applyAlpha(textPrimary(), subFade), 0.44f);
            glDisable(GL_TEXTURE_2D);
            if (clickEvent && isHit && bhov && s_dragWin < 0)
              sb.open = !sb.open;
            if (sb.open) {
              for (size_t k = 0; k < sb.choices.size(); ++k) {
                float oy = boxY + boxH + (float)k * kDdOptH;
                bool ohov = inVisibleArea && isHovered(mx, my, boxX, oy, boxW, kDdOptH) && isHit;
                bool sel = cur && strcmp(cur, sb.choices[k]) == 0;
                if (ohov || sel) {
                  DWORD ob = ohov ? surface2() : surface1();
                  RenderUtils::drawRoundedRect(boxX + 2, oy + 1, boxW - 4,
                                               kDdOptH - 2, 4.0f, ob,
                                               (((ob >> 24) & 0xFF) / 255.0f) *
                                                   subFade);
                }
                if (sel)
                  RenderUtils::drawCircle(boxX + boxW - 14, oy + kDdOptH * 0.5f,
                                          3.0f, accent(), subFade);
                glEnable(GL_TEXTURE_2D);
                g_guiFont.drawString(boxX + 12, oy + kDdOptH * 0.5f - 6.0f,
                                     sb.choices[k],
                                     applyAlpha(sel ? textPrimary()
                                                    : textSecondary(),
                                                subFade), 0.43f);
                glDisable(GL_TEXTURE_2D);
                if (clickEvent && isHit && ohov && s_dragWin < 0) {
                  sb.cset(sb.choices[k]);
                  sb.open = false;
                }
              }
            }
            ry += subHeight;
            continue;
          }

          if (sb.kind == LbKind::Action) {
            const bool shov = inVisibleArea &&
                              isHovered(mx, my, win.x + 10, ry, ww - 20, sh) &&
                              isHit;
            glDisable(GL_TEXTURE_2D);
            if (shov) {
              DWORD hb = surface2();
              RenderUtils::drawRoundedRect(
                  win.x + 10, ry + 2, ww - 20, sh - 4, 5.0f, hb,
                  (((hb >> 24) & 0xFF) / 255.0f) * 0.7f * A);
            }
            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(lx, ry + sh * 0.5f - 6.0f, sb.name,
                                 applyAlpha(shov ? textPrimary()
                                                 : textSecondary(),
                                            subFade),
                                 0.44f);
            if (sb.sget) {
              const std::string value = sb.sget();
              const float valueWidth =
                  g_guiFont.getStringWidth(value) * (0.38f / 0.5f);
              g_guiFont.drawString(rx - valueWidth, ry + sh * 0.5f - 5.0f,
                                   value.c_str(), applyAlpha(accent(), subFade),
                                   0.38f);
            }
            glDisable(GL_TEXTURE_2D);
            if (clickEvent && shov && s_dragWin < 0 && sb.action)
              sb.action();
            ry += sh;
            continue;
          }

          if (sb.kind == LbKind::Input) {
            bool typing = sb.typingStatePtr ? *(sb.typingStatePtr) : false;
            std::string &buf = *(sb.inputBufPtr);

            if (!typing && sb.sget) {
              buf = sb.sget();
            }

            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(lx, ry + 8.0f, sb.name,
                                 applyAlpha(textSecondary(), subFade), 0.42f);
            glDisable(GL_TEXTURE_2D);

            float boxX = lx, boxY = ry + 28.0f, boxW = rx - lx, boxH = 24.0f;
            bool bhov = inVisibleArea && isHovered(mx, my, boxX, boxY, boxW, boxH) && isHit;
            
            drawTextInput(boxX, boxY, boxW, boxH, typing, bhov, subFade);

            std::string dispText = buf;
            if (sb.isPassword && !typing && !dispText.empty() && dispText != "None") {
              dispText = "********************";
            }
            if (typing && (GetTickCount64() / 500) % 2 == 0) {
              dispText += "|";
            }
            if (dispText.empty() && !typing) {
              dispText = "None";
            }

            glEnable(GL_TEXTURE_2D);
            g_guiFont.drawString(boxX + 8.0f, boxY + boxH * 0.5f - 6.0f, dispText,
                                 applyAlpha(textPrimary(), subFade), 0.44f);
            glDisable(GL_TEXTURE_2D);

            if (clickEvent && isHit && bhov && s_dragWin < 0) {
              s_typingSearch = s_typingApiKey = s_typingAutoGG = s_typingUrchinKey =
                  s_typingSeraphKey = s_typingAuroraApiKey = s_typingPrefix = false;
              if (sb.typingStatePtr) {
                *(sb.typingStatePtr) = true;
              }
              if (sb.sget) {
                buf = sb.sget();
              }
            } else if (clickEvent && typing) {
              if (sb.sset) {
                sb.sset(buf);
              }
              if (sb.typingStatePtr) {
                *(sb.typingStatePtr) = false;
              }
            }

            ry += subHeight;
            continue;
          }
        }
        ry += 8.0f;
        glDisable(GL_TEXTURE_2D);
        RenderUtils::drawRect(win.x + 12.0f, ry - 1.0f, ww - 24.0f, 1.0f, divCol,
                              (((divCol >> 24) & 0xFF) / 255.0f) * 0.7f * A);
      }
      glDisable(GL_SCISSOR_TEST);
    }

    if (win.open && win.maxScroll > 1.0f) {
      float sbX = win.x + ww - 6.0f;
      float sbY = win.y + titleH + 4.0f;
      float sbH = drawnH - titleH - 8.0f;
      float thumbH = sbH * (sbH / (sbH + win.maxScroll));
      if (thumbH < 20.0f) thumbH = 20.0f;
      float t = win.scrollOffset / win.maxScroll;
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      float thumbY = sbY + t * (sbH - thumbH);

      static int s_dragScWin = -1;
      static float s_dragScY = 0.0f;

      bool hovThumb = isHovered(mx, my, sbX - 4.0f, thumbY, 14.0f, thumbH);
      bool hovTrack = isHovered(mx, my, sbX - 4.0f, sbY, 14.0f, sbH);

      if (lClick) {
        if (s_dragScWin < 0) {
          if (hovThumb) {
            s_dragScWin = (int)wi;
            s_dragScY = my - thumbY;
          } else if (clickEvent && hovTrack) {
            s_dragScWin = (int)wi;
            s_dragScY = thumbH * 0.5f;
          }
        }
        if (s_dragScWin == (int)wi) {
          float newThumbY = my - s_dragScY;
          float newT = (newThumbY - sbY) / (sbH - thumbH);
          newT = newT < 0.0f ? 0.0f : (newT > 1.0f ? 1.0f : newT);
          win.targetScroll = newT * win.maxScroll;
          win.scrollOffset = win.targetScroll; // instant update
          thumbY = sbY + newT * (sbH - thumbH);
        }
      } else {
        if (s_dragScWin == (int)wi) {
          s_dragScWin = -1;
        }
      }

      glDisable(GL_TEXTURE_2D);
      RenderUtils::drawRoundedRect(sbX, thumbY, 3.0f, thumbH, 1.5f, 0xFFFFFFFF, 0.25f * A);
      glEnable(GL_TEXTURE_2D);
    }
  }

  {
    static const char *lyLbl[] = {"A", "B"};
    const std::string &cur = Config::getClickGuiLayout();
    float segX = 16.0f, segY = 16.0f, segW = 84.0f, segH = 26.0f;
    float cW = segW / 2.0f;
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(segX, segY, segW, segH, 8.0f, panelBg(),
                                 0.92f * s_animAlpha);
    RenderUtils::drawRoundedOutline(segX, segY, segW, segH, 8.0f, 1.0f,
                                    hairlineStrong(), 0.5f * s_animAlpha);
    for (int i = 0; i < 2; ++i) {
      float chX = segX + i * cW;
      bool sel = (cur == lyLbl[i]);
      bool hov = isHovered(mx, my, chX, segY, cW, segH);
      if (sel) {
        DWORD soft = accentSoft();
        RenderUtils::drawRoundedRect(chX + 2, segY + 2, cW - 4, segH - 4, 6.0f,
                                     soft,
                                     (((soft >> 24) & 0xFF) / 255.0f) * s_animAlpha);
      }
      glEnable(GL_TEXTURE_2D);
      DWORD tc = sel ? accent() : (hov ? 0xFFFFFFFF : textSecondary());
      float lblW = g_guiFont.getStringWidth(lyLbl[i]) * (0.4f / 0.5f);
      g_guiFont.drawString(chX + cW * 0.5f - lblW * 0.5f - 3.5f, segY + 0.5f, lyLbl[i],
                           applyAlpha(tc, s_animAlpha), 0.4f);
      glDisable(GL_TEXTURE_2D);
      if (clickEvent && hov && !sel)
        Config::setClickGuiLayout(lyLbl[i]);
    }
    glEnable(GL_TEXTURE_2D);
  }

}

void ClickGUI::handleScrollB(float mx, float my, int delta) {
  int hoveredWin = -1;
  const float ww = 252.0f, titleH = 42.0f, rowH = 38.0f;
  for (int wi = (int)s_lbWins.size() - 1; wi >= 0; --wi) {
    LbWindow &w = s_lbWins[wi];
    if (w.open) {
      float wh = lbWinHeight(w, titleH, rowH);
      float drawnH = (wh > 580.0f) ? 580.0f : wh;
      if (mx >= w.x && mx <= w.x + ww && my >= w.y && my <= w.y + drawnH) {
        hoveredWin = wi;
        break;
      }
    }
  }

  if (hoveredWin >= 0) {
    LbWindow &win = s_lbWins[hoveredWin];
    win.targetScroll -= (float)delta * 0.25f;
    if (win.targetScroll < 0.0f) win.targetScroll = 0.0f;
    if (win.targetScroll > win.maxScroll) win.targetScroll = win.maxScroll;
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

  if (s_open) {
    if (!s_accentInit) {
      DWORD c = Config::getThemeColor();
      float rf = ((c >> 16) & 0xFF) / 255.0f;
      float gf = ((c >> 8) & 0xFF) / 255.0f;
      float bf = (c & 0xFF) / 255.0f;
      float cmax = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
      float cmin = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
      float d = cmax - cmin;
      float hh = 0, ss = 0, vv = cmax;
      if (d > 0.00001f) {
        ss = d / cmax;
        if (cmax == rf) {
          hh = (gf - bf) / d + (gf < bf ? 6.0f : 0.0f);
        } else if (cmax == gf) {
          hh = (bf - rf) / d + 2.0f;
        } else {
          hh = (rf - gf) / d + 4.0f;
        }
        hh /= 6.0f;
        if (hh < 0) hh += 1.0f;
      }
      s_accentHue = hh;
      s_accentSat = ss;
      s_accentVal = vv;
      s_accentInit = true;
    }

    if (Config::isChromaEnabled()) {
      static ULONGLONG s_lastChroma = GetTickCount64();
      ULONGLONG now = GetTickCount64();
      float dt = (now - s_lastChroma) / 1000.0f;
      s_lastChroma = now;
      if (dt > 0 && dt < 1.0f) {
        s_accentHue = fmodf(s_accentHue + (Config::getChromaSpeed() * dt) / 360.0f, 1.0f);
      }
      
      float h = s_accentHue;
      float s = s_accentSat;
      float v = s_accentVal;
      float h6 = h * 6.0f;
      int hi = (int)h6 % 6;
      float f = h6 - (int)h6;
      float p = v * (1.0f - s);
      float q = v * (1.0f - f * s);
      float t = v * (1.0f - (1.0f - f) * s);
      float r = 0, g = 0, b = 0;
      switch (hi) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
      }
      DWORD rgb = 0xFF000000u | ((uint8_t)(r * 255) << 16) | ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
      Config::setThemeColor(rgb);
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

  if (s_open && s_waitingForNickRollKey) {
    for (int k = 1; k < 255; ++k) {
      if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON)
        continue;
      if ((GetAsyncKeyState(k) & 0x8000) != 0) {
        if (k == VK_ESCAPE) {
          s_waitingForNickRollKey = false;
        } else {
          Config::setNickRollToggleKey(k);
          Config::save();
          NotificationManager::getInstance()->add(
              "Nick Score", "Toggle bind set to " + getKeyName(k),
              NotificationType::Success);
          s_waitingForNickRollKey = false;
        }
        break;
      }
    }
  }

  if (s_open && s_waitingForUninjectKey) {
    for (int k = 1; k < 255; ++k) {
      if (k == VK_LBUTTON || k == VK_RBUTTON || k == VK_MBUTTON)
        continue;
      if ((GetAsyncKeyState(k) & 0x8000) != 0) {
        if (k == VK_ESCAPE) {
          s_waitingForUninjectKey = false;
        } else {
          Config::setUninjectKey(k);
          Config::save();
          NotificationManager::getInstance()->add(
              "Settings", "Uninject bind set to " + getKeyName(k),
              NotificationType::Success);
          s_waitingForUninjectKey = false;
        }
        break;
      }
    }
  }

  float alphaDiff = s_targetAlpha - s_animAlpha;
  s_animAlpha += alphaDiff * 0.18f;
  s_openingScale = 0.94f + 0.06f * easeOut(s_animAlpha);

  if (s_animAlpha <= 0.001f && !s_open)
    return;

  if (!g_guiFont.isInitialized()) {
    g_guiFont.init(hdc);
  }

  GlGuard::GlMatrixGuard _gMv(GL_MODELVIEW);
  GlGuard::GlAttribGuard _gAttrib(GL_ALL_ATTRIB_BITS);
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

  if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
    Render::LiquidGlass::updateTime(RenderHook::getDelta());
    Render::LiquidGlass::beginFrame((int)sw, (int)sh);
  }

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
  bool rClick = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
  static bool s_lastRButton = false;
  bool rClickEvent = rClick && !s_lastRButton;
  s_lastRButton = rClick;

  if (Config::getClickGuiLayout() == "B") {
    RenderUtils::drawRect(0, 0, sw, sh, applyAlpha(0xB0000000, s_animAlpha));
    renderLayoutB(mx, my, lClick, clickEvent, rClickEvent, sw, sh);
    return;
  }

  constexpr float kMinGuiWidth = 640.0f;
  constexpr float kMinGuiHeight = 520.0f;
  constexpr float kScreenMargin = 8.0f;
  static bool geometryLoaded = false;
  static int resizeEdges = 0; // 1=left, 2=right, 4=top, 8=bottom
  static float resizeMouseX = 0.0f, resizeMouseY = 0.0f;
  static float resizeX = 0.0f, resizeY = 0.0f;
  static float resizeW = 0.0f, resizeH = 0.0f;
  if (!geometryLoaded) {
    g_x = Config::getClickGuiX();
    g_y = Config::getClickGuiY();
    g_w = Config::getClickGuiWidth();
    g_h = Config::getClickGuiHeight();
    geometryLoaded = true;
  }

  // Keep the complete window reachable after a resolution change. On very
  // small clients the screen itself becomes the upper bound, while ordinary
  // resolutions retain a useful minimum layout size.
  const float maximumW = (std::max)(320.0f, sw - kScreenMargin * 2.0f);
  const float maximumH = (std::max)(300.0f, sh - kScreenMargin * 2.0f);
  const float minimumW = (std::min)(kMinGuiWidth, maximumW);
  const float minimumH = (std::min)(kMinGuiHeight, maximumH);
  g_w = std::clamp(g_w, minimumW, maximumW);
  g_h = std::clamp(g_h, minimumH, maximumH);
  g_x = std::clamp(g_x, kScreenMargin,
                   (std::max)(kScreenMargin, sw - g_w - kScreenMargin));
  g_y = std::clamp(g_y, kScreenMargin,
                   (std::max)(kScreenMargin, sh - g_h - kScreenMargin));

  DWORD dim = (style() == Style::LiquidGlass) ? 0x00000000 : 0xB0000000;
  if (dim != 0) {
    RenderUtils::drawRect(0, 0, sw, sh, applyAlpha(dim, s_animAlpha));
  }

  drawThemeBackground(sw, sh, s_animAlpha);

  GlGuard::GlMatrixGuard _gMvInner(GL_MODELVIEW);
  float centerX = g_x + g_w / 2;
  float centerY = g_y + g_h / 2;
  glTranslatef(centerX, centerY, 0);
  glScalef(s_openingScale, s_openingScale, 1.0f);
  glTranslatef(-centerX, -centerY, 0);

  if (s_open && s_animAlpha >= 0.95f) {
    constexpr float edgeHit = 7.0f;
    const bool insideExpanded =
        isHovered(mx, my, g_x - edgeHit, g_y - edgeHit,
                  g_w + edgeHit * 2.0f, g_h + edgeHit * 2.0f);
    const bool nearLeft = insideExpanded && std::fabs(mx - g_x) <= edgeHit;
    const bool nearRight =
        insideExpanded && std::fabs(mx - (g_x + g_w)) <= edgeHit;
    const bool nearTop = insideExpanded && std::fabs(my - g_y) <= edgeHit;
    const bool nearBottom =
        insideExpanded && std::fabs(my - (g_y + g_h)) <= edgeHit;

    if (clickEvent && resizeEdges == 0 && !s_dragging &&
        (nearLeft || nearRight || nearTop || nearBottom)) {
      resizeEdges = (nearLeft ? 1 : 0) | (nearRight ? 2 : 0) |
                    (nearTop ? 4 : 0) | (nearBottom ? 8 : 0);
      resizeMouseX = mx;
      resizeMouseY = my;
      resizeX = g_x;
      resizeY = g_y;
      resizeW = g_w;
      resizeH = g_h;
      clickEvent = false;
    }

    if (resizeEdges != 0) {
      if (!lClick) {
        resizeEdges = 0;
      } else {
        const float dx = mx - resizeMouseX;
        const float dy = my - resizeMouseY;
        const float originalRight = resizeX + resizeW;
        const float originalBottom = resizeY + resizeH;
        if (resizeEdges & 1) {
          g_x = std::clamp(resizeX + dx, kScreenMargin,
                           originalRight - minimumW);
          g_w = originalRight - g_x;
        } else if (resizeEdges & 2) {
          g_w = std::clamp(resizeW + dx, minimumW,
                           sw - kScreenMargin - resizeX);
        }
        if (resizeEdges & 4) {
          g_y = std::clamp(resizeY + dy, kScreenMargin,
                           originalBottom - minimumH);
          g_h = originalBottom - g_y;
        } else if (resizeEdges & 8) {
          g_h = std::clamp(resizeH + dy, minimumH,
                           sh - kScreenMargin - resizeY);
        }
        Config::setClickGuiBounds(g_x, g_y, g_w, g_h);
        clickEvent = false;
      }
    } else if (lClick) {
      if (!s_dragging && clickEvent &&
          isHovered(mx, my, g_x + edgeHit, g_y + edgeHit,
                    g_w - edgeHit * 2.0f, 60.0f - edgeHit) &&
          mx < g_x + g_w - 54.0f) {
        s_dragging = true;
        s_dragOffsetX = mx - g_x;
        s_dragOffsetY = my - g_y;
        clickEvent = false;
      } else if (s_dragging) {
        g_x = std::clamp(mx - s_dragOffsetX, kScreenMargin,
                         (std::max)(kScreenMargin,
                                  sw - g_w - kScreenMargin));
        g_y = std::clamp(my - s_dragOffsetY, kScreenMargin,
                         (std::max)(kScreenMargin,
                                  sh - g_h - kScreenMargin));
        Config::setClickGuiBounds(g_x, g_y, g_w, g_h);
        clickEvent = false;
      }
    } else {
      s_dragging = false;
    }
  }



  const float mainX = g_x;
  const float mainY = g_y;
  const float sidebarW = 180.0f;

  drawThemePanel(mainX, mainY, g_w, g_h, s_animAlpha);
  drawThemeSidebar(mainX, mainY, sidebarW, g_h, s_animAlpha);

  // Small diagonal grip: enough to advertise resizing without adding a
  // permanent label or stealing space from any tab.
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_LINE_SMOOTH);
  glLineWidth(1.4f);
  const DWORD grip = applyAlpha(
      isHovered(mx, my, mainX + g_w - 22.0f, mainY + g_h - 22.0f,
                22.0f, 22.0f)
          ? accent()
          : textMuted(),
      s_animAlpha);
  glColor4ub(static_cast<GLubyte>((grip >> 16) & 0xFF),
             static_cast<GLubyte>((grip >> 8) & 0xFF),
             static_cast<GLubyte>(grip & 0xFF),
             static_cast<GLubyte>((grip >> 24) & 0xFF));
  glBegin(GL_LINES);
  glVertex2f(mainX + g_w - 15.0f, mainY + g_h - 5.0f);
  glVertex2f(mainX + g_w - 5.0f, mainY + g_h - 15.0f);
  glVertex2f(mainX + g_w - 9.0f, mainY + g_h - 5.0f);
  glVertex2f(mainX + g_w - 5.0f, mainY + g_h - 9.0f);
  glEnd();
  glDisable(GL_LINE_SMOOTH);
  glEnable(GL_TEXTURE_2D);

  RenderUtils::drawRect(mainX + sidebarW, mainY + 60, g_w - sidebarW, 1,
                        applyAlpha(0x18FFFFFF, s_animAlpha));

  {
    float lmX = mainX + 16.0f, lmY = mainY + 15.0f, lmS = 30.0f;
    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawGlow(lmX, lmY, lmS, lmS, 8.0f, accent(), 0.22f * s_animAlpha);
    RenderUtils::drawRoundedRect(lmX, lmY, lmS, lmS, 8.0f, accent(), s_animAlpha);
    RenderUtils::drawRoundedRect(lmX + 3, lmY + 2, lmS - 6, lmS * 0.45f, 6.0f,
                                 0xFFFFFFFF, 0.12f * s_animAlpha);
    glEnable(GL_TEXTURE_2D);
    float oW = g_guiFont.getStringWidth("O");
    g_guiFont.drawString(lmX + lmS * 0.5f - oW * 0.5f + 1.0f, lmY + 4.0f, "O",
                         applyAlpha(0xFFFFFFFF, s_animAlpha));
  }
  g_guiFont.drawString(mainX + 56, mainY + 22.0f, "OVSON",
                       applyAlpha(textPrimary(), s_animAlpha));
  g_guiFont.drawString(mainX + 56, mainY + 40.0f, "CLIENT",
                       applyAlpha(accent(), s_animAlpha), 0.45f);

  {
    static const char *tabNm[] = {"Visuals", "Players", "Tags", "Settings",
                                  "Colors", "Debug", "Utils", "Plugins",
                                  "Bedwars"};
    int ti = (s_targetTab >= 0 && s_targetTab < 9) ? s_targetTab : 0;
    float hbX = mainX + sidebarW + 30.0f, hbY = mainY + 22.0f;
    // getStringWidth() is measured at scale 0.5, so visual width of
    // drawString(text, S) == getStringWidth(text) * (S / 0.5).
    float ox = hbX;
    g_guiFont.drawString(ox, hbY, "OVSON",
                         applyAlpha(textPrimary(), s_animAlpha));   // scale 0.5
    ox += g_guiFont.getStringWidth("OVSON") + 8.0f;
    g_guiFont.drawString(ox, hbY + 1.0f, "CLIENT",
                         applyAlpha(accent(), s_animAlpha), 0.42f);
    ox += g_guiFont.getStringWidth("CLIENT") * (0.42f / 0.5f) + 10.0f;
    std::string bc = std::string("/  ") + tabNm[ti];
    g_guiFont.drawString(ox, hbY + 1.0f, bc.c_str(),
                         applyAlpha(textMuted(), s_animAlpha), 0.42f);

    std::string srv = getServerIp();
    float srvW = g_guiFont.getStringWidth(srv) * (0.4f / 0.5f);
    float chipH = 26.0f;
    float chipW = 26.0f + srvW + 14.0f;  // grows with the IP length
    float chipX = mainX + g_w - 16.0f - 28.0f - 12.0f - chipW;
    float chipY = mainY + 17.0f;
    glDisable(GL_TEXTURE_2D);
    DWORD cb = ClickGUITheme::surface2();
    RenderUtils::drawRoundedRect(chipX, chipY, chipW, chipH, chipH * 0.5f, cb,
                                 (((cb >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    DWORD cbd = ClickGUITheme::hairline();
    RenderUtils::drawRoundedOutline(chipX, chipY, chipW, chipH, chipH * 0.5f,
                                    1.0f, cbd,
                                    (((cbd >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    RenderUtils::drawCircle(chipX + 13.0f, chipY + chipH * 0.5f, 3.0f,
                            ClickGUITheme::online(), s_animAlpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(chipX + 21.0f, chipY + 4.5f,
                         srv.c_str(), applyAlpha(textSecondary(), s_animAlpha),
                         0.4f);
  }

  {
    float closeSize = 28.0f;
    float closeX = mainX + g_w - 16.0f - closeSize;
    float closeY = mainY + 16.0f;
    bool hovClose = isHovered(mx, my, closeX, closeY, closeSize, closeSize);

    glDisable(GL_TEXTURE_2D);
    if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
      Render::LiquidGlass::drawRect(closeX, closeY, closeSize, closeSize, 8.0f, s_animAlpha, hovClose ? 0xFFE03A4F : 0x00000000);
    } else {
      DWORD bg = hovClose ? 0xFFE03A4F : 0x15FFFFFF;
      RenderUtils::drawRoundedRect(closeX, closeY, closeSize, closeSize, 8.0f, applyAlpha(bg, s_animAlpha), -1.0f);
    }
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.5f);
    glDisable(GL_TEXTURE_2D);

    glColor4f(1.0f, 1.0f, 1.0f, s_animAlpha * (hovClose ? 1.0f : 0.8f));
    float pad = 9.0f;
    glBegin(GL_LINES);
    glVertex2f(closeX + pad, closeY + pad);
    glVertex2f(closeX + closeSize - pad, closeY + closeSize - pad);
    glVertex2f(closeX + closeSize - pad, closeY + pad);
    glVertex2f(closeX + pad, closeY + closeSize - pad);
    glEnd();

    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_TEXTURE_2D);

    if (clickEvent && hovClose) {
      toggle();
    }
  }

  const float tabStartY = 96.0f;
  const float tabRowH =
      std::clamp((g_h - tabStartY - 94.0f) / 9.0f, 36.0f, 48.0f);
  float targetY = tabStartY + (s_targetTab * tabRowH);
  s_tabIndicatorY += (targetY - s_tabIndicatorY) * 0.22f;

  glDisable(GL_TEXTURE_2D);
  drawThemeTabIndicator(mainX + 12, mainY + s_tabIndicatorY - 10.0f,
                         sidebarW - 24, 42.0f, s_animAlpha);
  glEnable(GL_TEXTURE_2D);

  if (s_activeTab != s_targetTab) {
    s_contentAlpha -= 0.20f;
    if (s_contentAlpha <= 0.0f) {
      s_activeTab = s_targetTab;
      s_contentSlide = 18.0f;
      s_targetScroll = 0.0f;
      s_scrollOffset = 0.0f;
      s_maxScroll = 0.0f;
    }
  } else {
    s_contentAlpha += 0.20f;
    if (s_contentAlpha > 1.0f)
      s_contentAlpha = 1.0f;
    s_contentSlide += (0.0f - s_contentSlide) * 0.18f;
  }

  if (s_targetScroll < 0)
    s_targetScroll = 0;
  if (s_targetScroll > s_maxScroll)
    s_targetScroll = s_maxScroll;
  s_scrollOffset += (s_targetScroll - s_scrollOffset) * 0.18f;

  const char *tabs[] = {"Visuals", "Players", "Tags",  "Settings",
                        "Colors",  "Debug",   "Utils", "Plugins",
                        "Bedwars", nullptr};
  float ty = mainY + tabStartY;
  for (int i = 0; tabs[i]; ++i) {
    bool hover = isHovered(mx, my, mainX + 12, ty - 10, sidebarW - 24, 42);
    
    static float s_tabHov[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    float wantHov = (hover && s_targetTab != i) ? 1.0f : 0.0f;
    s_tabHov[i] += (wantHov - s_tabHov[i]) * 0.2f;
    if (s_tabHov[i] > 0.01f) {
        glDisable(GL_TEXTURE_2D);
        if (ClickGUITheme::style() == ClickGUITheme::Style::LiquidGlass) {
            Render::LiquidGlass::drawRect(mainX + 12, ty - 10, sidebarW - 24, 42,
                                          21.0f, 0.8f * s_tabHov[i] * s_animAlpha,
                                          ClickGUITheme::cardHover());
        } else {
            DWORD hb = ClickGUITheme::surface2();
            RenderUtils::drawRoundedRect(
                mainX + 12, ty - 10, sidebarW - 24, 42, 21.0f, hb,
                (((hb >> 24) & 0xFF) / 255.0f) * s_tabHov[i] * s_animAlpha);
        }
        glEnable(GL_TEXTURE_2D);
    }

    DWORD col = (s_targetTab == i) ? textPrimary()
                : hover            ? 0xFFE5E5EA
                                   : textSecondary();
    DWORD iconCol = (s_targetTab == i) ? accent()
                    : hover            ? 0xFFFFFFFF
                                       : textSecondary();
    glDisable(GL_TEXTURE_2D);
    drawTabIcon(i, mainX + 28, ty + 2, 16.0f, iconCol, s_animAlpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(mainX + 54, ty, tabs[i],
                         applyAlpha(col, s_animAlpha));
    if (clickEvent && hover) {
      s_targetTab = i;
      s_isDropdownOpen = false;
      ClickGUIHelpers::cancelInlineEditors();
    }
    ty += tabRowH;
  }

  {
    static const char *lyLbl[] = {"A", "B"};
    const std::string &cur = Config::getClickGuiLayout();
    float segX = mainX + 12.0f, segY = mainY + g_h - 84.0f;
    float segW = sidebarW - 24.0f, segH = 28.0f;
    float cW = segW / 2.0f;
    glDisable(GL_TEXTURE_2D);
    DWORD gbg = ClickGUITheme::surface1();
    RenderUtils::drawRoundedRect(segX, segY, segW, segH, 8.0f, gbg,
                                 (((gbg >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    DWORD gbd = ClickGUITheme::hairline();
    RenderUtils::drawRoundedOutline(segX, segY, segW, segH, 8.0f, 1.0f, gbd,
                                    (((gbd >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    for (int i = 0; i < 2; ++i) {
      float chX = segX + i * cW;
      bool sel = (cur == lyLbl[i]) || (i == 0 && cur != "B");
      bool hov = isHovered(mx, my, chX, segY, cW, segH);
      if (sel) {
        DWORD soft = ClickGUITheme::accentSoft();
        RenderUtils::drawRoundedRect(chX + 2.0f, segY + 2.0f, cW - 4.0f,
                                     segH - 4.0f, 6.0f, soft,
                                     (((soft >> 24) & 0xFF) / 255.0f) * s_animAlpha);
      }
      glEnable(GL_TEXTURE_2D);
      DWORD tc = sel ? accent() : (hov ? 0xFFFFFFFF : textSecondary());
      g_guiFont.drawString(chX + cW * 0.5f - 4.0f, segY + 8.0f, lyLbl[i],
                           applyAlpha(tc, s_animAlpha), 0.4f);
      glDisable(GL_TEXTURE_2D);
      if (clickEvent && hov && !sel)
        Config::setClickGuiLayout(lyLbl[i]);
    }
    glEnable(GL_TEXTURE_2D);
  }

  {
    static const char *thLbl[] = {"Solid", "Glass", "Min"};
    static const char *thId[]  = {"Solid", "LiquidGlass", "Minimal"};
    const std::string &cur = Config::getClickGuiTheme();
    float segX = mainX + 12.0f, segY = mainY + g_h - 48.0f;
    float segW = sidebarW - 24.0f, segH = 30.0f;
    float cW = segW / 3.0f;
    glDisable(GL_TEXTURE_2D);
    DWORD gbg = ClickGUITheme::surface1();
    RenderUtils::drawRoundedRect(segX, segY, segW, segH, 8.0f, gbg,
                                 (((gbg >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    DWORD gbd = ClickGUITheme::hairline();
    RenderUtils::drawRoundedOutline(segX, segY, segW, segH, 8.0f, 1.0f, gbd,
                                    (((gbd >> 24) & 0xFF) / 255.0f) * s_animAlpha);
    for (int i = 0; i < 3; ++i) {
      float chX = segX + i * cW;
      bool sel = (cur == thId[i]) ||
                 (i == 0 && cur != "LiquidGlass" && cur != "Minimal");
      bool hov = isHovered(mx, my, chX, segY, cW, segH);
      if (sel) {
        RenderUtils::drawGlow(chX + 2.0f, segY + 2.0f, cW - 4.0f, segH - 4.0f,
                              6.0f, accent(), 0.12f * s_animAlpha);
        DWORD soft = ClickGUITheme::accentSoft();
        RenderUtils::drawRoundedRect(chX + 2.0f, segY + 2.0f, cW - 4.0f,
                                     segH - 4.0f, 6.0f, soft,
                                     (((soft >> 24) & 0xFF) / 255.0f) * s_animAlpha);
        DWORD sbd = ClickGUITheme::accentBorder();
        RenderUtils::drawRoundedOutline(chX + 2.0f, segY + 2.0f, cW - 4.0f,
                                        segH - 4.0f, 6.0f, 1.0f, sbd,
                                        (((sbd >> 24) & 0xFF) / 255.0f) * s_animAlpha);
      }
      glEnable(GL_TEXTURE_2D);
      DWORD tc = sel ? accent() : (hov ? 0xFFFFFFFF : textSecondary());
      float lblW = g_guiFont.getStringWidth(thLbl[i]) * (0.4f / 0.5f);
      g_guiFont.drawString(chX + cW * 0.5f - lblW * 0.5f - 2.0f, segY + 6.0f, thLbl[i],
                           applyAlpha(tc, s_animAlpha), 0.4f);
      glDisable(GL_TEXTURE_2D);
      if (clickEvent && hov && !sel) {
        Config::setClickGuiTheme(thId[i]);
        Config::save();
      }
    }
    glEnable(GL_TEXTURE_2D);
  }

  float cx = mainX + sidebarW + 30 + s_contentSlide;
  float startCy = mainY + 86;
  float cy = startCy - s_scrollOffset;
  float alpha = s_animAlpha * s_contentAlpha;

  glEnable(GL_SCISSOR_TEST);
  glScissor((int)(mainX + sidebarW), (int)(sh - (mainY + g_h - 10)),
            (int)(g_w - sidebarW), (int)(g_h - 70));

  TabCtx ctx{hwnd, mainX, mainY, cx, startCy, cy,
             mx, my, lClick, clickEvent, alpha};

  switch (s_activeTab) {
  case 0: Tabs::renderVisuals (ctx); break;
  case 1: Tabs::renderPlayers (ctx); break;
  case 2: Tabs::renderTags    (ctx); break;
  case 3: Tabs::renderSettings(ctx); break;
  case 4: Tabs::renderColors  (ctx); break;
  case 5: Tabs::renderDebug   (ctx); break;
  case 6: Tabs::renderUtils   (ctx); break;
  case 7: Tabs::renderPlugins (ctx); break;
  case 8: Tabs::renderBedwars (ctx); break;
  default: break;
  }

  float contentHeight = (cy + s_scrollOffset) - startCy;
  float visibleHeight = g_h - 110.0f;
  s_maxScroll = (contentHeight > visibleHeight)
                    ? (contentHeight - visibleHeight + 40.0f)
                    : 0.0f;

  glDisable(GL_SCISSOR_TEST);

  if (s_maxScroll > 1.0f) {
    float trackX = mainX + g_w - 13.0f;
    float trackY = mainY + 72.0f;
    float trackH = g_h - 110.0f;
    float thumbH = trackH * (trackH / (trackH + s_maxScroll));
    if (thumbH < 30.0f) thumbH = 30.0f;
    float t = s_scrollOffset / s_maxScroll;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float thumbY = trackY + t * (trackH - thumbH);

    static bool s_dragSc = false;
    static float s_dragScY = 0.0f;

    bool hovThumb = isHovered(mx, my, trackX - 4.0f, thumbY, 14.0f, thumbH);
    bool hovTrack = isHovered(mx, my, trackX - 4.0f, trackY, 14.0f, trackH);

    if (lClick) {
      if (!s_dragSc) {
        if (hovThumb) {
          s_dragSc = true;
          s_dragScY = my - thumbY;
        } else if (clickEvent && hovTrack) {
          s_dragSc = true;
          s_dragScY = thumbH * 0.5f;
        }
      }
      if (s_dragSc) {
        float newThumbY = my - s_dragScY;
        float newT = (newThumbY - trackY) / (trackH - thumbH);
        newT = newT < 0.0f ? 0.0f : (newT > 1.0f ? 1.0f : newT);
        s_targetScroll = newT * s_maxScroll;
        s_scrollOffset = s_targetScroll; // instant update while dragging
        thumbY = trackY + newT * (trackH - thumbH);
      }
    } else {
      s_dragSc = false;
    }

    glDisable(GL_TEXTURE_2D);
    RenderUtils::drawRoundedRect(trackX, thumbY, 6.0f, thumbH, 3.0f,
                                 0xFFFFFFFF, 0.12f * s_animAlpha);
    glEnable(GL_TEXTURE_2D);
  }
}

void ClickGUI::resetLayoutB() {
  s_lbInit = false;
  s_lbWins.clear();
}

} // namespace Render
