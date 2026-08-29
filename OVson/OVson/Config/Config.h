#pragma once
#include <string>
#include <utility>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Config {
bool initialize(HMODULE selfModule);
HMODULE getModuleHandle();
std::string getDataDirectory();
void update();
bool save();
bool saveNow();
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

bool isLobbyMentionStatsEnabled();
void setLobbyMentionStatsEnabled(bool enabled);

bool isChatStatsEnabled();
void setChatStatsEnabled(bool enabled);
const std::string &getChatStatsFormat();
void setChatStatsFormat(const std::string &format);
const std::string &getChatStatsStyle();
void setChatStatsStyle(const std::string &style);

const std::string &getLunarLogPath();
void setLunarLogPath(const std::string &path);

const std::string &getBadlionLogPath();
void setBadlionLogPath(const std::string &path);

const std::string &getLegacyBadlionLogPath();
void setLegacyBadlionLogPath(const std::string &path);

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

bool isRawMouseFixEnabled();
void setRawMouseFixEnabled(bool enabled);

// Client-side heuristic: correlates server hurt/health/velocity packets with a
// nearby player swing while the local player is sword-blocking. It is not a
// server-confirmed block result.
bool isBlockHitSoundEnabled();
void setBlockHitSoundEnabled(bool enabled);
bool isBlockHitSoundDebugEnabled();
void setBlockHitSoundDebugEnabled(bool enabled);
const std::string &getBlockHitSoundSource();
void setBlockHitSoundSource(const std::string &source);
const std::string &getBlockHitSoundFilename();
void setBlockHitSoundFilename(const std::string &filename);
float getBlockHitSoundVolume();
void setBlockHitSoundVolume(float volumePercent);
// On (the default): wait for the health drop or knockback packet that says the
// server registered the hit. Off: a hurt while blocking with a sword is enough
// on its own -- immediate, but it also fires on damage no player swung for.
bool isBlockHitWaitForServerEnabled();
void setBlockHitWaitForServerEnabled(bool enabled);

// Now-playing overlay. Reads whatever Windows' media session reports -- Spotify,
// a browser tab, any player that registers a transport session. Position is
// stored normalised (0..1) so it survives a resolution change.
bool isMediaOverlayEnabled();
void setMediaOverlayEnabled(bool enabled);
float getMediaOverlayX();
void setMediaOverlayX(float x);
float getMediaOverlayY();
void setMediaOverlayY(float y);
float getMediaOverlayScale();
void setMediaOverlayScale(float scale);
// 0 = wide landscape card, 1 = compact portrait card.
int getMediaOverlayLayout();
void setMediaOverlayLayout(int layout);
// Card appearance. Colours are stored as 0xRRGGBB; opacity is separate so the
// background can be tinted and faded independently.
unsigned long getMediaOverlayBgColor();
void setMediaOverlayBgColor(unsigned long rgb);
unsigned long getMediaOverlayAccentColor();
void setMediaOverlayAccentColor(unsigned long rgb);
unsigned long getMediaOverlayTextColor();
void setMediaOverlayTextColor(unsigned long rgb);
float getMediaOverlayOpacity();
void setMediaOverlayOpacity(float opacity);
float getMediaOverlayCorner();
void setMediaOverlayCorner(float radius);
bool isMediaOverlayArtEnabled();
void setMediaOverlayArtEnabled(bool enabled);

// /nick book scoring alerts. The scorer's fitted weights are intentionally
// fixed; only the pass line and notification behavior are user-configurable.
int getNickScoreThreshold();
void setNickScoreThreshold(int threshold);
bool isNickRollEnabled();
void setNickRollEnabled(bool enabled);
bool isNickScorePingEnabled();
void setNickScorePingEnabled(bool enabled);
bool isNickScoreAlertEveryEnabled();
void setNickScoreAlertEveryEnabled(bool enabled);
// Auto-reroll presses TRY AGAIN for a name that missed the threshold, using
// the command the page itself puts behind that button. Off by default.
bool isNickRollAutoRerollEnabled();
void setNickRollAutoRerollEnabled(bool enabled);
int getNickRollRerollDelayMs();
void setNickRollRerollDelayMs(int milliseconds);
int getNickRollRerollCap();
void setNickRollRerollCap(int cap);
// Virtual-key code that toggles nick scoring on and off. Polled directly so it
// works with the /nick book open, which is the only moment it is useful.
int getNickRollToggleKey();
void setNickRollToggleKey(int virtualKey);
const std::string &getNickRollTargetWord();
void setNickRollTargetWord(const std::string &targetWord);

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

bool isChromaEnabled();
void setChromaEnabled(bool enabled);

float getChromaSpeed();
void setChromaSpeed(float speed);

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
// Silences tag warnings for anyone resolved onto the local player's own team.
// Team membership comes from the scoreboard team a player belongs to, with the
// armour-colour table as the fallback -- the same resolution the nametags and
// the tab list already use.
bool isMuteTeamTagAlertsEnabled();
void setMuteTeamTagAlertsEnabled(bool enabled);

// ClickGUI visual theme: "LiquidGlass" (default) | "Minimal".
const std::string &getClickGuiTheme();
void setClickGuiTheme(const std::string &theme);
const std::string &getClickGuiLayout();
void setClickGuiLayout(const std::string &layout);
float getClickGuiX();
float getClickGuiY();
float getClickGuiWidth();
float getClickGuiHeight();
void setClickGuiBounds(float x, float y, float width, float height);
const std::string &getLayoutBData();
void setLayoutBData(const std::string &data);
const std::string &getBedwarsSettingsData();
void setBedwarsSettingsData(const std::string &data);

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

std::string getSpoofIp();
void setSpoofIp(const std::string& ip);
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
