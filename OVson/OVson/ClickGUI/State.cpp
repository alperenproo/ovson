#include "State.h"

namespace Render {
namespace ClickGUIState {

bool        s_open = false;
bool        s_init = false;
FontRenderer g_guiFont;
float       s_animAlpha = 0.0f;
float       s_targetAlpha = 0.0f;
float       s_openingScale = 0.95f;

int   s_activeTab = 0;
int   s_targetTab = 0;
float s_tabIndicatorY = 80.0f;
float s_contentSlide = 0.0f;
float s_contentAlpha = 1.0f;

bool s_lastLButton = false;
bool s_lastInsert = false;
bool s_lastBackspace = false;

std::string s_playerSearch;
std::string s_apiKeyInput;
std::string s_autoGGInput;
std::string s_urchinKeyInput;
std::string s_seraphKeyInput;
std::string s_auroraApiKeyInput;
std::string s_prefixInput = ".";
std::string s_muteTagPlayerInput;

bool s_typingSearch = false;
bool s_typingApiKey = false;
bool s_typingAutoGG = false;
bool s_typingUrchinKey = false;
bool s_typingSeraphKey = false;
bool s_typingAuroraApiKey = false;
bool s_typingPrefix = false;
bool s_typingMuteTagPlayer = false;

float s_scrollOffset = 0.0f;
float s_targetScroll = 0.0f;
float s_maxScroll = 0.0f;

bool  s_isDropdownOpen = false;
float s_dropdownAnim = 0.0f;
bool  s_isTagsDropdownOpen = false;
float s_tagsDropdownAnim = 0.0f;
bool  s_isSortOrderDropdownOpen = false;
float s_sortOrderDropdownAnim = 0.0f;
bool  s_isPingModeDropdownOpen = false;
float s_pingModeDropdownAnim = 0.0f;
bool  s_isColumnTargetDropdownOpen = false;
float s_columnTargetDropdownAnim = 0.0f;
bool  s_isSortColumnDropdownOpen = false;
float s_sortColumnDropdownAnim = 0.0f;
bool  s_isTabDisplayDropdownOpen = false;
float s_tabDisplayDropdownAnim = 0.0f;

float s_accentHue = 224.0f / 360.0f;  // spec navy default
float s_accentSat = 0.90f;
float s_accentVal = 0.96f;
bool  s_accentDragSV = false;
bool  s_accentDragHue = false;
bool  s_accentInit = false;
bool  s_chromaEnabled = false;
float s_chromaSpeed = 60.0f;

int   s_colorSelectedStat = 0;
bool  s_colorPickerOpen = false;
float s_cpHue = 0.0f;
float s_cpSat = 1.0f;
float s_cpVal = 1.0f;
bool  s_cpDraggingSV = false;
bool  s_cpDraggingHue = false;
char  s_cpMinBuf[16] = "0";
char  s_cpMaxBuf[16] = "100";
int   s_cpMinLen = 1;
int   s_cpMaxLen = 3;
int   s_cpEditingField = 0;
int   s_cpEditRangeIdx = -1;

int                                    s_columnTargetMode = 0;
Hypixel::PlayerStats                   s_lookupResult;
bool                                   s_hasLookup = false;
bool                                   s_searching = false;
std::string                            s_lookupName;
std::optional<Urchin::PlayerTags>      s_lookupUrchinTags;
std::optional<Seraph::PlayerTags>      s_lookupSeraphTags;
std::atomic<bool>                      s_tagsFetched{false};
GLuint                                 s_lookupSkinTexId = 0;
std::string                            s_lookupSkinUuid;
std::atomic<bool>                      s_skinLoading{false};
std::vector<uint8_t>                   s_skinPendingData;
int                                    s_skinPendingW = 0;
int                                    s_skinPendingH = 0;
std::atomic<bool>                      s_skinPendingReady{false};

float g_x = 100.0f;
float g_y = 100.0f;
float g_w = 920.0f;
float g_h = 600.0f;
bool  s_dragging = false;
float s_dragOffsetX = 0.0f;
float s_dragOffsetY = 0.0f;
bool  s_waitingForKey = false;
bool  s_waitingForUninjectKey = false;

SwitchAnim s_switches[50];

} // namespace ClickGUIState
} // namespace Render
