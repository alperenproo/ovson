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
  } else if (g_inReplay && g_helmetTeamSet.find(name) != g_helmetTeamSet.end()) {
      return;
  }

  auto it = g_playerTeamColor.find(name);
  if (it != g_playerTeamColor.end() && isRealBedwarsTeam(it->second) &&
      (newTeam == "Gray" || newTeam == "Grey")) {
    return;
  }
  g_playerTeamColor[name] = newTeam;

  // g_localTeam (the local player's own team) normally only comes from the
  // "You are on the X Team!" chat line, which Hypixel sends exactly once
  // per match. If that single line is missed -- e.g. the DLL was injected
  // or reattached after the match had already started, or the chat hook
  // wasn't attached in time -- g_localTeam stays empty for the rest of the
  // match with no other way to recover it. Every Bedwars Tools feature
  // that depends on knowing which nearby players are teammates (Player
  // Alerts, own-team upgrade tracking) silently stops working in that
  // case, even though the per-player team map above is still being
  // populated correctly via the scoreboard/tab list. Mirror any confident
  // team resolution for the local player's own name into g_localTeam here
  // as a fallback, without ever overwriting an already-known value.
  if (g_localTeam.empty() && !g_localName.empty() && name == g_localName &&
      isRealBedwarsTeam(newTeam)) {
    g_localTeam = newTeam;
    Logger::info("Local team resolved via scoreboard fallback: %s",
                 newTeam.c_str());
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
      Logger::info("Local team detected: %s", t);
      g_localTeam = t;
      if (!g_localName.empty() && !g_localTeam.empty()) {
        g_playerTeamColor[g_localName] = g_localTeam;
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
  
  if (g_inReplay) {
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

  // This dump is the only thing that can say WHY a player ended up without a
  // team, and it was pointed at C:\Users\HPC1\Desktop -- a path that exists on
  // whoever wrote it, and on nobody else's machine. Everywhere else it
  // silently failed to open, every two seconds, forever. It now goes next to
  // the rest of the logs and only runs when debugging is on.
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
    // Which of the two paths is even running, and what the helmet pass found.
    // In a replay the scoreboard usually has nothing, so the helmet pass is
    // the only real source -- and it can only see players whose entity is
    // loaded near the replay camera.
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
          } else {
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
              if (!tname.empty()) {
                setTeamColorSticky(name, tname);
                if (shouldDbg) dbg << "    -> Assigned Team: " << tname << std::endl;
              }
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
        env->ReleaseStringUTFChars(pref, (const char *)u);
      }
      if (code) {
        std::string tname = teamFromColorCode(code);
        if (!tname.empty())
          result = tname;
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
    std::lock_guard<std::mutex> lock(g_statsMutex);
    auto itT = g_playerTeamColor.find(name);
    if (itT != g_playerTeamColor.end() && !itT->second.empty()) {
      return itT->second;
    }
  }

  g_jCache.init(env);

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return std::string();
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
  jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;
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
  return result;
}

} // namespace OVson
