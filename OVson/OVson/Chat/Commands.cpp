#include "Commands.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Logic/BedDefense/BedDefenseManager.h"
#include "../Render/ClickGUI.h"
#include "../Services/Hypixel.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Utils/Logger.h"
#include "../Utils/ReplaySpammer.h"
#include "ChatInterceptor.h"
#include "ChatSDK.h"
#include <algorithm>
#include <iomanip>
#include <jni.h>
#include <sstream>

CommandRegistry &CommandRegistry::instance() {
  static CommandRegistry inst;
  return inst;
}

void CommandRegistry::registerCommand(const std::string &name,
                                      CommandHandler handler) {
  nameToHandler[name] = std::move(handler);
}

bool CommandRegistry::tryDispatch(const std::string &message) {
  if (message.empty() || message[0] != '.')
    return false;
  std::string rest = message.substr(1);
  std::istringstream iss(rest);
  std::string cmd;
  iss >> cmd;
  std::string args;
  std::getline(iss, args);
  if (!args.empty() && args[0] == ' ')
    args.erase(0, 1);

  auto it = nameToHandler.find(cmd);
  if (it == nameToHandler.end()) {
    ChatSDK::showPrefixed(std::string("§cUnknown command: §f.") + cmd);
    return true;
  }
  try {
    it->second(args);
  } catch (...) {
    Logger::error("Command threw: %s", cmd.c_str());
  }
  return true;
}

void CommandRegistry::forEachCommand(
    const std::function<void(const std::string &)> &visitor) {
  for (std::unordered_map<std::string, CommandHandler>::const_iterator it =
           nameToHandler.begin();
       it != nameToHandler.end(); ++it) {
    visitor(it->first);
  }
}

namespace {
void cmd_echo(const std::string &args) { ChatSDK::showPrefixed(args); }

void cmd_help(const std::string &args) {
  (void)args;
  std::string list;
  CommandRegistry::instance().forEachCommand([&](const std::string &name) {
    if (!list.empty())
      list += ", ";
    list += "." + name;
  });
  if (list.empty())
    list = "(no commands)";
  ChatSDK::showPrefixed(std::string("§7Commands: §f") + list);
}

void cmd_api(const std::string &args) {
  std::string trimmed = args;
  while (!trimmed.empty() && trimmed.front() == ' ')
    trimmed.erase(trimmed.begin());
  while (!trimmed.empty() && trimmed.back() == ' ')
    trimmed.pop_back();
  if (trimmed.rfind("new ", 0) == 0) {
    std::string key = trimmed.substr(4);
    Config::setApiKey(key);
    ChatSDK::showPrefixed("API key updated.");
    return;
  }
  const std::string &key = Config::getApiKey();
  if (key.empty())
    ChatSDK::showPrefixed("No API key set. Use .api new <key>");
  else
    ChatSDK::showPrefixed(std::string("API KEY: §f") + key);
}

void cmd_mode(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  if (a == "bedwars") {
    ChatInterceptor::setMode(0);
    ChatSDK::showPrefixed("mode: bedwars");
    return;
  }
  if (a == "skywars") {
    ChatInterceptor::setMode(1);
    ChatSDK::showPrefixed("mode: skywars");
    return;
  }
  if (a == "duels") {
    ChatInterceptor::setMode(2);
    ChatSDK::showPrefixed("mode: duels");
    return;
  }
  ChatSDK::showPrefixed("usage: .mode bedwars|skywars|duels");
}
void cmd_ovmode(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  while (!a.empty() && a.back() == ' ')
    a.pop_back();
  if (a == "gui") {
    Config::setOverlayMode("gui");
    ChatSDK::showPrefixed("§aOverlay mode: §fGUI");
    return;
  }
  if (a == "chat") {
    Config::setOverlayMode("chat");
    ChatSDK::showPrefixed("§aOverlay mode: §fChat");
    return;
  }
  if (a == "invisible") {
    Config::setOverlayMode("invisible");
    ChatSDK::showPrefixed("§aOverlay mode: §fInvisible");
    return;
  }
  ChatSDK::showPrefixed("§cusage: §f.ovmode gui|chat|invisible");
}

void cmd_tab(const std::string &args) {
  std::istringstream iss(args);
  std::string sub;
  iss >> sub;
  std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

  if (sub == "on") {
    Config::setTabEnabled(true);
    ChatSDK::showPrefixed("§aTab Stats: §fEnabled");
    return;
  }
  if (sub == "off") {
    Config::setTabEnabled(false);
    ChatSDK::showPrefixed("§cTab Stats: §fDisabled");
    return;
  }

  std::string val;
  iss >> val;
  std::transform(val.begin(), val.end(), val.begin(), ::tolower);

  if (sub == "display") {
    if (val == "star" || val == "fk" || val == "fkdr" || val == "wins" ||
        val == "wlr" || val == "ws" || val == "lvl") {
      Config::setTabDisplayMode(val);
      ChatSDK::showPrefixed("§aTab Display set to: §f" + val);
    } else {
      ChatSDK::showPrefixed(
          "§cValid display metrics: §fstar, lvl, fk, fkdr, wins, wlr, ws");
    }
    return;
  }
  if (sub == "sort") {
    if (val == "star" || val == "fk" || val == "fkdr" || val == "wins" ||
        val == "wlr" || val == "ws" || val == "team") {
      Config::setSortMode(val);
      ChatSDK::showPrefixed("§aTab Sort set to: §f" + val);
    } else {
      ChatSDK::showPrefixed(
          "§cValid sort metrics: §fstar, fk, fkdr, wins, wlr, ws, team");
    }
    return;
  }
  if (sub == "order") {
    if (val == "asc" || val == "ascending") {
      Config::setTabSortDescending(false);
      ChatSDK::showPrefixed("§aSort Order: §fAscending");
    } else if (val == "desc" || val == "descending") {
      Config::setTabSortDescending(true);
      ChatSDK::showPrefixed("§aSort Order: §fDescending");
    } else {
      ChatSDK::showPrefixed("§cusage: §f.tab order asc|desc");
    }
    return;
  }

  ChatSDK::showPrefixed("§7Usage: §f.tab on|off§7, §f.tab display <metric>§7, "
                        "§f.tab sort <metric>§7, §f.tab order asc|desc");
}

void cmd_debugging(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  while (!a.empty() && a.back() == ' ')
    a.pop_back();

  if (a == "on") {
    Config::setGlobalDebugEnabled(true);
    ChatSDK::showPrefixed("§aDebugging: §fEnabled");
    return;
  }
  if (a == "off") {
    Config::setGlobalDebugEnabled(false);
    ChatSDK::showPrefixed("§cDebugging: §fDisabled");
    return;
  }
  ChatSDK::showPrefixed("§cusage: §f.debugging on|off");
}

void cmd_stats(const std::string &args) {
  std::string playerName = args;
  while (!playerName.empty() && playerName.front() == ' ')
    playerName.erase(playerName.begin());
  while (!playerName.empty() && playerName.back() == ' ')
    playerName.pop_back();

  if (playerName.empty()) {
    ChatSDK::showPrefixed("§cusage: §f.stats <player>");
    return;
  }

  ChatSDK::showPrefixed("§7Fetching stats for §f" + playerName + "§7...");

  auto uuidOpt = Hypixel::getUuidByName(playerName);
  if (!uuidOpt.has_value()) {
    ChatSDK::showPrefixed("§cPlayer not found: §f" + playerName);
    return;
  }

  std::string apiKey = Config::getApiKey();
  if (apiKey.empty()) {
    ChatSDK::showPrefixed("§cNo API key set. Use §f.api new <key>");
    return;
  }

  auto statsOpt = Hypixel::getPlayerStats(apiKey, uuidOpt.value());
  if (!statsOpt.has_value()) {
    ChatSDK::showPrefixed("§cFailed to fetch stats for §f" + playerName);
    return;
  }

  Hypixel::PlayerStats stats = statsOpt.value();
  std::string realName =
      stats.displayName.empty() ? playerName : stats.displayName;

  double fkdr =
      (stats.bedwarsFinalDeaths == 0)
          ? (double)stats.bedwarsFinalKills
          : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
  double wlr = (stats.bedwarsLosses == 0)
                   ? (double)stats.bedwarsWins
                   : (double)stats.bedwarsWins / stats.bedwarsLosses;

  auto colorFkdr = [](double val) -> std::string {
    if (val < 1.0)
      return "§7";
    if (val < 2.0)
      return "§f";
    if (val < 4.0)
      return "§6";
    if (val < 5.0)
      return "§b";
    if (val < 10.0)
      return "§d";
    return "§c";
  };

  auto colorWlr = [](double val) -> std::string {
    if (val < 1.0)
      return "§7";
    if (val < 2.0)
      return "§f";
    if (val < 4.0)
      return "§6";
    if (val < 5.0)
      return "§b";
    if (val < 10.0)
      return "§d";
    return "§c";
  };

  auto colorKills = [](int fk) -> std::string {
    if (fk < 1000)
      return "§7";
    if (fk < 2000)
      return "§f";
    if (fk < 4000)
      return "§6";
    if (fk < 5000)
      return "§b";
    if (fk < 10000)
      return "§d";
    return "§c";
  };

  auto colorWins = [](int w) -> std::string {
    if (w < 500)
      return "§7";
    if (w < 1000)
      return "§f";
    if (w < 2000)
      return "§6";
    if (w < 4000)
      return "§b";
    if (w < 5000)
      return "§d";
    return "§c";
  };

  std::ostringstream fkdrSs, wlrSs;
  fkdrSs << std::fixed << std::setprecision(2) << fkdr;
  wlrSs << std::fixed << std::setprecision(2) << wlr;

  std::string msg = ChatSDK::formatPrefix();
  msg += BedwarsStars::GetFormattedLevel(stats.bedwarsStar) + " ";

  auto getRankColor = [](const std::string &col) -> std::string {
    if (col == "RED")
      return "§c";
    if (col == "GOLD")
      return "§6";
    if (col == "GREEN")
      return "§a";
    if (col == "YELLOW")
      return "§e";
    if (col == "LIGHT_PURPLE")
      return "§d";
    if (col == "WHITE")
      return "§f";
    if (col == "BLUE")
      return "§9";
    if (col == "DARK_GREEN")
      return "§2";
    if (col == "DARK_RED")
      return "§4";
    if (col == "DARK_AQUA")
      return "§3";
    if (col == "DARK_PURPLE")
      return "§5";
    if (col == "DARK_GRAY")
      return "§8";
    if (col == "BLACK")
      return "§0";
    if (col == "DARK_BLUE")
      return "§1";
    return "§c"; // default plus color
  };

  std::string rankDisplay = "§7"; // default gray name
  if (!stats.prefix.empty()) {
    rankDisplay = stats.prefix + " ";
  } else if (!stats.rank.empty() && stats.rank != "NORMAL") {
    if (stats.rank == "ADMIN")
      rankDisplay = "§c[ADMIN] ";
    else if (stats.rank == "YOUTUBER")
      rankDisplay = "§c[§fYOUTUBE§c] ";
    else if (stats.rank == "MOD")
      rankDisplay = "§2[MOD] ";
    else if (stats.rank == "HELPER")
      rankDisplay = "§9[HELPER] ";
    else
      rankDisplay = "§7[" + stats.rank + "] ";
  } else if (stats.monthlyPackageRank == "SUPERSTAR") {
    std::string pc = getRankColor(stats.rankPlusColor);
    rankDisplay = "§6[MVP" + pc + "++§6] ";
  } else if (stats.newPackageRank == "MVP_PLUS") {
    std::string pc = getRankColor(stats.rankPlusColor);
    rankDisplay = "§b[MVP" + pc + "+§b] ";
  } else if (stats.newPackageRank == "MVP") {
    rankDisplay = "§b[MVP] ";
  } else if (stats.newPackageRank == "VIP_PLUS") {
    rankDisplay = "§a[VIP§6+§a] ";
  } else if (stats.newPackageRank == "VIP") {
    rankDisplay = "§a[VIP] ";
  }

  msg += rankDisplay + realName + " §7- ";

  msg += "§7[§fFKDR§7] " + colorFkdr(fkdr) + fkdrSs.str() + " ";
  msg += "§7[§fFK§7] " + colorKills(stats.bedwarsFinalKills) +
         std::to_string(stats.bedwarsFinalKills) + " ";
  msg += "§7[§fWins§7] " + colorWins(stats.bedwarsWins) +
         std::to_string(stats.bedwarsWins) + " ";
  msg += "§7[§fWLR§7] " + colorWlr(wlr) + wlrSs.str();

  std::string tagsStr = "";
  {
    std::lock_guard<std::mutex> lock(ChatInterceptor::g_statsMutex);
    auto it = ChatInterceptor::g_playerStatsMap.find(realName);
    if (it == ChatInterceptor::g_playerStatsMap.end()) {
      it = ChatInterceptor::g_playerStatsMap.find(playerName);
    }

    if (it != ChatInterceptor::g_playerStatsMap.end() &&
        !it->second.tagsDisplay.empty()) {
      tagsStr = it->second.tagsDisplay;
    }
  }

  if (tagsStr.empty()) {
    auto getAbbr = [](const std::string &raw) -> std::string {
      std::string t = raw;
      for (auto &c : t)
        c = toupper(c);
      if (t.find("BLATANT") != std::string::npos)
        return "§4[BC]";
      if (t.find("CLOSET") != std::string::npos)
        return "§4[CC]";
      if (t.find("CONFIRMED") != std::string::npos)
        return "§5[C]";
      if (t.find("CAUTION") != std::string::npos)
        return "§e[!]";
      if (t.find("SUSPICIOUS") != std::string::npos)
        return "§6[?]";
      if (t.find("SNIPER") != std::string::npos)
        return "§6[S]";
      return "";
    };
    std::string activeS = Config::getActiveTagService();
    if (activeS == "Urchin" || activeS == "Both") {
      auto uT = Urchin::getPlayerTags(realName);
      if (uT && !uT->tags.empty()) {
        std::string abbr = getAbbr(uT->tags[0].type);
        tagsStr += " " + (abbr.empty() ? "§4[U]" : abbr);
      }
    }
    if (activeS == "Seraph" || activeS == "Both") {
      std::string uuid = stats.uuid;
      if (uuid.empty()) {
        auto uOpt = Hypixel::getUuidByName(realName);
        if (uOpt)
          uuid = *uOpt;
      }
      if (!uuid.empty()) {
        auto sT = Seraph::getPlayerTags(realName, uuid);
        if (sT && !sT->tags.empty()) {
          std::string abbr = getAbbr(sT->tags[0].type);
          tagsStr += " " + (abbr.empty() ? "§4[S]" : abbr);
        }
      }
    }
  }

  if (!tagsStr.empty()) {
    while (!tagsStr.empty() && tagsStr[0] == ' ')
      tagsStr.erase(0, 1);
    msg += " §7[§fTags§7] §f" + tagsStr;
  }

  ChatSDK::showClientMessage(msg);
}

void cmd_clickgui(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  if (a == "on") {
    Config::setClickGuiOn(true);
    std::string key = Render::ClickGUI::getKeyName(Config::getClickGuiKey());
    ChatSDK::showPrefixed("§aClickGUI: §fEnabled (" + key + " will open menu)");
    return;
  }
  if (a == "off") {
    Config::setClickGuiOn(false);
    std::string key = Render::ClickGUI::getKeyName(Config::getClickGuiKey());
    ChatSDK::showPrefixed("§cClickGUI: §fDisabled (" + key +
                          " will open overlay)");
    return;
  }
  ChatSDK::showPrefixed("usage: .clickgui on|off");
}

void cmd_tech(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  if (a == "on") {
    Config::setTechEnabled(true);
    ChatSDK::showPrefixed("§aTech Overlay: §fEnabled");
    return;
  }
  if (a == "off") {
    Config::setTechEnabled(false);
    ChatSDK::showPrefixed("§cTech Overlay: §fDisabled");
    return;
  }
  bool current = Config::isTechEnabled();
  Config::setTechEnabled(!current);
  ChatSDK::showPrefixed(std::string("§aTech Overlay: §f") +
                        (Config::isTechEnabled() ? "ON" : "OFF"));
}
void cmd_reportspam(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  if (a == "on") {
    Utils::ReplaySpammer::getInstance().setEnabled(true);
    return;
  }
  if (a == "off") {
    Utils::ReplaySpammer::getInstance().setEnabled(false);
    return;
  }
  Utils::ReplaySpammer::getInstance().toggle();
}

} // namespace

void cmd_bedplates(const std::string &args) {
  std::string trimmed = args;
  while (!trimmed.empty() && trimmed.front() == ' ')
    trimmed.erase(trimmed.begin());
  while (!trimmed.empty() && trimmed.back() == ' ')
    trimmed.pop_back();

  BedDefense::BedDefenseManager *manager =
      BedDefense::BedDefenseManager::getInstance();

  if (Config::isForgeEnvironment()) {
    ChatSDK::showPrefixed(
        "§cBedDefense is permanently disabled on Forge for safety.");
    return;
  }

  if (trimmed == "on") {
    Config::setBedDefenseEnabled(true);
    manager->enable();
    ChatSDK::showPrefixed("§aBed Defense nameplates enabled");
  } else if (trimmed == "off") {
    Config::setBedDefenseEnabled(false);
    manager->disable();
    ChatSDK::showPrefixed("§cBed Defense nameplates disabled");
  } else {
    bool current = Config::isBedDefenseEnabled();
    ChatSDK::showPrefixed(std::string("§7Bed Defense: ") +
                          (current ? "§aON" : "§cOFF"));
    ChatSDK::showPrefixed("§7Usage: §f.bedplates on§7 or §f.bedplates off");
  }
}

void cmd_bedscan(const std::string &args) {
  (void)args;
  if (Config::isForgeEnvironment()) {
    ChatSDK::showPrefixed("§cBed scan is disabled on Forge.");
    return;
  }
  ChatSDK::showPrefixed("§7Manually triggering bed scan...");
  BedDefense::BedDefenseManager *manager =
      BedDefense::BedDefenseManager::getInstance();
  manager->onWorldChange();
  manager->forceScan();
}

void cmd_lookat(const std::string &args) {
  (void)args;
  if (!lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls)
      return;

    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                          "Lnet/minecraft/client/Minecraft;",
                                          "field_71432_P", "S", "Lave;");
    if (!theMc)
      return;

    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj)
      return;

    jfieldID f_mouse = lc->GetFieldID(
        mcCls, "objectMouseOver", "Lnet/minecraft/util/MovingObjectPosition;",
        "field_71476_x", "s", "Lauh;");
    if (!f_mouse) {
      env->DeleteLocalRef(mcObj);
      return;
    }

    jobject mop = env->GetObjectField(mcObj, f_mouse);
    if (!mop) {
      ChatSDK::showPrefixed("§7Not looking at anything.");
      env->DeleteLocalRef(mcObj);
      return;
    }

    jclass mopCls = lc->GetClass("net.minecraft.util.MovingObjectPosition");
    if (!mopCls) {
      env->DeleteLocalRef(mop);
      env->DeleteLocalRef(mcObj);
      return;
    }

    jfieldID f_type = lc->GetFieldID(
        mopCls, "typeOfHit",
        "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;",
        "field_72313_a", "a", "Lauh$a;");
    if (!f_type) {
      env->DeleteLocalRef(mop);
      env->DeleteLocalRef(mcObj);
      return;
    }

    jobject hitType = env->GetObjectField(mop, f_type);
    if (!hitType) {
      env->DeleteLocalRef(mop);
      env->DeleteLocalRef(mcObj);
      return;
    }

    jclass typeCls = lc->GetClass(
        "net.minecraft.util.MovingObjectPosition$MovingObjectType");
    if (!typeCls) {
      env->DeleteLocalRef(hitType);
      env->DeleteLocalRef(mop);
      env->DeleteLocalRef(mcObj);
      return;
    }

    jfieldID f_blockType = lc->GetStaticFieldID(
        typeCls, "BLOCK",
        "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;", "BLOCK",
        "a", "Lauh$a;");
    if (!f_blockType) {
      env->DeleteLocalRef(hitType);
      env->DeleteLocalRef(mop);
      env->DeleteLocalRef(mcObj);
      return;
    }

    jobject blockType = env->GetStaticObjectField(typeCls, f_blockType);
    if (env->IsSameObject(hitType, blockType)) {
      jfieldID f_bpos =
          lc->GetFieldID(mopCls, "blockPos", "Lnet/minecraft/util/BlockPos;",
                         "field_178783_e", "e", "Lcj;");
      if (!f_bpos) {
        env->DeleteLocalRef(blockType);
        env->DeleteLocalRef(hitType);
        env->DeleteLocalRef(mop);
        env->DeleteLocalRef(mcObj);
        return;
      }

      jobject bpos = env->GetObjectField(mop, f_bpos);
      if (bpos) {
        jclass bposCls = lc->GetClass("net.minecraft.util.BlockPos");
        if (!bposCls) {
          env->DeleteLocalRef(bpos);
          env->DeleteLocalRef(blockType);
          env->DeleteLocalRef(hitType);
          env->DeleteLocalRef(mop);
          env->DeleteLocalRef(mcObj);
          return;
        }

        jmethodID m_getX =
            lc->GetMethodID(bposCls, "getX", "()I", "func_177958_n", "n");
        jmethodID m_getY =
            lc->GetMethodID(bposCls, "getY", "()I", "func_177956_o", "o");
        jmethodID m_getZ =
            lc->GetMethodID(bposCls, "getZ", "()I", "func_177952_p", "p");

        if (m_getX && m_getY && m_getZ) {
          int bx = env->CallIntMethod(bpos, m_getX);
          int by = env->CallIntMethod(bpos, m_getY);
          int bz = env->CallIntMethod(bpos, m_getZ);

          std::string name = "unknown";
          try {
            name = BedDefense::BedDefenseManager::getInstance()->getBlockName(
                bx, by, bz);
          } catch (...) {
          }
          int meta = 0;
          try {
            meta =
                BedDefense::BedDefenseManager::getInstance()->getBlockMetadata(
                    bx, by, bz);
          } catch (...) {
          }

          std::string debugInfo = "§7ID: §f?";
          try {
            jclass worldCls = lc->GetClass("net.minecraft.world.World");
            jmethodID m_getState =
                lc->GetMethodID(worldCls, "getBlockState",
                                "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/"
                                "block/state/IBlockState;",
                                "func_180495_p", "p", "(Lcj;)Lalz;");

            jfieldID f_world =
                lc->GetFieldID(mcCls, "theWorld",
                               "Lnet/minecraft/client/multiplayer/WorldClient;",
                               "field_71441_e", "f", "Lbdb;");
            jobject world =
                f_world ? env->GetObjectField(mcObj, f_world) : nullptr;

            if (m_getState && world) {
              jobject state = env->CallObjectMethod(world, m_getState, bpos);
              if (state) {
                jclass stateCls =
                    lc->GetClass("net.minecraft.block.state.IBlockState");
                jmethodID m_getBlock = lc->GetMethodID(
                    stateCls, "getBlock", "()Lnet/minecraft/block/Block;",
                    "func_177230_c", "c", "()Lafh;");
                if (m_getBlock) {
                  jobject block = env->CallObjectMethod(state, m_getBlock);
                  if (block) {
                    jclass blockCls = lc->GetClass("net.minecraft.block.Block");
                    jmethodID m_getId =
                        lc->GetStaticMethodID(blockCls, "getIdFromBlock",
                                              "(Lnet/minecraft/block/Block;)I",
                                              "func_149682_b", "a", "(Lafh;)I");
                    int id = m_getId ? env->CallStaticIntMethod(blockCls,
                                                                m_getId, block)
                                     : -1;

                    jmethodID m_getUnloc = lc->GetMethodID(
                        blockCls, "getUnlocalizedName", "()Ljava/lang/String;",
                        "func_149739_a", "a");
                    std::string uName = "unknown";
                    if (m_getUnloc) {
                      jstring jUnloc =
                          (jstring)env->CallObjectMethod(block, m_getUnloc);
                      if (jUnloc) {
                        const char *unlocStr =
                            env->GetStringUTFChars(jUnloc, 0);
                        if (unlocStr)
                          uName = unlocStr;
                        env->ReleaseStringUTFChars(jUnloc, unlocStr);
                        env->DeleteLocalRef(jUnloc);
                      }
                    }

                    jclass objCls = env->GetObjectClass(block);
                    jmethodID m_getClass = lc->GetMethodID(
                        nullptr, "getClass", "()Ljava/lang/Class;");
                    jclass clsObj =
                        (jclass)env->CallObjectMethod(block, m_getClass);
                    jmethodID m_getName = lc->GetMethodID(
                        nullptr, "getName", "()Ljava/lang/String;");
                    jstring clsNameStr =
                        (jstring)env->CallObjectMethod(clsObj, m_getName);

                    const char *clsUtf = env->GetStringUTFChars(clsNameStr, 0);
                    std::string cName = clsUtf ? clsUtf : "unknown";
                    env->ReleaseStringUTFChars(clsNameStr, clsUtf);

                    debugInfo = "§7ID: §f" + std::to_string(id) +
                                " §7Name: §f" + uName + " §7Class: §f" + cName;

                    env->DeleteLocalRef(objCls);
                    env->DeleteLocalRef(clsObj);
                    env->DeleteLocalRef(clsNameStr);
                    env->DeleteLocalRef(block);
                  }
                }
                env->DeleteLocalRef(state);
              }
            }
            if (world)
              env->DeleteLocalRef(world);
          } catch (...) {
          }

          ChatSDK::showPrefixed("§7LookAt: §f" + name + " §7(Meta: §f" +
                                std::to_string(meta) + "§7)");
          ChatSDK::showPrefixed(debugInfo + " §7at §f" + std::to_string(bx) +
                                "," + std::to_string(by) + "," +
                                std::to_string(bz));
        }
        env->DeleteLocalRef(bpos);
      }
    } else {
      ChatSDK::showPrefixed("§7Not looking at a block.");
    }
    env->DeleteLocalRef(blockType);
    env->DeleteLocalRef(hitType);
    env->DeleteLocalRef(mop);
    env->DeleteLocalRef(mcObj);
  } catch (...) {
    ChatSDK::showPrefixed("§cException in lookat command.");
  }
}

void cmd_clearcache(const std::string &args) {
  (void)args;
  ChatInterceptor::clearAllCaches();
  ChatSDK::showPrefixed("§aAll caches cleared! Stats will be re-fetched.");
}

void cmd_commands(const std::string &args) {
  std::string a = args;
  while (!a.empty() && a.front() == ' ')
    a.erase(a.begin());
  if (a == "on") {
    Config::setCommandsEnabled(true);
    ChatSDK::showPrefixed("§aCommand Interception: §fEnabled");
  } else if (a == "off") {
    Config::setCommandsEnabled(false);
    ChatSDK::showPrefixed("§cCommand Interception: §fDisabled");
    ChatSDK::showPrefixed("§7(Use ClickGUI to turn back on)");
  } else {
    bool current = Config::isCommandsEnabled();
    ChatSDK::showPrefixed(std::string("§aCommand Interception: §f") +
                          (current ? "ON" : "OFF"));
    ChatSDK::showPrefixed("§7Usage: §f.commands on|off");
  }
}

void RegisterDefaultCommands() {
  CommandRegistry::instance().registerCommand("echo", cmd_echo);
  CommandRegistry::instance().registerCommand("help", cmd_help);
  CommandRegistry::instance().registerCommand("api", cmd_api);
  CommandRegistry::instance().registerCommand("mode", cmd_mode);
  CommandRegistry::instance().registerCommand("ovmode", cmd_ovmode);
  CommandRegistry::instance().registerCommand("tab", cmd_tab);
  CommandRegistry::instance().registerCommand("debugging", cmd_debugging);
  CommandRegistry::instance().registerCommand("stats", cmd_stats);
  CommandRegistry::instance().registerCommand("clickgui", cmd_clickgui);
  CommandRegistry::instance().registerCommand("bedplates", cmd_bedplates);
  CommandRegistry::instance().registerCommand("bedscan", cmd_bedscan);
  CommandRegistry::instance().registerCommand("lookat", cmd_lookat);
  CommandRegistry::instance().registerCommand("clearcache", cmd_clearcache);
  CommandRegistry::instance().registerCommand("tech", cmd_tech);
  CommandRegistry::instance().registerCommand("reportspam", cmd_reportspam);
  CommandRegistry::instance().registerCommand("commands", cmd_commands);
}
