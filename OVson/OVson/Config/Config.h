#pragma once
#include <string>
#include <utility>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Config {
bool initialize(HMODULE selfModule);
HMODULE getModuleHandle();
void update();
bool save();
const std::string &getApiKey();
void setApiKey(const std::string &key);

bool isKeylessModeEnabled();
void setKeylessModeEnabled(bool enabled);

// overlay mode: "gui", "chat", "invisible"
const std::string &getOverlayMode();
void setOverlayMode(const std::string &mode);

// AutoGG settings
bool isAutoGGEnabled();
void setAutoGGEnabled(bool enabled);
const std::string &getAutoGGMessage();
void setAutoGGMessage(const std::string &msg);

bool isTabEnabled();
void setTabEnabled(bool enabled);

bool isBetterTabModeEnabled();
void setBetterTabModeEnabled(bool enabled);

float getBetterTabX();
void setBetterTabX(float x);
float getBetterTabY();
void setBetterTabY(float y);
float getBetterTabScale();
void setBetterTabScale(float scale);

bool isPreGameChatStatsEnabled();
void setPreGameChatStatsEnabled(bool enabled);
const std::string &getSortMode(); // General sort metric (Stars, FKDR, etc.)
void setSortMode(const std::string &mode);

const std::string &getTabDisplayMode(); // What stat shows in Tab
void setTabDisplayMode(const std::string &mode);

bool isTabSortDescending();
void setTabSortDescending(bool desc);

bool isDebugging();
void setDebugging(bool enabled);

// bed defense settings
bool isBedDefenseEnabled();
void setBedDefenseEnabled(bool enabled);

bool isNickedBypass();
void setNickedBypass(bool enabled);

// click gui settings
int getClickGuiKey();
void setClickGuiKey(int key);
int getUninjectKey();
void setUninjectKey(int key);
bool isUninjectKeyEnabled();
void setUninjectKeyEnabled(bool enabled);

bool isNotificationsEnabled();
void setNotificationsEnabled(bool enabled);

bool isClickGuiOn();
void setClickGuiOn(bool on);

// commands toggle
bool isCommandsEnabled();
void setCommandsEnabled(bool enabled);

// theme customization
DWORD getThemeColor();
void setThemeColor(DWORD color);

// motion blur (gonna make this work one day)
bool isMotionBlurEnabled();
void setMotionBlurEnabled(bool enabled);
float getMotionBlurAmount();
void setMotionBlurAmount(float amount);

bool isNameTagsEnabled();
void setNameTagsEnabled(bool enabled);
float getNameTagHeight();
void setNameTagHeight(float h);

std::vector<std::pair<std::string, bool>> getNameTagStats();
void setNameTagStats(const std::vector<std::pair<std::string, bool>> &stats);

// tags general
bool isTagsEnabled();
void setTagsEnabled(bool enabled);
const std::string &getActiveTagService();
void setActiveTagService(const std::string &service);
bool isMuteTagAlertsEnabled();
void setMuteTagAlertsEnabled(bool enabled);
const std::vector<std::string> &getMutedTagPlayers();
void addMutedTagPlayer(const std::string &name);
void removeMutedTagPlayer(const std::string &name);
bool isMuteSelfTagAlertsEnabled();
void setMuteSelfTagAlertsEnabled(bool enabled);

// ClickGUI visual theme: "LiquidGlass" (default) | "Minimal".
const std::string &getClickGuiTheme();
void setClickGuiTheme(const std::string &theme);
const std::string &getClickGuiLayout();
void setClickGuiLayout(const std::string &layout);
const std::string &getLayoutBData();
void setLayoutBData(const std::string &data);

bool isLiquidGlassWiggleEnabled();
void setLiquidGlassWiggleEnabled(bool enabled);

bool isLiquidGlassGlowEnabled();
void setLiquidGlassGlowEnabled(bool enabled);

float getLiquidGlassRefractStrength();
void setLiquidGlassRefractStrength(float str);

float getLiquidGlassEdgeWidth();
void setLiquidGlassEdgeWidth(float w);

float getLiquidGlassCardEdgeWidth();
void setLiquidGlassCardEdgeWidth(float w);

float getLiquidGlassDarkness();
void setLiquidGlassDarkness(float d);

bool isChatBypasserEnabled();
void setChatBypasserEnabled(bool enabled);

bool isSmartChatBypassEnabled();
void setSmartChatBypassEnabled(bool enabled);

// overlay
bool isOvShowStar();
void setOvShowStar(bool show);
bool isOvShowFk();
void setOvShowFk(bool show);
bool isOvShowFkdr();
void setOvShowFkdr(bool show);
bool isOvShowWins();
void setOvShowWins(bool show);
bool isOvShowWlr();
void setOvShowWlr(bool show);
bool isOvShowWs();
void setOvShowWs(bool show);
bool isOvShowKills();
void setOvShowKills(bool show);
bool isOvShowKdr();
void setOvShowKdr(bool show);
bool isOvShowBeds();
void setOvShowBeds(bool show);
bool isOvShowBlr();
void setOvShowBlr(bool show);
bool isOvShowPing();
void setOvShowPing(bool show);
bool isOvShowTags();
void setOvShowTags(bool show);

// bettertab
bool isProShowStar();
void setProShowStar(bool show);
bool isProShowFk();
void setProShowFk(bool show);
bool isProShowFkdr();
void setProShowFkdr(bool show);
bool isProShowWins();
void setProShowWins(bool show);
bool isProShowWlr();
void setProShowWlr(bool show);
bool isProShowWs();
void setProShowWs(bool show);
bool isProShowKills();
void setProShowKills(bool show);
bool isProShowKdr();
void setProShowKdr(bool show);
bool isProShowBeds();
void setProShowBeds(bool show);
bool isProShowBlr();
void setProShowBlr(bool show);
bool isProShowPing();
void setProShowPing(bool show);
bool isProShowTags();
void setProShowTags(bool show);
bool isProShowHp();
void setProShowHp(bool show);

// deprecated
bool isShowStar();
void setShowStar(bool show);
bool isShowFk();
void setShowFk(bool show);
bool isShowFkdr();
void setShowFkdr(bool show);
bool isShowWins();
void setShowWins(bool show);
bool isShowWlr();
void setShowWlr(bool show);
bool isShowWs();
void setShowWs(bool show);
bool isShowKills();
void setShowKills(bool show);
bool isShowKdr();
void setShowKdr(bool show);
bool isShowBeds();
void setShowBeds(bool show);
bool isShowBlr();
void setShowBlr(bool show);
bool isShowPing();
void setShowPing(bool show);

// urchin tags
bool isUrchinEnabled();
void setUrchinEnabled(bool enabled);
const std::string &getUrchinApiKey();
void setUrchinApiKey(const std::string &key);

// seraph tags
bool isSeraphEnabled();
void setSeraphEnabled(bool enabled);
const std::string &getSeraphApiKey();
void setSeraphApiKey(const std::string &key);

// aurora API (number denicker)
const std::string &getAuroraApiKey();
void setAuroraApiKey(const std::string &key);
bool isNumberDenickerEnabled();
void setNumberDenickerEnabled(bool enabled);
int getPingDisplayMode();
void setPingDisplayMode(int mode);

// granular debugging
enum class DebugCategory {
  General,
  GameDetection,
  BedDetection,
  Urchin,
  Seraph,
  GUI,
  BedDefense
};

bool isDebugEnabled(DebugCategory cat);
void setDebugEnabled(DebugCategory cat, bool enabled);
bool isGlobalDebugEnabled();
void setGlobalDebugEnabled(bool enabled);
bool isDiscordRpcEnabled();
void setDiscordRpcEnabled(bool enabled);
const std::string &getDiscordAppId();
void setDiscordAppId(const std::string &id);

// T info
bool isTechEnabled();
void setTechEnabled(bool enabled);
float getTechX();
void setTechX(float x);
float getTechY();
void setTechY(float y);

// team stats
bool isTeamReportEnabled();
void setTeamReportEnabled(bool enabled);
const std::string &getTeamReportChannel();
void setTeamReportChannel(const std::string &channel);

const std::string &getCommandPrefix();
void setCommandPrefix(const std::string &prefix);

bool isForgeEnvironment();

// ac
bool isAnticheatEnabled();
void setAnticheatEnabled(bool e);
bool isAnticheatNoSlowEnabled();
void setAnticheatNoSlowEnabled(bool e);
bool isAnticheatAutoBlockEnabled();
void setAnticheatAutoBlockEnabled(bool e);
bool isAnticheatEagleEnabled();
void setAnticheatEagleEnabled(bool e);
bool isAnticheatScaffoldEnabled();
void setAnticheatScaffoldEnabled(bool e);
bool isAnticheatCheckSelfEnabled();
void setAnticheatCheckSelfEnabled(bool e);
int getAnticheatVl();
void setAnticheatVl(int vl);
int getAnticheatCooldownSec();
void setAnticheatCooldownSec(int sec);
} // namespace Config
