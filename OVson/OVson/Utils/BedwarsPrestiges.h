// more will be added
#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include "../Services/Hypixel.h"

namespace BedwarsStars
{
    inline std::string resolveStarSymbol(const std::string& activeStar, int level)
    {
        std::string s = activeStar;
        for (char& c : s) c = (char)::tolower((unsigned char)c);
        if (s.rfind("bedwars_active_star_", 0) == 0) s = s.substr(20);
        else if (s.rfind("active_star_", 0) == 0) s = s.substr(12);
        else if (s.rfind("bedwars_star_", 0) == 0) s = s.substr(13);

        if (s == "star_four_pointed" || s == "four_pointed" || s == "✦") return "✦";
        if (s == "star_four_clubs" || s == "four_clubs" || s == "✥") return "✥";
        if (s == "star_black_open" || s == "black_open" || s == "✫") return "✫";
        if (s == "star_open" || s == "open" || s == "✧") return "✧";
        if (s == "star_heart" || s == "heart" || s == "❤") return "❤";
        if (s == "star_skull" || s == "skull" || s == "☠") return "☠";
        if (s == "star_lightning" || s == "lightning" || s == "⚡") return "⚡";
        if (s == "star_sword" || s == "sword" || s == "⚔") return "⚔";
        if (s == "star_snowflake" || s == "snowflake" || s == "❄") return "❄";
        if (s == "star_flower" || s == "flower" || s == "✿") return "✿";
        if (s == "star_music" || s == "star_notes" || s == "music" || s == "notes" || s == "♫") return "♫";
        if (s == "star_queen" || s == "star_crown" || s == "queen" || s == "crown" || s == "♛") return "♛";
        if (s == "star_king" || s == "king" || s == "♚") return "♚";
        if (s == "star_diamond" || s == "diamond" || s == "◆") return "◆";
        if (s == "star_circle" || s == "circle" || s == "●") return "●";
        if (s == "star_triangle" || s == "triangle" || s == "▲") return "▲";
        if (s == "star_airplane" || s == "airplane" || s == "✈") return "✈";
        if (s == "star_umbrella" || s == "umbrella" || s == "☂") return "☂";
        if (s == "star_gear" || s == "gear" || s == "⚙") return "⚙";
        if (s == "star_check" || s == "check" || s == "✔") return "✔";
        if (s == "star_five_pointed" || s == "five_pointed" || s == "✪") return "✪";
        if (s == "star_six_pointed" || s == "six_pointed" || s == "⚝") return "⚝";
        if (s == "star_cross" || s == "cross" || s == "✖") return "✖";
        if (s == "star_peace" || s == "peace" || s == "☮") return "☮";
        if (s == "star_yin_yang" || s == "yin_yang" || s == "☯") return "☯";
        if (s == "star_sun" || s == "sun" || s == "☀") return "☀";
        if (s == "star_moon" || s == "moon" || s == "☽") return "☽";
        if (s == "star_target" || s == "target" || s == "◎") return "◎";
        if (s == "star_wave" || s == "wave") return "≈";
        if (s == "star_regular" || s == "star_standard" || s == "regular" || s == "standard" || s == "none" || s.empty()) {
            if (level >= 3100) return "✥";
            else if (level >= 2100) return "⚝";
            else if (level >= 1100) return "✪";
            return "✫";
        }

        if (level >= 3100) return "✥";
        else if (level >= 2100) return "⚝";
        else if (level >= 1100) return "✪";
        return "✫";
    }

    inline void resolveBrackets(const std::string& activeBracket, std::string& openB, std::string& closeB)
    {
        std::string b = activeBracket;
        for (char& c : b) c = (char)::tolower((unsigned char)c);

        if (b == "prestige_bracket_parenthesis" || b == "parenthesis" || b == "round" || b == "(") {
            openB = "(";
            closeB = ")";
        } else if (b == "prestige_bracket_curly" || b == "curly" || b == "brace" || b == "{") {
            openB = "{";
            closeB = "}";
        } else if (b == "prestige_bracket_angle" || b == "angle" || b == "<") {
            openB = "<";
            closeB = ">";
        } else {
            openB = "[";
            closeB = "]";
        }
    }

    inline int resolveSchemeLevel(const std::string& activeScheme)
    {
        if (activeScheme.empty()) return -1;
        std::string s = activeScheme;
        for (char& c : s) c = (char)::tolower((unsigned char)c);

        if (s == "prestige_scheme_none" || s == "none") return -1;

        static const std::unordered_map<std::string, int> s_schemeLevels = {
            {"prestige_scheme_stone", 0},
            {"stone", 0},
            {"prestige_scheme_iron", 100},
            {"iron", 100},
            {"prestige_scheme_gold", 200},
            {"gold", 200},
            {"prestige_scheme_diamond", 300},
            {"diamond", 300},
            {"prestige_scheme_emerald", 400},
            {"emerald", 400},
            {"prestige_scheme_sapphire", 500},
            {"sapphire", 500},
            {"prestige_scheme_ruby", 600},
            {"ruby", 600},
            {"prestige_scheme_crystal", 700},
            {"crystal", 700},
            {"prestige_scheme_opal", 800},
            {"opal", 800},
            {"prestige_scheme_amethyst", 900},
            {"amethyst", 900},
            {"prestige_scheme_rainbow", 1000},
            {"rainbow", 1000},
            {"prestige_scheme_iron_prime", 1100},
            {"iron_prime", 1100},
            {"prestige_scheme_gold_prime", 1200},
            {"gold_prime", 1200},
            {"prestige_scheme_diamond_prime", 1300},
            {"diamond_prime", 1300},
            {"prestige_scheme_emerald_prime", 1400},
            {"emerald_prime", 1400},
            {"prestige_scheme_sapphire_prime", 1500},
            {"sapphire_prime", 1500},
            {"prestige_scheme_ruby_prime", 1600},
            {"ruby_prime", 1600},
            {"prestige_scheme_crystal_prime", 1700},
            {"crystal_prime", 1700},
            {"prestige_scheme_opal_prime", 1800},
            {"opal_prime", 1800},
            {"prestige_scheme_amethyst_prime", 1900},
            {"amethyst_prime", 1900},
            {"prestige_scheme_mirror", 2000},
            {"mirror", 2000},
            {"prestige_scheme_light", 2100},
            {"light", 2100},
            {"prestige_scheme_dawn", 2200},
            {"dawn", 2200},
            {"prestige_scheme_dusk", 2300},
            {"dusk", 2300},
            {"prestige_scheme_air", 2400},
            {"air", 2400},
            {"prestige_scheme_wind", 2500},
            {"wind", 2500},
            {"prestige_scheme_nebula", 2600},
            {"nebula", 2600},
            {"prestige_scheme_thunder", 2700},
            {"thunder", 2700},
            {"prestige_scheme_earth", 2800},
            {"earth", 2800},
            {"prestige_scheme_water", 2900},
            {"water", 2900},
            {"prestige_scheme_fire", 3000},
            {"fire", 3000},
            {"prestige_scheme_sandstorm", 3100},
            {"sandstorm", 3100},
            {"prestige_scheme_blood", 3200},
            {"blood", 3200},
            {"prestige_scheme_plague", 3200},
            {"plague", 3200},
            {"prestige_scheme_portal", 3300},
            {"portal", 3300},
            {"prestige_scheme_dragon", 3400},
            {"dragon", 3400},
            {"prestige_scheme_overgrowth", 3500},
            {"overgrowth", 3500},
            {"prestige_scheme_abyssal", 3600},
            {"abyssal", 3600},
            {"prestige_scheme_charge", 3700},
            {"charge", 3700},
            {"prestige_scheme_solar", 3800},
            {"solar", 3800},
            {"prestige_scheme_eclipse", 3900},
            {"eclipse", 3900},
            {"prestige_scheme_aurora", 4000},
            {"aurora", 4000},
            {"prestige_scheme_warmth", 4100},
            {"warmth", 4100},
            {"prestige_scheme_winter", 4200},
            {"winter", 4200},
            {"prestige_scheme_obsidian", 4300},
            {"obsidian", 4300},
            {"prestige_scheme_spring", 4400},
            {"spring", 4400},
            {"prestige_scheme_ice", 4500},
            {"ice", 4500},
            {"prestige_scheme_summer", 4600},
            {"summer", 4600},
            {"prestige_scheme_autumn", 4700},
            {"autumn", 4700},
            {"prestige_scheme_mystic", 4800},
            {"mystic", 4800},
            {"prestige_scheme_galactic", 4900},
            {"galactic", 4900},
            {"prestige_scheme_magic", 5000},
            {"magic", 5000},
            {"prestige_scheme_atomic", 5000},
            {"atomic", 5000}
        };

        auto it = s_schemeLevels.find(s);
        if (it != s_schemeLevels.end()) return it->second;
        return -1;
    }

    inline std::string GetFormattedLevel(int level, const std::string& activeStar = "", const std::string& activeScheme = "", const std::string& activeBracket = "")
    {
        std::string sLevel = std::to_string(level);
        std::string star = resolveStarSymbol(activeStar, level);
        std::string openB = "[", closeB = "]";
        resolveBrackets(activeBracket, openB, closeB);

        int schemeLvl = resolveSchemeLevel(activeScheme);
        int colorLevel = (schemeLvl >= 0) ? schemeLvl : level;

        if (colorLevel < 100) return "§7" + openB + sLevel + star + closeB;
        if (colorLevel < 200) return "§f" + openB + sLevel + star + closeB; // Iron (White)
        if (colorLevel < 300) return "§6" + openB + sLevel + star + closeB; // Gold
        if (colorLevel < 400) return "§b" + openB + sLevel + star + closeB; // Diamond (Aqua)
        if (colorLevel < 500) return "§2" + openB + sLevel + star + closeB; // Emerald (Dark Green)
        if (colorLevel < 600) return "§3" + openB + sLevel + star + closeB; // Sapphire (Dark Aqua)
        if (colorLevel < 700) return "§4" + openB + sLevel + star + closeB; // Ruby (Dark Red)
        if (colorLevel < 800) return "§d" + openB + sLevel + star + closeB; // Crystal (Pink)
        if (colorLevel < 900) return "§9" + openB + sLevel + star + closeB; // Opal (Blue)
        if (colorLevel < 1000) return "§5" + openB + sLevel + star + closeB; // Amethyst (Purple)

        auto format = [&](const std::string& b1, const std::string& d1, const std::string& d2, const std::string& d3, const std::string& d4, const std::string& st, const std::string& b2) {
            std::string s = sLevel;
            if (s.length() <= 1) {
                return b1 + openB + d1 + s + st + star + b2 + closeB;
            } else if (s.length() == 2) {
                return b1 + openB + d1 + s.substr(0,1) + d2 + s.substr(1,1) + st + star + b2 + closeB;
            } else if (s.length() == 3) {
                return b1 + openB + d1 + s.substr(0,1) + d2 + s.substr(1,1) + d3 + s.substr(2,1) + st + star + b2 + closeB;
            } else {
                return b1 + openB + d1 + s.substr(0,1) + d2 + s.substr(1,1) + d3 + s.substr(2,1) + d4 + s.substr(3) + st + star + b2 + closeB;
            }
        };

        // 1000: Rainbow (c, 6, e, a, b, d, 5)
        if (colorLevel < 1100) return format("§c", "§6", "§e", "§a", "§b", "§d", "§5");

        // 1100: Iron Prime (7, f, f, f, f, 7, 7) - White/Gray
        if (colorLevel < 1200) return format("§7", "§f", "§f", "§f", "§f", "§7", "§7");

        // 1200: Gold Prime (7, e, 6, 6, e, 6, 7) - Mostly Gold/Yellow
        if (colorLevel < 1300) return format("§7", "§e", "§6", "§6", "§e", "§6", "§7");

        // 1300: Diamond Prime (7, b, 3, 3, b, 3, 7)
        if (colorLevel < 1400) return format("§7", "§b", "§3", "§3", "§b", "§3", "§7");

        // 1400: Emerald Prime (7, a, 2, 2, a, 2, 7)
        if (colorLevel < 1500) return format("§7", "§a", "§2", "§2", "§a", "§2", "§7");

        // 1500: Sapphire Prime (7, 3, 9, 9, 3, 9, 7)
        if (colorLevel < 1600) return format("§7", "§3", "§9", "§9", "§3", "§9", "§7");

        // 1600: Ruby Prime (7, c, 4, 4, c, 4, 7)
        if (colorLevel < 1700) return format("§7", "§c", "§4", "§4", "§c", "§4", "§7");

        // 1700: Crystal Prime (7, d, 5, 5, d, 5, 7)
        if (colorLevel < 1800) return format("§7", "§d", "§5", "§5", "§d", "§5", "§7");

        // 1800: Opal Prime (7, 9, 1, 1, 9, 1, 7)
        if (colorLevel < 1900) return format("§7", "§9", "§1", "§1", "§9", "§1", "§7");

        // 1900: Amethyst Prime (7, 5, 8, 8, 5, 8, 7)
        if (colorLevel < 2000) return format("§7", "§5", "§8", "§8", "§5", "§8", "§7");

        // 2000: Mirror (8, 7, f, f, 7, 8, 8)
        if (colorLevel < 2100) return format("§8", "§7", "§f", "§f", "§7", "§8", "§8");

        // 2100: Light (7, f, e, 6, 6, 6, 7)
        if (colorLevel < 2200) return format("§f", "§f", "§e", "§6", "§6", "§6", "§7"); // approx

        // 2200: Dawn (Orange/Teal?) HTML: 2 is Gray, 2 is White, 0 is Teal?
        if (colorLevel < 2300) return format("§7", "§f", "§f", "§3", "§3", "§3", "§7");

        // 2300 Dusk: 5, d, 6, e, e 
        if (colorLevel < 2400) return format("§5", "§5", "§d", "§6", "§e", "§e", "§6");

        // 2400 Air: b, f, f, 8, 8
        if (colorLevel < 2500) return format("§b", "§b", "§f", "§f", "§8", "§8", "§8");

        // 2500 Wind: f, a, a, 2, 2
        if (colorLevel < 2600) return format("§f", "§f", "§a", "§a", "§2", "§2", "§8");

        // 2600 Nebula: 4, c, c, d, d
        if (colorLevel < 2700) return format("§4", "§4", "§c", "§c", "§d", "§d", "§5");

        // 2700 Thunder: e, f, f, 8, 8
        if (colorLevel < 2800) return format("§e", "§e", "§f", "§f", "§8", "§8", "§8");

        // 2800 Earth: a, 2, 2, 6, e
        if (colorLevel < 2900) return format("§a", "§a", "§2", "§2", "§6", "§e", "§a");

        // 2900 Water: b, 3, 3, 9, 1
        if (colorLevel < 3000) return format("§b", "§b", "§3", "§3", "§9", "§1", "§9");

        // 3000 Fire: e, 6, 6, c, 4
        if (colorLevel < 3100) return format("§e", "§6", "§6", "§c", "§4", "§c", "§c");

        // 3100: 9, 3, 6, e
        if (colorLevel < 3200) return format("§9", "§3", "§6", "§e", "§e", "§6", "§e");

        // 3200: c, 4, 7, 4, c
        if (colorLevel < 3300) return format("§c", "§4", "§7", "§4", "§c", "§4", "§c");

        // 3300: 9, d, c, 4
        if (colorLevel < 3400) return format("§9", "§9", "§d", "§c", "§4", "§4", "§4");

        // 3400: 2, a, d, 5, 2
        if (colorLevel < 3500) return format("§2", "§a", "§d", "§5", "§2", "§2", "§2");

        // 3500: c, 4, 2, a
        if (colorLevel < 3600) return format("§c", "§4", "§2", "§a", "§a", "§a", "§a");

        // 3600: a, b, 9, 1
        if (colorLevel < 3700) return format("§a", "§a", "§b", "§9", "§1", "§1", "§1");

        // 3700: 4, c, b, 3
        if (colorLevel < 3800) return format("§4", "§c", "§b", "§3", "§3", "§3", "§3");

        // 3800: 1, 9, 5, d, 1
        if (colorLevel < 3900) return format("§1", "§9", "§5", "§d", "§1", "§1", "§1");

        // 3900: c, a, 3, 9
        if (colorLevel < 4000) return format("§c", "§a", "§3", "§9", "§1", "§9", "§1");

        // 4000: 5, c, 6, e
        if (colorLevel < 4100) return format("§5", "§c", "§6", "§e", "§e", "§6", "§e");

        // 4100: e, 6, c, d, 5
        if (colorLevel < 4200) return format("§e", "§6", "§c", "§d", "§5", "§5", "§5");

        // 4200: 1, 5, 3, b, ?
        if (colorLevel < 4300) return format("§1", "§9", "§3", "§b", "§f", "§7", "§1");

        // 4300: 0, 5, 8, 5, 0
        if (colorLevel < 4400) return format("§0", "§5", "§8", "§5", "§0", "§5", "§0");

        // 4400: 2, a, e, 6, 5
        if (colorLevel < 4500) return format("§2", "§a", "§e", "§6", "§5", "§d", "§2");

        // 4500: f, b, b, 3, 3
        if (colorLevel < 4600) return format("§f", "§b", "§b", "§3", "§3", "§3", "§3");

        // 4600: 3, b, e, 6, 5
        if (colorLevel < 4700) return format("§3", "§b", "§e", "§6", "§d", "§5", "§5");

        // 4700: f, 4, c, 1, 9
        if (colorLevel < 4800) return format("§f", "§4", "§c", "§9", "§1", "§9", "§1");

        // 4800: 5, c, 6, e, b
        if (colorLevel < 4900) return format("§5", "§c", "§6", "§e", "§b", "§3", "§3");

        // 4900: 0, 2, a, f, 8
        if (colorLevel < 5000) return format("§0", "§2", "§a", "§a", "§f", "§7", "§8");

        // 5000: 4, 5, 9, 1
        return format("§4", "§5", "§9", "§1", "§1", "§1", "§1");
    }

    inline std::string GetFormattedLevel(const Hypixel::PlayerStats& stats)
    {
        return GetFormattedLevel(stats.bedwarsStar, stats.activeStar, stats.activePrestigeScheme, stats.activePrestigeBracket);
    }
}

