#define WIN32_LEAN_AND_MEAN
#include "StatsTracker.internal.h"

#include "../Java.h"
#include "../Config/Config.h"
#include "../Utils/Logger.h"
#include "../Utils/Anticheat/Anticheat.h"

#include <Windows.h>
#include <cstring>
#include <jni.h>
#include <mutex>
#include <string>
#include <unordered_set>
#include <fstream>
#include <chrono>

namespace OVson {

std::unordered_set<std::string> g_helmetTeamSet;

const char *mcColorForTeam(const std::string &team) {
  if (team == "Red")
    return "\xC2\xA7"
           "c";
  if (team == "Blue")
    return "\xC2\xA7"
           "9";
  if (team == "Green")
    return "\xC2\xA7"
           "a";
  if (team == "Yellow")
    return "\xC2\xA7"
           "e";
  if (team == "Aqua")
    return "\xC2\xA7"
           "b";
  if (team == "White")
    return "\xC2\xA7"
           "f";
  if (team == "Pink")
    return "\xC2\xA7"
           "d";
  if (team == "Gray" || team == "Grey")
    return "\xC2\xA7"
           "8";
  return "\xC2\xA7"
         "f";
}

const char *teamInitial(const std::string &team) {
  if (team == "Red")
    return "R";
  if (team == "Blue")
    return "B";
  if (team == "Green")
    return "G";
  if (team == "Yellow")
    return "Y";
  if (team == "Aqua")
    return "A";
  if (team == "White")
    return "W";
  if (team == "Pink")
    return "P";
  if (team == "Gray" || team == "Grey")
    return "G";
  return "?";
}

bool isRealBedwarsTeam(const std::string &t) {
  return t == "Red" || t == "Blue" || t == "Green" || t == "Yellow" ||
         t == "Aqua" || t == "Pink" || t == "White";
}

void setTeamColorSticky(const std::string &name, const std::string &newTeam, bool fromHelmet) {
  if (newTeam.empty())
    return;

  if (fromHelmet) {
      g_helmetTeamSet.insert(name);
  } else if (g_helmetTeamSet.find(name) != g_helmetTeamSet.end()) {
      return;
  }

  auto it = g_playerTeamColor.find(name);
  if (it != g_playerTeamColor.end() && isRealBedwarsTeam(it->second) &&
      (newTeam == "Gray" || newTeam == "Grey")) {
    return;
  }
  g_playerTeamColor[name] = newTeam;

  std::string realLocal = getRealLocalUsername();
  bool isLocal = (!g_localName.empty() && name == g_localName) ||
                 (!realLocal.empty() && name == realLocal) ||
                 (g_isNicked && !g_activeNick.empty() && name == g_activeNick);

  if (isLocal && isRealBedwarsTeam(newTeam)) {
    if (fromHelmet || g_localTeam.empty()) {
      g_localTeam = newTeam;
      Logger::info("Local team set sticky (fromHelmet=%d): %s", fromHelmet,
                   newTeam.c_str());
    }
  }
}

std::string teamFromColorCode(char code) {
  switch (code) {
  case 'c':
    return "Red";
  case '9':
    return "Blue";
  case 'a':
    return "Green";
  case 'e':
    return "Yellow";
  case 'b':
    return "Aqua";
  case 'f':
    return "White";
  case '7':
    return "Gray";
  case 'd':
    return "Pink";
  case '8':
    return "Gray";
  default:
    return "Unknown";
  }
}

void detectTeamsFromLine(const std::string &chat) {
  static const char *teams[] = {"Red",   "Blue", "Green", "Yellow", "Aqua",
                                "White", "Pink", "Gray",  "Grey"};
  for (const char *t : teams) {
    std::string needle1 = std::string("You are on the ") + t + " Team!";
    if (chat.find(needle1) != std::string::npos) {
      Logger::info("Local team detected from chat: %s", t);
      g_localTeam = t;
      std::string myName = !g_localName.empty() ? g_localName : getRealLocalUsername();
      if (!myName.empty() && !g_localTeam.empty()) {
        g_playerTeamColor[myName] = g_localTeam;
        g_helmetTeamSet.insert(myName);
      }
    }
    std::string needle2 = std::string(" joined (") + t + ")";
    auto p2 = chat.find(needle2);
    if (p2 != std::string::npos) {
      auto s = chat.rfind(' ', p2);
      std::string name =
          (s == std::string::npos) ? std::string() : chat.substr(0, s);
      auto sp = name.find_last_of(' ');
      if (sp != std::string::npos)
        name = name.substr(sp + 1);
      if (!name.empty()) {
        setTeamColorSticky(name, t);
        Logger::info("Team detected: %s -> %s", name.c_str(), t);
      }
    }
  }
}

std::string closestTeamColor(int color) {
  if (color == 10511680 || color == -1) return ""; // default leather color or invalid

  int r = (color >> 16) & 0xFF;
  int g = (color >> 8) & 0xFF;
  int b = color & 0xFF;

  struct TeamColor { std::string name; int r, g, b; };
  static const TeamColor teams[] = {
      {"Red", 255, 0, 0},
      {"Blue", 0, 0, 255},
      {"Blue", 51, 76, 178},
      {"Green", 72, 204, 24},
      {"Yellow", 255, 255, 0},
      {"Aqua", 0, 255, 255},
      {"White", 255, 255, 255},
      {"Pink", 239, 130, 164},
      {"Gray", 128, 128, 128}
  };
  
  std::string bestTeam;
  int bestDist = 9999999;
  for (const auto& tc : teams) {
    int dr = r - tc.r;
    int dg = g - tc.g;
    int db = b - tc.b;
    int dist = dr*dr + dg*dg + db*db;
    if (dist < bestDist) {
      bestDist = dist;
      bestTeam = tc.name;
    }
  }
  return bestTeam;
}

static bool isRankPrefix(const std::string &str) {
  if (str.empty()) return false;
  std::string s = str;
  for (char &c : s) c = (char)tolower((unsigned char)c);
  if (s.find("vip") != std::string::npos ||
      s.find("mvp") != std::string::npos ||
      s.find("helper") != std::string::npos ||
      s.find("mod") != std::string::npos ||
      s.find("admin") != std::string::npos ||
      s.find("yt") != std::string::npos ||
      s.find("youtube") != std::string::npos ||
      s.find("hypixel") != std::string::npos ||
      s.find("build team") != std::string::npos ||
      s.find("mojang") != std::string::npos ||
      s.find("owner") != std::string::npos) {
    return true;
  }
  return false;
}

static std::string findColorCodeBeforeName(const std::string &formatted, const std::string &name) {
  if (formatted.empty()) return "";

  static const char *teamNames[] = {"Red", "Blue", "Green", "Yellow", "Aqua", "White", "Pink", "Gray", "Grey"};
  for (const char *t : teamNames) {
    std::string needle = std::string("[") + t + "]";
    if (formatted.find(needle) != std::string::npos) {
      return (strcmp(t, "Grey") == 0) ? "Gray" : t;
    }
  }

  std::string fLower = formatted;
  for (char &c : fLower) c = (char)tolower((unsigned char)c);
  std::string nLower = name;
  for (char &c : nLower) c = (char)tolower((unsigned char)c);

  size_t pos = fLower.rfind(nLower);
  if (pos == std::string::npos) {
    pos = formatted.length();
  }

  char colorCode = 0;

  for (int i = (int)pos - 1; i >= 0; --i) {
    unsigned char c = (unsigned char)formatted[i];
    if (c == ']' && colorCode == 0) {
      break;
    }

    if (i >= 2 && (unsigned char)formatted[i - 2] == 0xC2 && (unsigned char)formatted[i - 1] == 0xA7) {
      char code = (char)tolower(c);
      if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
        colorCode = code;
        break;
      } else if (code == 'k' || code == 'l' || code == 'm' || code == 'n' || code == 'o' || code == 'r') {
        i -= 2;
        continue;
      }
    } else if (i >= 1 && (unsigned char)formatted[i - 1] == 0xA7) {
      char code = (char)tolower(c);
      if ((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) {
        colorCode = code;
        break;
      } else if (code == 'k' || code == 'l' || code == 'm' || code == 'n' || code == 'o' || code == 'r') {
        i -= 1;
        continue;
      }
    }
  }

  if (colorCode == 0) {
    return "";
  }

  std::string team = teamFromColorCode(colorCode);
  if (isRealBedwarsTeam(team)) {
    return team;
  }
  return "";
}

struct TabLocalFrame {
  JNIEnv *env;
  bool ok;
  TabLocalFrame(JNIEnv *e, jint cap = 64) : env(e), ok(false) {
    if (env && env->PushLocalFrame(cap) == 0) ok = true;
  }
  ~TabLocalFrame() {
    if (ok && env) env->PopLocalFrame(nullptr);
  }
};

std::string resolveTeamFromTabList(const std::string &targetPlayerName) {
  JNIEnv *env = lc ? lc->getEnv() : nullptr;
  if (!env) return "";

  TabLocalFrame topFrame(env, 128);
  if (!topFrame.ok) return "";

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) mcCls = env->FindClass("ave");
  if (!mcCls) return "";

  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (env->ExceptionCheck()) env->ExceptionClear();

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (env->ExceptionCheck()) env->ExceptionClear();

  jobject mcObj = nullptr;
  if (m_getMc)
    mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj && theMc)
    mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj) return "";

  jmethodID m_getNet = lc->GetMethodID(
      mcCls, "getNetHandler", "()Lnet/minecraft/client/network/NetHandlerPlayClient;",
      "func_147114_u", "v", "()Lbcy;");
  if (!m_getNet) {
    m_getNet = lc->FindMethodBySignature(mcCls, "()Lnet/minecraft/client/network/NetHandlerPlayClient;");
    if (!m_getNet) m_getNet = lc->FindMethodBySignature(mcCls, "()Lbcy;");
  }

  jobject nh = m_getNet ? env->CallObjectMethod(mcObj, m_getNet) : nullptr;
  if (env->ExceptionCheck()) env->ExceptionClear();

  if (!nh) {
    jfieldID f_thePlayer = lc->GetFieldID(
        mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
        "field_71439_g", "h");
    if (f_thePlayer) {
      jobject playerObj = env->GetObjectField(mcObj, f_thePlayer);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (playerObj) {
        jclass pCls = env->GetObjectClass(playerObj);
        jfieldID f_sendQueue = lc->GetFieldID(
            pCls, "sendQueue", "Lnet/minecraft/client/network/NetHandlerPlayClient;",
            "field_71174_a", "a");
        if (!f_sendQueue) f_sendQueue = lc->FindFieldBySignature(pCls, "Lnet/minecraft/client/network/NetHandlerPlayClient;");
        if (!f_sendQueue) f_sendQueue = lc->FindFieldBySignature(pCls, "Lbcy;");
        if (f_sendQueue) {
          nh = env->GetObjectField(playerObj, f_sendQueue);
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }
    }
  }
  if (!nh) return "";

  jclass nhCls = env->GetObjectClass(nh);
  jmethodID m_getPlayerInfoMap = lc->GetMethodID(
      nhCls, "getPlayerInfoMap", "()Ljava/util/Collection;",
      "func_175106_d", "d", "()Ljava/util/Collection;");
  if (!m_getPlayerInfoMap) {
    m_getPlayerInfoMap = lc->FindMethodBySignature(nhCls, "()Ljava/util/Collection;");
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!m_getPlayerInfoMap) return "";

  jobject coll = env->CallObjectMethod(nh, m_getPlayerInfoMap);
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!coll) return "";

  jclass collCls = env->GetObjectClass(coll);
  jmethodID m_iterator = env->GetMethodID(collCls, "iterator", "()Ljava/util/Iterator;");
  jobject iter = m_iterator ? env->CallObjectMethod(coll, m_iterator) : nullptr;
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!iter) return "";

  jclass iterCls = env->GetObjectClass(iter);
  jmethodID m_hasNext = env->GetMethodID(iterCls, "hasNext", "()Z");
  jmethodID m_next = env->GetMethodID(iterCls, "next", "()Ljava/lang/Object;");

  jclass npiCls = lc->GetClass("net.minecraft.client.network.NetworkPlayerInfo");
  if (!npiCls) npiCls = env->FindClass("bdc");
  if (env->ExceptionCheck()) env->ExceptionClear();

  jmethodID m_getGameProfile = npiCls ? lc->GetMethodID(
      npiCls, "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
      "func_178845_a", "a", "()Lcom/mojang/authlib/GameProfile;") : nullptr;
  jmethodID m_getDisplayName = npiCls ? lc->GetMethodID(
      npiCls, "getDisplayName", "()Lnet/minecraft/util/IChatComponent;",
      "func_178854_k", "k", "()Leu;") : nullptr;
  jmethodID m_getPlayerTeam = npiCls ? lc->GetMethodID(
      npiCls, "getPlayerTeam", "()Lnet/minecraft/scoreboard/ScorePlayerTeam;",
      "func_178850_i", "i", "()Lbfa;") : nullptr;

  jclass gpCls = lc->GetClass("com.mojang.authlib.GameProfile");
  if (!gpCls) gpCls = env->FindClass("com/mojang/authlib/GameProfile");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jmethodID m_getName = gpCls ? env->GetMethodID(gpCls, "getName", "()Ljava/lang/String;") : nullptr;

  jclass chatCompCls = lc->GetClass("net.minecraft.util.IChatComponent");
  if (!chatCompCls) chatCompCls = env->FindClass("eu");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jmethodID m_getFormattedText = chatCompCls ? lc->GetMethodID(
      chatCompCls, "getFormattedText", "()Ljava/lang/String;",
      "func_150254_d", "d", "()Ljava/lang/String;") : nullptr;

  jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
  if (!teamCls) teamCls = env->FindClass("bfa");
  if (env->ExceptionCheck()) env->ExceptionClear();
  jmethodID m_getPrefix = teamCls ? lc->GetMethodID(
      teamCls, "getColorPrefix", "()Ljava/lang/String;",
      "func_96668_e", "c", "()Ljava/lang/String;") : nullptr;
  jmethodID m_getRegisteredName = teamCls ? lc->GetMethodID(
      teamCls, "getRegisteredName", "()Ljava/lang/String;",
      "func_96661_b", "b", "()Ljava/lang/String;") : nullptr;

  jobject tabOverlay = nullptr;
  jmethodID m_getTabPlayerName = nullptr;
  jfieldID f_gui = lc->GetFieldID(
      mcCls, "ingameGUI", "Lnet/minecraft/client/gui/GuiIngame;",
      "field_71456_v", "q", "Lavo;");
  if (!f_gui) f_gui = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
  if (env->ExceptionCheck()) env->ExceptionClear();

  if (f_gui) {
    jobject guiObj = env->GetObjectField(mcObj, f_gui);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (guiObj) {
      jclass gCls = env->GetObjectClass(guiObj);
      jfieldID f_tab = lc->GetFieldID(
          gCls, "overlayPlayerList", "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;",
          "field_175181_C", "v", "Lawh;");
      if (!f_tab) f_tab = lc->FindFieldBySignature(gCls, "Lnet/minecraft/client/gui/GuiPlayerTabOverlay;");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (f_tab) {
        tabOverlay = env->GetObjectField(guiObj, f_tab);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (tabOverlay) {
          jclass tabCls = env->GetObjectClass(tabOverlay);
          m_getTabPlayerName = lc->GetMethodID(
              tabCls, "getPlayerName", "(Lnet/minecraft/client/network/NetworkPlayerInfo;)Ljava/lang/String;",
              "func_175243_a", "a", "(Lbdc;)Ljava/lang/String;");
          if (!m_getTabPlayerName) {
            m_getTabPlayerName = lc->FindMethodBySignature(tabCls, "(Lnet/minecraft/client/network/NetworkPlayerInfo;)Ljava/lang/String;");
            if (!m_getTabPlayerName) m_getTabPlayerName = lc->FindMethodBySignature(tabCls, "(Lbdc;)Ljava/lang/String;");
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
      }
    }
  }

  std::string lowerTarget = targetPlayerName;
  for (char &c : lowerTarget) c = (char)tolower((unsigned char)c);

  std::string realLocalLower = getRealLocalUsername();
  for (char &c : realLocalLower) c = (char)tolower((unsigned char)c);

  std::string nickLower = g_activeNick;
  for (char &c : nickLower) c = (char)tolower((unsigned char)c);

  std::string localNameLower = g_localName;
  for (char &c : localNameLower) c = (char)tolower((unsigned char)c);

  bool lookingForLocal = lowerTarget.empty() || lowerTarget == realLocalLower ||
                         lowerTarget == nickLower || lowerTarget == localNameLower;

  std::string foundTeam = "";

  while (m_hasNext && env->CallBooleanMethod(iter, m_hasNext)) {
    if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
    TabLocalFrame loopFrame(env, 32);
    if (!loopFrame.ok) break;

    jobject npi = env->CallObjectMethod(iter, m_next);
    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
    if (!npi) continue;

    std::string pName = "";
    if (m_getGameProfile && m_getName) {
      jobject gp = env->CallObjectMethod(npi, m_getGameProfile);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (gp) {
        jstring jn = (jstring)env->CallObjectMethod(gp, m_getName);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jn) {
          const char *utf = env->GetStringUTFChars(jn, nullptr);
          if (utf) {
            pName = utf;
            env->ReleaseStringUTFChars(jn, utf);
          }
        }
      }
    }

    if (pName.empty()) continue;

    std::string pNameLower = pName;
    for (char &c : pNameLower) c = (char)tolower((unsigned char)c);

    bool match = false;
    if (pNameLower == lowerTarget) {
      match = true;
    } else if (lookingForLocal &&
               ((!realLocalLower.empty() && pNameLower == realLocalLower) ||
                (!nickLower.empty() && pNameLower == nickLower) ||
                (!localNameLower.empty() && pNameLower == localNameLower))) {
      match = true;
    }

    if (match) {
      std::string tabFormatted = "";

      if (tabOverlay && m_getTabPlayerName) {
        jstring jFormatted = (jstring)env->CallObjectMethod(tabOverlay, m_getTabPlayerName, npi);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (jFormatted) {
          const char *utf = env->GetStringUTFChars(jFormatted, nullptr);
          if (utf) {
            tabFormatted = utf;
            env->ReleaseStringUTFChars(jFormatted, utf);
          }
        }
      }

      if (tabFormatted.empty() && m_getDisplayName && m_getFormattedText) {
        jobject chatComp = env->CallObjectMethod(npi, m_getDisplayName);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (chatComp) {
          jstring jFormatted = (jstring)env->CallObjectMethod(chatComp, m_getFormattedText);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (jFormatted) {
            const char *utf = env->GetStringUTFChars(jFormatted, nullptr);
            if (utf) {
              tabFormatted = utf;
              env->ReleaseStringUTFChars(jFormatted, utf);
            }
          }
        }
      }

      jobject teamObj = m_getPlayerTeam ? env->CallObjectMethod(npi, m_getPlayerTeam) : nullptr;
      if (env->ExceptionCheck()) env->ExceptionClear();

      if (teamObj) {
        if (m_getRegisteredName) {
          jstring jReg = (jstring)env->CallObjectMethod(teamObj, m_getRegisteredName);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (jReg) {
            const char *utf = env->GetStringUTFChars(jReg, nullptr);
            if (utf) {
              std::string regStr = utf;
              env->ReleaseStringUTFChars(jReg, utf);
              for (const char *t : {"Red", "Blue", "Green", "Yellow", "Aqua", "White", "Pink", "Gray"}) {
                if (strstr(regStr.c_str(), t)) {
                  foundTeam = t;
                  break;
                }
              }
            }
          }
        }

        if (tabFormatted.empty() && m_getPrefix) {
          jstring jPref = (jstring)env->CallObjectMethod(teamObj, m_getPrefix);
          if (env->ExceptionCheck()) env->ExceptionClear();
          if (jPref) {
            const char *utf = env->GetStringUTFChars(jPref, nullptr);
            if (utf) {
              tabFormatted = std::string(utf) + pName;
              env->ReleaseStringUTFChars(jPref, utf);
            }
          }
        }
      }

      if (foundTeam.empty() && !tabFormatted.empty()) {
        foundTeam = findColorCodeBeforeName(tabFormatted, pName);
      }

      if (isRealBedwarsTeam(foundTeam)) {
        break;
      }
    }
  }

  return foundTeam;
}

void updateTeamsFromScoreboard() {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
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
  }

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  jobject mcObj = nullptr;
  if (m_getMc)
    mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj && theMc)
    mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  if (!f_world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass worldCls =
      lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
  if (!worldCls) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  
  if (g_inHypixelGame || g_inReplay) {
    jfieldID f_loadedEntityList = env->GetFieldID(worldCls, "loadedEntityList", "Ljava/util/List;");
    if (!f_loadedEntityList) { env->ExceptionClear(); f_loadedEntityList = env->GetFieldID(worldCls, "field_72996_f", "Ljava/util/List;"); }
    if (!f_loadedEntityList) { env->ExceptionClear(); f_loadedEntityList = env->GetFieldID(worldCls, "f", "Ljava/util/List;"); }

    if (f_loadedEntityList) {
      jobject playerList = env->GetObjectField(world, f_loadedEntityList);
      if (playerList) {
        jclass listCls = env->GetObjectClass(playerList);
        jmethodID m_toArray = env->GetMethodID(listCls, "toArray", "()[Ljava/lang/Object;");
        env->DeleteLocalRef(listCls);
        if (m_toArray) {
          jobjectArray array = (jobjectArray)env->CallObjectMethod(playerList, m_toArray);
          if (array) {
            jclass epCls = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
            jmethodID m_getName = lc->GetMethodID(epCls, "getName", "()Ljava/lang/String;", "func_70005_c_", "e_");
            jfieldID f_inventory = lc->GetFieldID(epCls, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;", "field_71071_by", "bi", "Lwm;");
            jclass ipCls = lc->GetClass("net.minecraft.entity.player.InventoryPlayer");
            jfieldID f_armorInventory = lc->GetFieldID(ipCls, "armorInventory", "[Lnet/minecraft/item/ItemStack;", "field_70460_b", "b", "[Lzx;");
            jclass isCls = lc->GetClass("net.minecraft.item.ItemStack");
            jmethodID m_getItem = lc->GetMethodID(isCls, "getItem", "()Lnet/minecraft/item/Item;", "func_77973_b", "b", "()Lzw;");
            jclass iaCls = lc->GetClass("net.minecraft.item.ItemArmor");
            jmethodID m_getColor = lc->GetMethodID(iaCls, "getColor", "(Lnet/minecraft/item/ItemStack;)I", "func_82814_b", "b", "(Lzx;)I");

            if (epCls && m_getName && f_inventory && ipCls && f_armorInventory && isCls && m_getItem && iaCls && m_getColor) {
                int len = env->GetArrayLength(array);
                for (int i = 0; i < len; i++) {
                  jobject player = env->GetObjectArrayElement(array, i);
                  if (player) {
                    if (env->IsInstanceOf(player, epCls)) {
                      jstring jName = (jstring)env->CallObjectMethod(player, m_getName);
                      env->ExceptionClear();
                      if (jName) {
                        const char *nameStr = env->GetStringUTFChars(jName, 0);
                        if (nameStr) {
                          std::string pName(nameStr);
                          env->ReleaseStringUTFChars(jName, nameStr);
                          
                          std::string cleanName;
                          for (size_t k = 0; k < pName.length(); ++k) {
                            if (k + 2 < pName.length() && (unsigned char)pName[k] == 0xC2 && (unsigned char)pName[k+1] == 0xA7) {
                              k += 2;
                            } else if (k + 1 < pName.length() && (unsigned char)pName[k] == 0xA7) {
                              k += 1;
                            } else {
                              cleanName += pName[k];
                            }
                          }
                          pName = cleanName;
                          
                          jobject inv = env->GetObjectField(player, f_inventory);
                          if (inv) {
                            jobjectArray armor = (jobjectArray)env->GetObjectField(inv, f_armorInventory);
                            if (armor) {
                              if (env->GetArrayLength(armor) >= 4) {
                                jobject helmet = env->GetObjectArrayElement(armor, 3);
                                if (helmet) {
                                  jobject item = env->CallObjectMethod(helmet, m_getItem);
                                  env->ExceptionClear();
                                  if (item && env->IsInstanceOf(item, iaCls)) {
                                    int color = env->CallIntMethod(item, m_getColor, helmet);
                                    env->ExceptionClear();
                                    std::string teamStr = closestTeamColor(color);
                                    if (!teamStr.empty()) {
                                        setTeamColorSticky(pName, teamStr, true);
                                    }
                                  }
                                  if (item) env->DeleteLocalRef(item);
                                  env->DeleteLocalRef(helmet);
                                }
                              }
                              env->DeleteLocalRef(armor);
                            }
                            env->DeleteLocalRef(inv);
                          }
                        }
                        env->DeleteLocalRef(jName);
                      }
                    }
                    env->DeleteLocalRef(player);
                  }
                }
            }
            env->DeleteLocalRef(array);
          }
        }
        env->DeleteLocalRef(playerList);
      }
    }
  }


  jmethodID m_getScoreboard = env->GetMethodID(
      worldCls, "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;");
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(
        worldCls, "func_96441_U", "()Lnet/minecraft/scoreboard/Scoreboard;");
  }
  if (!m_getScoreboard) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getScoreboard = env->GetMethodID(worldCls, "Z", "()Lauo;");
  }
  if (!m_getScoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jobject scoreboard = env->CallObjectMethod(world, m_getScoreboard);
  if (!scoreboard) {
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jclass sbCls = lc->GetClass("net.minecraft.scoreboard.Scoreboard");
  if (!sbCls) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }
  jmethodID m_getPlayersTeam = env->GetMethodID(
      sbCls, "getPlayersTeam",
      "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam = env->GetMethodID(
        sbCls, "func_96509_i",
        "(Ljava/lang/String;)Lnet/minecraft/scoreboard/ScorePlayerTeam;");
  }
  if (!m_getPlayersTeam) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    m_getPlayersTeam =
        env->GetMethodID(sbCls, "h", "(Ljava/lang/String;)Laul;");
  }
  if (!m_getPlayersTeam) {
    env->DeleteLocalRef(scoreboard);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return;
  }

  jclass teamCls = lc->GetClass("net.minecraft.scoreboard.ScorePlayerTeam");
  if (!teamCls)
    return;

  auto oldReporter = Lunar::reporter;
  Lunar::reporter = nullptr;

  jmethodID m_getPrefix = lc->GetMethodID(
      teamCls, "getColorPrefix", "()Ljava/lang/String;", "func_96668_e", "c");
  if (!m_getPrefix) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    m_getPrefix = lc->FindMethodBySignature(teamCls, "()Ljava/lang/String;");
  }

  jmethodID m_getSuffix = lc->GetMethodID(
      teamCls, "getColorSuffix", "()Ljava/lang/String;", "func_96663_f", "d");
  if (!m_getSuffix) {
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  (void)m_getSuffix; // resolved for completeness but only prefix is used below

  Lunar::reporter = oldReporter;

  static auto lastDbg = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  bool shouldDbg =
      Config::isGlobalDebugEnabled() &&
      std::chrono::duration_cast<std::chrono::seconds>(now - lastDbg).count() >= 2;
  if (shouldDbg) lastDbg = now;

  std::ofstream dbg;
  if (shouldDbg) {
    const std::string path = Config::getDataDirectory() + "\\ovson_team_debug.txt";
    dbg.open(path.c_str(), std::ios::out);
    dbg << "--- updateTeamsFromScoreboard ---" << std::endl;
    dbg << "inReplay=" << (g_inReplay ? 1 : 0)
        << " inGame=" << (g_inHypixelGame ? 1 : 0)
        << " onlinePlayers=" << g_onlinePlayers.size()
        << " helmetResolved=" << g_helmetTeamSet.size()
        << " teamColorEntries=" << g_playerTeamColor.size() << std::endl;
  }

  for (const std::string &name : g_onlinePlayers) {
    jstring jn = env->NewStringUTF(name.c_str());
    jobject team = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jn);
    if (team) {
      jstring pref = nullptr;
      if (m_getPrefix)
        pref = (jstring)env->CallObjectMethod(team, m_getPrefix);
      env->ExceptionClear();
      if (pref) {
        const char *utf = env->GetStringUTFChars(pref, 0);
        if (utf) {
          if (shouldDbg) dbg << "Player: " << name << " -> Prefix: " << utf;
          std::string teamWord = "";
          const char* tNames[] = {"Red", "Blue", "Green", "Yellow", "Aqua", "Pink", "Gray", "White"};
          for (const char* t : tNames) {
              if (strstr(utf, t)) {
                  teamWord = t;
                  break;
              }
          }

          if (!teamWord.empty()) {
            setTeamColorSticky(name, teamWord);
            if (shouldDbg) dbg << "    -> Assigned Team (by Word): " << teamWord << std::endl;
          } else if (!isRankPrefix(utf)) {
            char code = 0;
            const unsigned char *u = (const unsigned char *)utf;
            for (size_t i = 0; u[i]; ++i) {
              if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
                char c = (char)tolower(u[i + 2]);
                if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                  code = c;
                }
              } else if (u[i] == 0xA7 && u[i + 1]) {
                char c = (char)tolower(u[i + 1]);
                if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                  code = c;
                }
              }
            }
            if (shouldDbg) dbg << " -> Extracted Color: " << (code ? std::string(1, code) : "none") << std::endl;
            if (code) {
              std::string tname = teamFromColorCode(code);
              if (!tname.empty() && isRealBedwarsTeam(tname)) {
                setTeamColorSticky(name, tname);
                if (shouldDbg) dbg << "    -> Assigned Team: " << tname << std::endl;
              }
            }
          } else {
            if (shouldDbg) dbg << "    -> Rank Prefix (" << utf << "), checking tab list..." << std::endl;
            std::string tabTeam = resolveTeamFromTabList(name);
            if (isRealBedwarsTeam(tabTeam)) {
              setTeamColorSticky(name, tabTeam);
              if (shouldDbg) dbg << "    -> Assigned Team (from Tab): " << tabTeam << std::endl;
            }
          }
          env->ReleaseStringUTFChars(pref, utf);
        }
        env->DeleteLocalRef(pref);
      } else {
        if (shouldDbg) dbg << "Player: " << name << " -> NO PREFIX FOUND (m_getPrefix returned NULL)" << std::endl;
      }
      env->DeleteLocalRef(team);
    } else {
      if (shouldDbg) dbg << "Player: " << name << " -> NO TEAM OBJECT FOUND" << std::endl;
    }
    env->DeleteLocalRef(jn);
  }
  env->DeleteLocalRef(scoreboard);
  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
}

std::string resolveTeamForNameEx(JNIEnv *env, const std::string &name,
                                 jobject scoreboard, jmethodID m_getPlayersTeam,
                                 jclass teamCls, jmethodID m_getPrefix) {
  if (!env || !scoreboard || !m_getPlayersTeam || !teamCls || !m_getPrefix)
    return std::string();

  jstring jname = env->NewStringUTF(name.c_str());
  jobject teamObj = env->CallObjectMethod(scoreboard, m_getPlayersTeam, jname);
  env->ExceptionClear();
  env->DeleteLocalRef(jname);

  std::string result;
  if (teamObj) {
    jstring pref = (jstring)env->CallObjectMethod(teamObj, m_getPrefix);
    env->ExceptionClear();
    if (pref) {
      const unsigned char *u =
          (const unsigned char *)env->GetStringUTFChars(pref, 0);
      char code = 0;
      if (u) {
        std::string pStr = (const char *)u;
        if (!isRankPrefix(pStr)) {
          for (size_t i = 0; u[i]; ++i) {
            if (u[i] == 0xC2 && u[i + 1] == 0xA7 && u[i + 2]) {
              char c = (char)tolower(u[i + 2]);
              if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                code = c;
              }
            } else if (u[i] == 0xA7 && u[i + 1]) {
              char c = (char)tolower(u[i + 1]);
              if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                code = c;
              }
            }
          }
          if (code) {
            std::string tname = teamFromColorCode(code);
            if (!tname.empty() && isRealBedwarsTeam(tname))
              result = tname;
          }
        }
        env->ReleaseStringUTFChars(pref, (const char *)u);
      }
      env->DeleteLocalRef(pref);
    }
    env->DeleteLocalRef(teamObj);
  }
  return result;
}

std::string resolveTeamForName(const std::string &name) {
  JNIEnv *env = lc->getEnv();
  if (!g_initialized || !env)
    return std::string();

  if (!g_localName.empty() && name == g_localName) {
    if (!g_localTeam.empty())
      return g_localTeam;
  }

  {
    std::lock_guard<std::recursive_mutex> lock(g_statsMutex);
    auto itT = g_playerTeamColor.find(name);
    if (itT != g_playerTeamColor.end() && !itT->second.empty()) {
      return itT->second;
    }
  }

  g_jCache.init(env);

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return std::string();

  jmethodID m_getMc = env->GetStaticMethodID(
      mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;");
  if (!m_getMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "func_71410_x",
                                     "()Lnet/minecraft/client/Minecraft;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    m_getMc = env->GetStaticMethodID(mcCls, "A", "()Lave;");
  }
  if (!m_getMc) {
    if (env->ExceptionCheck()) env->ExceptionClear();
  }

  jfieldID theMc = env->GetStaticFieldID(mcCls, "theMinecraft",
                                         "Lnet/minecraft/client/Minecraft;");
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "field_71432_P",
                                  "Lnet/minecraft/client/Minecraft;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    theMc = env->GetStaticFieldID(mcCls, "S", "Lave;");
  }
  if (!theMc) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }

  jobject mcObj = nullptr;
  if (m_getMc)
    mcObj = env->CallStaticObjectMethod(mcCls, m_getMc);
  if (!mcObj && theMc)
    mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return std::string();

  jfieldID f_world = env->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;");
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "field_71441_e",
                              "Lnet/minecraft/client/multiplayer/WorldClient;");
  }
  if (!f_world) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_world = env->GetFieldID(mcCls, "f", "Lbdb;");
  }
  jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

  std::string result;
  if (world) {
    jmethodID m_getScoreboard = g_jCache.m_getScoreboard;
    jobject scoreboard = m_getScoreboard
                             ? env->CallObjectMethod(world, m_getScoreboard)
                             : nullptr;
    env->ExceptionClear();
    if (scoreboard) {
      result =
          resolveTeamForNameEx(env, name, scoreboard, g_jCache.m_getPlayersTeam,
                               g_jCache.teamCls, g_jCache.m_getPrefix);
      env->DeleteLocalRef(scoreboard);
    }
    env->DeleteLocalRef(world);
  }
  env->DeleteLocalRef(mcObj);

  if (!isRealBedwarsTeam(result)) {
    std::string tabResult = resolveTeamFromTabList(name);
    if (isRealBedwarsTeam(tabResult)) {
      result = tabResult;
      setTeamColorSticky(name, result);
    }
  }

  return result;
}

} // namespace OVson
