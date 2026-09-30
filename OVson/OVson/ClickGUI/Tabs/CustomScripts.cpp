#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../ClickGUI_Bridge.h"
#include "../../Render/RenderUtils.h"
#include <gl/GL.h>

namespace Render {
namespace Tabs {

static bool doesModuleMatchCategory(const ClickGUIBridge::CustomJavaModule &mod, const char* targetCategory) {
  if (!targetCategory) return true;
  const std::string &cat = mod.category;
  if (_stricmp(targetCategory, "Plugins") == 0) {
    return (_stricmp(cat.c_str(), "plugins") == 0 ||
            _stricmp(cat.c_str(), "scripts") == 0 ||
            cat.empty());
  }
  if (_stricmp(targetCategory, "Utils") == 0) {
    return (_stricmp(cat.c_str(), "utils") == 0 || _stricmp(cat.c_str(), "util") == 0);
  }
  return (_stricmp(cat.c_str(), targetCategory) == 0);
}

void renderCustomScripts(TabCtx &ctx, const char* targetCategory) {
  using namespace ClickGUIState;
  using namespace ClickGUIHelpers;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  auto& customModules = ClickGUIBridge::getCachedModules();
  if (customModules.empty()) return;

  const bool isSearching = !s_moduleSearch.empty();

  int matchingCount = 0;
  for (const auto& mod : customModules) {
    if (!isSearching && !doesModuleMatchCategory(mod, targetCategory)) {
      continue;
    }
    if (isSearching && !shouldShowInSearch(mod.name.c_str(), mod.description.c_str())) {
      continue;
    }
    matchingCount++;
  }
  if (matchingCount == 0) return;

  g_guiFont.drawString(cx, cy, "Scripts", applyAlpha(0xFFFFFFFF, alpha));
  cy += 28;

  for (size_t mi = 0; mi < customModules.size(); mi++) {
    const auto& mod = customModules[mi];
    if (!isSearching && !doesModuleMatchCategory(mod, targetCategory)) {
      continue;
    }
    if (isSearching && !shouldShowInSearch(mod.name.c_str(), mod.description.c_str())) {
      continue;
    }
    
    float settingsH = 0;
    for (size_t si = 0; si < mod.settings.size(); si++) {
      if (mod.settings[si].kind == "toggle") settingsH += 32;
      else if (mod.settings[si].kind == "description") settingsH += 22;
      else if (mod.settings[si].kind == "button") settingsH += 36;
      else settingsH += 32;
    }
    float cardH = 60 + settingsH;
    float cardX = mainX + 190;
    float cardW = g_w - 210;
    bool hCard = isHovered(mx, my, cardX, cy - 5, cardW, cardH);

    glDisable(GL_TEXTURE_2D);
    drawThemeCard(cardX, cy - 5, cardW, cardH, hCard, alpha);
    glEnable(GL_TEXTURE_2D);

    drawSectionLabel(cx, cy, mod.name.c_str(), alpha);

    if (!mod.description.empty()) {
      g_guiFont.drawString(cx, cy + 18, mod.description.c_str(),
                           applyAlpha(0xFFA0A0A5, alpha), 0.38f);
    }

    float swX = mainX + g_w - 65;
    glDisable(GL_TEXTURE_2D);
    drawSwitch(500 + (int)mi, swX, cy + 10, mod.enabled, hCard, alpha);
    glEnable(GL_TEXTURE_2D);
    if (clickEvent && hCard && isHovered(mx, my, swX - 5, cy + 5, 55, 30)) {
      bool newEn = !mod.enabled;
      const_cast<ClickGUIBridge::CustomJavaModule&>(mod).enabled = newEn;
      ClickGUIBridge::setModuleEnabled(mod.moduleObj, newEn);
    }

    float setY = cy + 40;
    for (size_t si = 0; si < mod.settings.size(); si++) {
      const auto& set = mod.settings[si];
      if (set.kind == "description") {
        g_guiFont.drawString(cx + 10, setY + 2, set.name.c_str(),
                             applyAlpha(0xFF909095, alpha), 0.36f);
        setY += 22;
      } else if (set.kind == "toggle") {
        bool togVal = ClickGUIBridge::getToggleValue(set.settingObj);
        bool hTog = hCard && my >= setY - 2 && my < setY + 28;
        g_guiFont.drawString(cx + 10, setY + 4, set.name.c_str(),
                             applyAlpha(0xFFFFFFFF, alpha), 0.42f);
        glDisable(GL_TEXTURE_2D);
        drawSwitch(600 + (int)(mi * 20 + si), swX, setY + 2, togVal, hTog, alpha);
        glEnable(GL_TEXTURE_2D);
        if (clickEvent && hTog) {
          ClickGUIBridge::setToggleValue(set.settingObj, !togVal);
        }
        setY += 32;
      } else if (set.kind == "button") {
        float btnW = 140.0f, btnH = 28.0f;
        float btnX = cx + 10;
        bool hBtn = hCard && isHovered(mx, my, btnX, setY, btnW, btnH);
        glDisable(GL_TEXTURE_2D);
        drawThemeButton(btnX, setY, btnW, btnH, hBtn, false, alpha);
        glEnable(GL_TEXTURE_2D);
        float tw = g_guiFont.getStringWidth(set.name.c_str()) * 0.42f;
        float textX = btnX + (btnW - tw) * 0.5f;
        g_guiFont.drawString(textX, setY + 6, set.name.c_str(),
                             applyAlpha(hBtn ? 0xFFFFFFFF : 0xFFC0C0C5, alpha), 0.42f);
        if (clickEvent && hBtn) {
          ClickGUIBridge::clickButton(set.settingObj);
        }
        setY += 36;
      } else if (set.kind == "slider") {
        float slVal = ClickGUIBridge::getSliderValue(set.settingObj);
        float workingVal = slVal;
        g_guiFont.drawString(cx + 10, setY + 4, set.name.c_str(),
                             applyAlpha(0xFFFFFFFF, alpha), 0.40f);
        float valRight = swX + 50.0f;
        float numBoxW = 46.0f;
        float slW = 120.0f;
        float slX = valRight - numBoxW - 10.0f - slW;
        int sliderId = 15000 + (int)(mi * 50 + si);
        bool changed = drawSlider(sliderId, slX, setY + 11.0f, slW, 8.0f,
                                  workingVal, set.minVal, set.maxVal, mx, my, ctx.lClick, alpha);
        changed = drawNumericInput(sliderId, valRight - numBoxW, setY + 3.0f, numBoxW,
                                   24.0f, workingVal, set.minVal, set.maxVal, 1, "",
                                   mx, my, clickEvent, alpha) || changed;
        if (changed) {
          ClickGUIBridge::setSliderValue(set.settingObj, workingVal);
        }
        setY += 32;
      } else if (set.kind == "choice") {
        g_guiFont.drawString(cx + 10, setY + 4, set.name.c_str(),
                             applyAlpha(0xFFFFFFFF, alpha), 0.40f);
        std::string curChoice = ClickGUIBridge::getChoiceValue(set.settingObj);
        float rightX = swX + 50.0f;
        float curBtnX = rightX;
        float chipH = 24.0f;
        for (int k = (int)set.choices.size() - 1; k >= 0; --k) {
          const std::string& ch = set.choices[k];
          float tw = g_guiFont.getStringWidth(ch.c_str()) * (0.38f / 0.5f);
          float bw = tw + 18.0f;
          if (bw < 45.0f) bw = 45.0f;
          curBtnX -= (bw + 6.0f);
          float btnX = curBtnX + 6.0f;
          float btnY = setY + 3.0f;
          bool hov = hCard && isHovered(mx, my, btnX, btnY, bw, chipH);
          bool sel = (curChoice == ch);
          glDisable(GL_TEXTURE_2D);
          drawThemeButton(btnX, btnY, bw, chipH, hov, sel, alpha);
          glEnable(GL_TEXTURE_2D);
          g_guiFont.drawString(btnX + (bw - tw) * 0.5f, btnY + 4.5f, ch.c_str(),
                               applyAlpha(sel ? 0xFFFFFFFF : 0xFF808085, alpha), 0.38f);
          if (clickEvent && hov) {
            ClickGUIBridge::setChoiceValue(set.settingObj, ch);
          }
        }
        setY += 32;
      } else if (set.kind == "input") {
        g_guiFont.drawString(cx + 10, setY + 4, set.name.c_str(),
                             applyAlpha(0xFFFFFFFF, alpha), 0.40f);
        std::string curVal = ClickGUIBridge::getInputValue(set.settingObj);
        float rightX = swX + 50.0f;
        float inpW = 140.0f, inpH = 24.0f;
        float inpX = rightX - inpW;
        bool hInp = hCard && isHovered(mx, my, inpX, setY + 3.0f, inpW, inpH);
        glDisable(GL_TEXTURE_2D);
        drawTextInput(inpX, setY + 3.0f, inpW, inpH, false, hInp, alpha);
        glEnable(GL_TEXTURE_2D);
        g_guiFont.drawString(inpX + 6.0f, setY + 6.0f, curVal.c_str(),
                             applyAlpha(0xFFFFFFFF, alpha), 0.38f);
        setY += 32;
      } else {
        g_guiFont.drawString(cx + 10, setY + 4, set.name.c_str(),
                             applyAlpha(0xFFCCCCCC, alpha), 0.40f);
        setY += 32;
      }
    }
    cy += (int)cardH + 15;
  }
}

} // namespace Tabs
} // namespace Render
