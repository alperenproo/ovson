#include "ClickGUI.h"
#include "State.h"
#include "Helpers.h"
#include "../Config/Config.h"
#include "../Render/BetterTab.h"
#include "../Render/StatsOverlay.h"
#include "../Render/NotificationManager.h"
#include "../Utils/SensitivityFix.h"
#include "../Utils/Timer.h"
#include <Windows.h>
#include <string>

namespace Render {

using namespace ClickGUIState;

void ClickGUI::init() {
  s_init = true;
  TimeUtil::init();
}

void ClickGUI::shutdown() {}

bool ClickGUI::isOpen() { return s_open; }

std::string ClickGUI::getKeyName(int vk) {
  if (vk == VK_INSERT) return "INSERT";
  if (vk == VK_DELETE) return "DELETE";
  if (vk == VK_HOME)   return "HOME";
  if (vk == VK_END)    return "END";
  if (vk == VK_PRIOR)  return "PAGE UP";
  if (vk == VK_NEXT)   return "PAGE DOWN";
  if (vk == VK_RSHIFT) return "RSHIFT";
  if (vk == VK_LSHIFT) return "LSHIFT";

  UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
  char buf[32] = {0};
  if (GetKeyNameTextA(scanCode << 16, buf, 32))
    return std::string(buf);
  return "Key " + std::to_string(vk);
}

void ClickGUI::setOpen(bool open) {
  if (s_open == open) return;
  s_open = open;
  s_targetAlpha = s_open ? 1.0f : 0.0f;

  if (s_open) {
    s_openingScale = 0.95f;
    s_apiKeyInput = Config::getApiKey();
    s_autoGGInput = Config::getAutoGGMessage();
    s_urchinKeyInput = Config::getUrchinApiKey();
    s_seraphKeyInput = Config::getSeraphApiKey();
    s_auroraApiKeyInput = Config::getAuroraApiKey();
    ShowCursor(TRUE);
    setMouseGrabbed(false);
    FocusFix::setIngameFocus(false);
  } else {
    FocusFix::setIngameFocus(true);
    if (isIngame() && !BetterTab::isResizeMode()) {
      ShowCursor(FALSE);
      setMouseGrabbed(true);
    }
  }
}

void ClickGUI::toggle() {
  setOpen(!s_open);
}

void ClickGUI::updateInput(HWND hwnd) {
  if (hwnd && GetForegroundWindow() != hwnd) {
    s_lastInsert = false;
    return;
  }
  int key = Config::getClickGuiKey();
  bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
  if (down && !s_lastInsert) {
    if (Config::isClickGuiOn()) {
      toggle();
    } else {
      if (Config::getOverlayMode() == "gui") {
        bool current = StatsOverlay::isEnabled();
        StatsOverlay::setEnabled(!current);
        NotificationManager::getInstance()->add(
            "Overlay",
            !current ? "Stats Overlay Enabled" : "Stats Overlay Disabled",
            NotificationType::Info);
      } else {
        NotificationManager::getInstance()->add(
            "Overlay", "Unlock 'GUI' mode to toggle overlay",
            NotificationType::Warning);
      }
    }
  }
  s_lastInsert = down;
}

} // namespace Render
