#include "ClickGUI.h"
#include "State.h"
#include "ClickGUI_Bridge.h"
#include "Helpers.h"
#include "../Render/NotificationManager.h"
#include "../Config/Config.h"
#include "../Services/AbyssService.h"
#include "../Services/PrismService.h"
#include "../Services/Hypixel.h"
#include "../Services/KhadowService.h"
#include "../Services/SeraphService.h"
#include "../Services/UrchinService.h"
#include "../Net/Http.h"
#include "../Utils/SafeGuard.h"
#include "../Utils/stb_image.h"
#include <Windows.h>
#include <string>
#include <thread>

namespace Render {

using namespace ClickGUIState;

namespace {

bool hasActiveNativeInput() {
  return s_typingSearch || s_typingModuleSearch || s_typingApiKey || s_typingAutoGG ||
         s_typingUrchinKey || s_typingSeraphKey ||
         s_typingAuroraApiKey || s_typingPrefix ||
         s_typingMuteTagPlayer || s_typingNickRollTarget;
}

std::string *activeNativeInput(int &cap) {
  cap = 48;
  if (s_typingSearch) return &s_playerSearch;
  if (s_typingModuleSearch) return &s_moduleSearch;
  if (s_typingApiKey) return &s_apiKeyInput;
  if (s_typingAutoGG) {
    cap = 100;
    return &s_autoGGInput;
  }
  if (s_typingUrchinKey) {
    cap = 100;
    return &s_urchinKeyInput;
  }
  if (s_typingSeraphKey) {
    cap = 100;
    return &s_seraphKeyInput;
  }
  if (s_typingAuroraApiKey) {
    cap = 100;
    return &s_auroraApiKeyInput;
  }
  if (s_typingPrefix) {
    cap = 1;
    return &s_prefixInput;
  }
  if (s_typingMuteTagPlayer) {
    cap = 16;
    return &s_muteTagPlayerInput;
  }
  if (s_typingNickRollTarget) {
    cap = 16;
    return &s_nickRollTargetInput;
  }
  return nullptr;
}

} // namespace

namespace ClickGUIState {

void triggerPlayerSearch(const std::string &searchName) {
  if (searchName.empty()) return;
  std::string key = s_apiKeyInput;
  if (key.empty() || key == "None")
    key = Config::getApiKey();
  bool keyless = Config::isKeylessModeEnabled();

  bool hasValidKey = (!key.empty() && key != "None");

  s_searching = true;
  s_hasLookup = false;
  s_lookupUrchinMonthly = std::nullopt;
  NotificationManager::getInstance()->add(
      "Stats", "Fetching player ID...", NotificationType::Info);

  std::thread([searchName, key, keyless, hasValidKey]() {
    SafeGuard::installSehTranslator();
    SafeGuard::run("ClickGUI::statsLookup", [&]() {
      std::string exactName;
      auto uuidOpt = Hypixel::getUuidByName(searchName, &exactName);
      if (uuidOpt) {
        NotificationManager::getInstance()->add(
            "Stats", "ID found, fetching stats...",
            NotificationType::Info);
        std::optional<Hypixel::PlayerStats> statsOpt;
        if (!keyless && hasValidKey) {
          statsOpt = Hypixel::getPlayerStats(key, *uuidOpt);
          if (!statsOpt) {
            statsOpt = AbyssService::getPlayerStats(*uuidOpt);
          }
          if (!statsOpt) {
            statsOpt = PrismService::getPlayerStats(*uuidOpt);
          }
        } else {
          statsOpt = AbyssService::getPlayerStats(*uuidOpt);
          if (!statsOpt) {
            statsOpt = PrismService::getPlayerStats(*uuidOpt);
          }
          if (!statsOpt && hasValidKey) {
            statsOpt = Hypixel::getPlayerStats(key, *uuidOpt);
          }
        }
        if (statsOpt) {
          s_lookupResult = *statsOpt;
          if (s_lookupResult.displayName.empty() && !exactName.empty()) {
            s_lookupResult.displayName = exactName;
          }
          s_lookupName = !s_lookupResult.displayName.empty() ? s_lookupResult.displayName : (!exactName.empty() ? exactName : searchName);
          s_lookupUrchinTags = std::nullopt;
          s_lookupSeraphTags = std::nullopt;
          s_lookupUrchinMonthly = std::nullopt;
          s_tagsFetched = false;
          s_hasLookup = true;

          std::string urchinKey = Config::getUrchinApiKey();
          if (!urchinKey.empty()) {
            std::string monthlyTarget = !s_lookupResult.uuid.empty() ? s_lookupResult.uuid : (!exactName.empty() ? exactName : searchName);
            std::thread([monthlyTarget]() {
              SafeGuard::installSehTranslator();
              SafeGuard::run("ClickGUI::monthlyFetch", [&]() {
                auto m = Urchin::getMonthlyStats(monthlyTarget, true);
                if (m && m->hasData) {
                  s_lookupUrchinMonthly = m;
                }
              });
            }).detach();
          }

          if (Config::isTagsEnabled()) {
            std::string uuid = s_lookupResult.uuid;
            std::thread([searchName, uuid]() {
              SafeGuard::installSehTranslator();
              SafeGuard::run("ClickGUI::tagFetch", [&]() {
                std::string activeS = Config::getActiveTagService();
                if (activeS == "Khadow") {
                  auto kh = Khadow::getPlayerAnticheat(searchName,
                                                       true);
                  if (kh) {
                    if (kh->urchinBlacklisted) {
                      Urchin::PlayerTags ut;
                      ut.uuid = uuid;
                      Urchin::Tag t;
                      t.type   = kh->urchinType;
                      t.reason = kh->urchinReason;
                      ut.tags.push_back(t);
                      s_lookupUrchinTags = ut;
                    }
                    if (kh->seraphBlacklisted) {
                      Seraph::PlayerTags st;
                      st.uuid = uuid;
                      Seraph::Tag t;
                      t.type   = kh->seraphType;
                      t.reason = kh->seraphReason;
                      st.tags.push_back(t);
                      s_lookupSeraphTags = st;
                    }
                  }
                }
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
          std::string skinTargetName = s_lookupResult.displayName.empty() ? searchName : s_lookupResult.displayName;
          if (!uuid.empty() && uuid != s_lookupSkinUuid) {
            s_skinLoading = true;
            s_skinPendingReady = false;
            s_headPendingReady = false;
            std::thread([skinTargetName, uuid]() {
              SafeGuard::installSehTranslator();
              SafeGuard::run("ClickGUI::skinFetch", [&]() {
                std::string cleanUuid;
                for (char c : uuid) if (c != '-') cleanUuid += c;

                std::string officialSkinUrl;
                bool mojangDetectedSlim = false;
                bool hasMojangModel = false;

                auto base64Decode = [](const std::string &in) -> std::string {
                  std::string out;
                  std::vector<int> T(256, -1);
                  for (int i = 0; i < 64; i++) {
                    T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
                  }
                  int val = 0, valb = -8;
                  for (unsigned char c : in) {
                    if (T[c] == -1) break;
                    val = (val << 6) + T[c];
                    valb += 6;
                    if (valb >= 0) {
                      out.push_back(char((val >> valb) & 0xFF));
                      valb -= 8;
                    }
                  }
                  return out;
                };

                if (cleanUuid.size() == 32) {
                  std::string mojangUrl = "https://sessionserver.mojang.com/session/minecraft/profile/" + cleanUuid;
                  std::string mojangBody;
                  if (Http::get(mojangUrl, mojangBody, "", "", "Mozilla/5.0")) {
                    size_t valPos = mojangBody.find("\"value\"");
                    if (valPos != std::string::npos) {
                      size_t q1 = mojangBody.find('"', valPos + 7);
                      if (q1 != std::string::npos) {
                        size_t q2 = mojangBody.find('"', q1 + 1);
                        if (q2 != std::string::npos) {
                          std::string b64 = mojangBody.substr(q1 + 1, q2 - (q1 + 1));
                          std::string decoded = base64Decode(b64);
                          hasMojangModel = true;
                          if (decoded.find("\"model\"") != std::string::npos &&
                              decoded.find("\"slim\"") != std::string::npos) {
                            mojangDetectedSlim = true;
                          }
                          size_t skinPos = decoded.find("\"SKIN\"");
                          if (skinPos != std::string::npos) {
                            size_t uPos = decoded.find("\"url\"", skinPos);
                            if (uPos != std::string::npos) {
                              size_t uq1 = decoded.find('"', uPos + 5);
                              if (uq1 != std::string::npos) {
                                size_t uq2 = decoded.find('"', uq1 + 1);
                                if (uq2 != std::string::npos) {
                                  officialSkinUrl = decoded.substr(uq1 + 1, uq2 - (uq1 + 1));
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }

                std::string skinData;
                bool skinFetched = false;
                if (!officialSkinUrl.empty()) {
                  skinFetched = Http::get(officialSkinUrl, skinData, "", "", "Mozilla/5.0") && skinData.size() > 100;
                }
                if (!skinFetched) {
                  std::string skinUrl = "https://minotar.net/skin/" + skinTargetName;
                  if (!Http::get(skinUrl, skinData, "", "", "Mozilla/5.0") || skinData.size() < 100) {
                    skinUrl = "https://mc-heads.net/skin/" + skinTargetName;
                    Http::get(skinUrl, skinData, "", "", "Mozilla/5.0");
                  }
                }

                if (skinData.size() > 100 && skinData.size() < 1024 * 1024) {
                  int w = 0, h = 0, ch = 0;
                  unsigned char *px = stbi_load_from_memory(
                      (const stbi_uc *)skinData.data(), (int)skinData.size(),
                      &w, &h, &ch, STBI_rgb_alpha);
                  if (px && w > 0 && h > 0) {
                    bool slim = false;
                    if (hasMojangModel) {
                      slim = mojangDetectedSlim;
                    } else if (h >= 64 && w >= 64) {
                      int transparentCount = 0;
                      int totalChecked = 0;
                      for (int vy = 20; vy < 32; vy++) {
                        for (int ux = 54; ux <= 55; ux++) {
                          int idx = (vy * w + ux) * 4;
                          if (idx + 3 < w * h * 4) {
                            totalChecked++;
                            if (px[idx + 3] == 0) transparentCount++;
                          }
                        }
                      }
                      for (int vy = 52; vy < 64; vy++) {
                        for (int ux = 46; ux <= 47; ux++) {
                          int idx = (vy * w + ux) * 4;
                          if (idx + 3 < w * h * 4) {
                            totalChecked++;
                            if (px[idx + 3] == 0) transparentCount++;
                          }
                        }
                      }
                      slim = (totalChecked > 0 && transparentCount >= (totalChecked * 9) / 10);
                    }
                    s_skinIsSlim = slim;
                    s_skinPendingData.assign(px, px + w * h * 4);
                    s_skinPendingW = w;
                    s_skinPendingH = h;
                    s_lookupSkinUuid = uuid;
                    s_skinPendingReady = true;
                  }
                  if (px)
                    stbi_image_free(px);
                }

                std::string headUrl = "https://mc-heads.net/avatar/" + skinTargetName + "/64";
                std::string headData;
                if (!Http::get(headUrl, headData, "", "", "Mozilla/5.0") || headData.size() < 100) {
                  headUrl = "https://api.mcheads.org/head/" + skinTargetName + "/64";
                  Http::get(headUrl, headData, "", "", "Mozilla/5.0");
                }
                if (headData.size() > 100 && headData.size() < 256 * 1024) {
                  int hw = 0, hh = 0, hch = 0;
                  unsigned char *hpx = stbi_load_from_memory(
                      (const stbi_uc *)headData.data(), (int)headData.size(),
                      &hw, &hh, &hch, STBI_rgb_alpha);
                  if (hpx && hw > 0 && hh > 0) {
                    s_headPendingData.assign(hpx, hpx + hw * hh * 4);
                    s_headPendingW = hw;
                    s_headPendingH = hh;
                    s_headPendingReady = true;
                  }
                  if (hpx)
                    stbi_image_free(hpx);
                }

                s_skinLoading = false;
              });
            }).detach();
          }
        } else {
          NotificationManager::getInstance()->add(
              "Hypixel",
              keyless ? "Abyss / Prism API failed"
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

} // namespace ClickGUIState

void ClickGUI::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  if (!s_open)
    return;

  if (ClickGUIHelpers::handleEditorMessage(msg, wParam, lParam))
    return;

  if (msg == WM_MOUSEWHEEL) {
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (Config::getClickGuiLayout() == "B") {
      POINT pt;
      GetCursorPos(&pt);
      HWND hwnd = GetActiveWindow();
      if (hwnd) {
        ScreenToClient(hwnd, &pt);
        ClickGUI::handleScrollB((float)pt.x, (float)pt.y, (int)delta);
      }
    } else {
      s_targetScroll -= (float)delta * 0.5f;
    }
    return;
  }

  if (msg == WM_KEYDOWN) {
    if (wParam == VK_ESCAPE) {
      toggle();
      return;
    }
    if ((wParam == 'V') && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
      ClickGUIBridge::CustomJavaSetting* activeJavaSetting = nullptr;
      for (auto &mod : const_cast<std::vector<ClickGUIBridge::CustomJavaModule>&>(ClickGUIBridge::getCachedModules())) {
        for (auto &set : mod.settings) {
          if (set.typingState) {
            activeJavaSetting = &set;
            break;
          }
        }
        if (activeJavaSetting) break;
      }

      if (activeJavaSetting || hasActiveNativeInput()) {
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

              std::string *target = nullptr;
              int cap = 100;
              if (activeJavaSetting) {
                target = &activeJavaSetting->inputBuf;
                cap = 100;
              } else {
                target = activeNativeInput(cap);
              }

              if (target && target->length() + filtered.length() <=
                                static_cast<std::size_t>(cap)) {
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
    ClickGUIBridge::CustomJavaSetting* activeJavaSetting = nullptr;
    for (auto &mod : const_cast<std::vector<ClickGUIBridge::CustomJavaModule>&>(ClickGUIBridge::getCachedModules())) {
      for (auto &set : mod.settings) {
        if (set.typingState) {
          activeJavaSetting = &set;
          break;
        }
      }
      if (activeJavaSetting) break;
    }

    if (activeJavaSetting || hasActiveNativeInput()) {
      
      std::string *target = nullptr;
      int cap = 100;

      if (activeJavaSetting) {
        target = &activeJavaSetting->inputBuf;
        cap = 100;
      } else {
        target = activeNativeInput(cap);
      }

      if (c == 8) {
        if (target && !target->empty())
          target->pop_back();
      } else if (c == 13) {
        if (activeJavaSetting) {
          ClickGUIBridge::setInputValue(activeJavaSetting->settingObj, activeJavaSetting->inputBuf);
          NotificationManager::getInstance()->add(
              activeJavaSetting->name, "Saved: " + activeJavaSetting->inputBuf,
              NotificationType::Success);
          activeJavaSetting->typingState = false;
        } else {
          if (s_typingSearch && !s_playerSearch.empty()) {
            triggerPlayerSearch(s_playerSearch);
            s_typingSearch = false;
          }
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
        if (s_typingMuteTagPlayer) {
          Config::addMutedTagPlayer(s_muteTagPlayerInput);
          NotificationManager::getInstance()->add("Tags", "Player added to mute list: " + s_muteTagPlayerInput,
                                                  NotificationType::Success);
          s_muteTagPlayerInput.clear();
          s_typingMuteTagPlayer = false;
        }
        if (s_typingNickRollTarget) {
          Config::setNickRollTargetWord(s_nickRollTargetInput);
          s_nickRollTargetInput = Config::getNickRollTargetWord();
          NotificationManager::getInstance()->add(
              "Nick Roll",
              s_nickRollTargetInput.empty()
                  ? "Target cleared; using score threshold"
                  : "Target saved: " + s_nickRollTargetInput,
              NotificationType::Success);
          s_typingNickRollTarget = false;
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

} // namespace Render

