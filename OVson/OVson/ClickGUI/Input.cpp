#include "ClickGUI.h"
#include "State.h"
#include "ClickGUI_Bridge.h"
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

void ClickGUI::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  if (!s_open)
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

      if (activeJavaSetting || s_typingSearch || s_typingApiKey || s_typingAutoGG ||
          s_typingUrchinKey || s_typingSeraphKey || s_typingAuroraApiKey ||
          s_typingPrefix || s_typingMuteTagPlayer) {
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
                target =
                s_typingSearch
                    ? &s_playerSearch
                    : (s_typingApiKey
                           ? &s_apiKeyInput
                           : (s_typingAutoGG
                                  ? &s_autoGGInput
                                  : (s_typingUrchinKey
                                         ? &s_urchinKeyInput
                                         : (s_typingSeraphKey
                                                ? &s_seraphKeyInput
                                                : (s_typingAuroraApiKey
                                                       ? &s_auroraApiKeyInput
                                                       : (s_typingPrefix
                                                              ? &s_prefixInput
                                                              : &s_muteTagPlayerInput))))));
                cap = (s_typingAutoGG || s_typingUrchinKey ||
                       s_typingSeraphKey || s_typingAuroraApiKey)
                          ? 100
                          : (s_typingPrefix ? 1 : (s_typingMuteTagPlayer ? 16 : 48));
              }

              if (target && target->length() + filtered.length() < cap) {
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

    if (activeJavaSetting || s_typingSearch || s_typingApiKey || s_typingAutoGG ||
        s_typingUrchinKey || s_typingSeraphKey || s_typingAuroraApiKey ||
        s_typingPrefix || s_typingMuteTagPlayer) {
      
      std::string *target = nullptr;
      int cap = 100;

      if (activeJavaSetting) {
        target = &activeJavaSetting->inputBuf;
        cap = 100;
      } else {
        target =
        s_typingSearch
            ? &s_playerSearch
            : (s_typingApiKey
                   ? &s_apiKeyInput
                   : (s_typingAutoGG
                          ? &s_autoGGInput
                          : (s_typingUrchinKey
                                 ? &s_urchinKeyInput
                                 : (s_typingSeraphKey
                                        ? &s_seraphKeyInput
                                        : (s_typingAuroraApiKey
                                               ? &s_auroraApiKeyInput
                                               : (s_typingPrefix
                                                      ? &s_prefixInput
                                                      : &s_muteTagPlayerInput))))));
        cap = (s_typingAutoGG || s_typingUrchinKey || s_typingSeraphKey ||
               s_typingAuroraApiKey)
                  ? 100
                  : (s_typingPrefix ? 1 : (s_typingMuteTagPlayer ? 16 : 48));
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
          std::string key = s_apiKeyInput;
          if (key.empty() || key == "None")
            key = Config::getApiKey();
          bool keyless = Config::isKeylessModeEnabled();

          if ((key.empty() || key == "None") && !keyless) {
            NotificationManager::getInstance()->add(
                "Hypixel", "Set an API Key or enable Keyless Mode!",
                NotificationType::Error);
          } else {
            s_searching = true;
            s_hasLookup = false;
            std::string searchName = s_playerSearch;
            NotificationManager::getInstance()->add(
                "Hypixel", "Fetching player ID...", NotificationType::Info);

            std::thread([searchName, key, keyless]() {
              SafeGuard::installSehTranslator();
              SafeGuard::run("ClickGUI::statsLookup", [&]() {
                auto uuidOpt = Hypixel::getUuidByName(searchName);
                if (uuidOpt) {
                  NotificationManager::getInstance()->add(
                      "Hypixel", "ID found, fetching stats...",
                      NotificationType::Info);
                  std::optional<Hypixel::PlayerStats> statsOpt;
                  if (keyless) {
                    statsOpt = AbyssService::getPlayerStats(*uuidOpt);
                    if (!statsOpt) {
                      statsOpt = PrismService::getPlayerStats(*uuidOpt);
                    }
                  } else {
                    statsOpt = Hypixel::getPlayerStats(key, *uuidOpt);
                  }
                  if (statsOpt) {
                    s_lookupResult = *statsOpt;
                    s_lookupName = searchName;
                    s_lookupUrchinTags = std::nullopt;
                    s_lookupSeraphTags = std::nullopt;
                    s_tagsFetched = false;
                    s_hasLookup = true;

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
                    if (!uuid.empty() && uuid != s_lookupSkinUuid) {
                      s_skinLoading = true;
                      s_skinPendingReady = false;
                      std::thread([searchName, uuid]() {
                        SafeGuard::installSehTranslator();
                        SafeGuard::run("ClickGUI::skinHead", [&]() {
                          std::string url = "https://api.mcheads.org/head/" +
                                            searchName + "/64";
                          std::string body;
                          if (Http::get(url, body) && body.size() > 100 &&
                              body.size() < 256 * 1024) {
                            int w, h, ch;
                            unsigned char *px = stbi_load_from_memory(
                                (const stbi_uc *)body.data(), (int)body.size(),
                                &w, &h, &ch, STBI_rgb_alpha);
                            if (px && w > 0 && h > 0) {
                              s_skinPendingData.assign(px, px + w * h * 4);
                              s_skinPendingW = w;
                              s_skinPendingH = h;
                              s_lookupSkinUuid = uuid;
                              s_skinPendingReady = true;
                            }
                            if (px)
                              stbi_image_free(px);
                          }
                          s_skinLoading = false;
                        });
                      }).detach();
                    }
                  } else {
                    NotificationManager::getInstance()->add(
                        "Hypixel",
                        keyless ? "Abyss API failed"
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
          s_typingSearch = false;
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
