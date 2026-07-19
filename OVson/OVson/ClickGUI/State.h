#pragma once
#include "../Render/FontRenderer.h"
#include "../Services/Hypixel.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include <Windows.h>
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render {
namespace ClickGUIState {

extern bool        s_open;
extern bool        s_init;
extern FontRenderer g_guiFont;
extern float       s_animAlpha;
extern float       s_targetAlpha;
extern float       s_openingScale;

extern int   s_activeTab;
extern int   s_targetTab;
extern float s_tabIndicatorY;
extern float s_contentSlide;
extern float s_contentAlpha;

extern bool s_lastLButton;
extern bool s_lastInsert;
extern bool s_lastBackspace;

extern std::string s_playerSearch;
extern std::string s_apiKeyInput;
extern std::string s_autoGGInput;
extern std::string s_urchinKeyInput;
extern std::string s_seraphKeyInput;
extern std::string s_auroraApiKeyInput;
extern std::string s_prefixInput;
extern std::string s_muteTagPlayerInput;

extern bool s_typingSearch;
extern bool s_typingApiKey;
extern bool s_typingAutoGG;
extern bool s_typingUrchinKey;
extern bool s_typingSeraphKey;
extern bool s_typingAuroraApiKey;
extern bool s_typingPrefix;
extern bool s_typingMuteTagPlayer;

extern float s_scrollOffset;
extern float s_targetScroll;
extern float s_maxScroll;

extern bool  s_isDropdownOpen;
extern float s_dropdownAnim;
extern bool  s_isTagsDropdownOpen;
extern float s_tagsDropdownAnim;
extern bool  s_isSortOrderDropdownOpen;
extern float s_sortOrderDropdownAnim;
extern bool  s_isPingModeDropdownOpen;
extern float s_pingModeDropdownAnim;
extern bool  s_isColumnTargetDropdownOpen;
extern float s_columnTargetDropdownAnim;
extern bool  s_isSortColumnDropdownOpen;
extern float s_sortColumnDropdownAnim;
extern bool  s_isTabDisplayDropdownOpen;
extern float s_tabDisplayDropdownAnim;

extern float s_accentHue;     // 0..1
extern float s_accentSat;     // 0..1
extern float s_accentVal;     // 0..1
extern bool  s_accentDragSV;
extern bool  s_accentDragHue;
extern bool  s_accentInit;
extern bool  s_chromaEnabled;
extern float s_chromaSpeed;   // degrees / second

extern int   s_colorSelectedStat;
extern bool  s_colorPickerOpen;
extern float s_cpHue;
extern float s_cpSat;
extern float s_cpVal;
extern bool  s_cpDraggingSV;
extern bool  s_cpDraggingHue;
extern char  s_cpMinBuf[16];
extern char  s_cpMaxBuf[16];
extern int   s_cpMinLen;
extern int   s_cpMaxLen;
extern int   s_cpEditingField;  // 0=none, 1=min, 2=max
extern int   s_cpEditRangeIdx;  // -1 = new, >=0 = editing existing

extern int                                    s_columnTargetMode;
extern Hypixel::PlayerStats                   s_lookupResult;
extern bool                                   s_hasLookup;
extern bool                                   s_searching;
extern std::string                            s_lookupName;
extern std::optional<Urchin::PlayerTags>      s_lookupUrchinTags;
extern std::optional<Seraph::PlayerTags>      s_lookupSeraphTags;
extern std::atomic<bool>                      s_tagsFetched;
extern GLuint                                 s_lookupSkinTexId;
extern std::string                            s_lookupSkinUuid;
extern std::atomic<bool>                      s_skinLoading;
extern std::vector<uint8_t>                   s_skinPendingData;
extern int                                    s_skinPendingW;
extern int                                    s_skinPendingH;
extern std::atomic<bool>                      s_skinPendingReady;

extern float g_x;
extern float g_y;
extern float g_w;
extern float g_h;
extern bool  s_dragging;
extern float s_dragOffsetX;
extern float s_dragOffsetY;
extern bool  s_waitingForKey;
extern bool  s_waitingForUninjectKey;

struct SwitchAnim {
  float currX = 0.0f;
  float targetX = 0.0f;
};
extern SwitchAnim s_switches[50];

} // namespace ClickGUIState
} // namespace Render
