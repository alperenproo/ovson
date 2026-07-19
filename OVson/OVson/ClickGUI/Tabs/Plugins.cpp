#include "Tabs.h"
#include "../State.h"
#include "../Theme.h"
#include "../Helpers.h"
#include "../../Render/RenderUtils.h"
#include "../../Render/NotificationManager.h"
#include "../../Plugins/PluginLoader.h"
#include "../../Utils/Logger.h"
#include "../../Java.h"
#include <gl/GL.h>
#include "../../JavaHook/JavaHook.h"
#include "../ClickGUI_Bridge.h"
#include "../ClickGUI.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace Render {
namespace Tabs {

void renderPlugins(TabCtx &ctx) {
  using namespace ClickGUIState;
  const float mainX = ctx.mainX;
  const float cx    = ctx.cx;
  float      &cy    = ctx.cy;
  const float mx    = ctx.mx;
  const float my    = ctx.my;
  const bool  clickEvent = ctx.clickEvent;
  const float alpha = ctx.alpha;

  g_guiFont.drawString(cx, cy, "Plugins", applyAlpha(0xFFFFFFFF, alpha));
  cy += 28;

  {
    float btnW = 160.0f, btnH = 36.0f;
    float btnX = mainX + 200, btnY = cy;
    bool hBtn = isHovered(mx, my, btnX, btnY, btnW, btnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(btnX, btnY, btnW, btnH, hBtn, false, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(btnX + 12, btnY + 8, "+ Add Plugin",
                         applyAlpha(hBtn ? 0xFFFFFFFF : 0xFFC0C0C5, alpha),
                         0.44f);
    if (clickEvent && hBtn) {
      wchar_t* localAppData;
      if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &localAppData) == S_OK) {
        std::wstring dir = std::wstring(localAppData) + L"\\OVson\\plugins";
        CoTaskMemFree(localAppData);
        if (!std::filesystem::exists(dir))
          std::filesystem::create_directories(dir);
        ShellExecuteW(NULL, L"open", L"explorer.exe", dir.c_str(), NULL, SW_SHOWDEFAULT);
      }
    }
  }

  {
    float btnW = 100.0f, btnH = 36.0f;
    float btnX = mainX + 370, btnY = cy;
    bool hBtn = isHovered(mx, my, btnX, btnY, btnW, btnH);
    glDisable(GL_TEXTURE_2D);
    drawThemeButton(btnX, btnY, btnW, btnH, hBtn, false, alpha);
    glEnable(GL_TEXTURE_2D);
    g_guiFont.drawString(btnX + 12, btnY + 8, "Reload",
                         applyAlpha(hBtn ? 0xFFFFFFFF : 0xFFC0C0C5, alpha),
                         0.44f);
    if (clickEvent && hBtn) {
      JavaHook::shutdown();
      PluginLoader::shutdown();
      ClickGUIBridge::clearCache();
      Render::ClickGUI::resetLayoutB();
      PluginLoader::initialize();
      JavaHook::initialize();
      NotificationManager::getInstance()->add(
          "Plugins", "Plugins reloaded.", NotificationType::Success);
          
      JNIEnv* e = lc ? lc->getEnv() : nullptr;
      if (e) {
          jclass pmClass = PluginLoader::getPluginManagerClass(e);
          Logger::info("[Plugins Tab Reload] getPluginManagerClass returned: %p", pmClass);
          if (pmClass) {
              jmethodID getP = e->GetStaticMethodID(pmClass, "getPlugins", "()Ljava/util/List;");
              Logger::info("[Plugins Tab Reload] getPlugins method: %p", getP);
              if (getP) {
                  jobject listObj = e->CallStaticObjectMethod(pmClass, getP);
                  Logger::info("[Plugins Tab Reload] list object: %p", listObj);
                  if (listObj) {
                      jclass lCls = e->GetObjectClass(listObj);
                      jmethodID sM = e->GetMethodID(lCls, "size", "()I");
                      int cnt = e->CallIntMethod(listObj, sM);
                      Logger::info("[Plugins Tab Reload] Plugin Count: %d", cnt);
                      e->DeleteLocalRef(lCls);
                  }
                  if (listObj) e->DeleteLocalRef(listObj);
              }
              e->DeleteLocalRef(pmClass);
          }
          if (e->ExceptionCheck()) e->ExceptionClear();
      }
    }
  }

  cy += 50;

  JNIEnv* env = lc ? lc->getEnv() : nullptr;
  int pluginCount = 0;

  struct PluginInfo {
    std::string name;
    std::string version;
    std::string author;
    bool enabled;
  };
  std::vector<PluginInfo> plugins;
  static bool s_debugLogged = false;
  if (env) {
    if (env->ExceptionCheck()) env->ExceptionClear();

    jclass pmClass = PluginLoader::getPluginManagerClass(env);
    if (!s_debugLogged) {
      Logger::info("[Plugins Tab] getPluginManagerClass returned: %s", pmClass ? "OK" : "NULL");
    }
    if (pmClass) {
      jmethodID getPlugins = env->GetStaticMethodID(pmClass, "getPlugins", "()Ljava/util/List;");
      if (!s_debugLogged) {
        Logger::info("[Plugins Tab] getPlugins method: %s", getPlugins ? "OK" : "NULL");
      }
      if (getPlugins) {
        jobject list = env->CallStaticObjectMethod(pmClass, getPlugins);
        if (env->ExceptionCheck()) {
          if (!s_debugLogged) Logger::error("[Plugins Tab] Exception calling getPlugins");
          env->ExceptionDescribe();
          env->ExceptionClear();
        }
        if (!s_debugLogged) {
          Logger::info("[Plugins Tab] list object: %s", list ? "OK" : "NULL");
        }
        if (list) {
          jclass listClass = env->GetObjectClass(list);
          jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
          jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
          pluginCount = env->CallIntMethod(list, sizeMethod);
          if (!s_debugLogged) {
            Logger::info("[Plugins Tab] pluginCount = %d", pluginCount);
          }
          
          for (int i = 0; i < pluginCount; i++) {
            jobject plugin = env->CallObjectMethod(list, getMethod, i);
            if (plugin) {
              jclass pluginCls = env->GetObjectClass(plugin);
              
              jmethodID getNameM = env->GetMethodID(pluginCls, "getName", "()Ljava/lang/String;");
              jmethodID getVersionM = env->GetMethodID(pluginCls, "getVersion", "()Ljava/lang/String;");
              jmethodID getAuthorM = env->GetMethodID(pluginCls, "getAuthor", "()Ljava/lang/String;");
              jmethodID isEnabledM = env->GetMethodID(pluginCls, "isEnabled", "()Z");
              
              PluginInfo info;
              
              if (getNameM) {
                jstring jName = (jstring)env->CallObjectMethod(plugin, getNameM);
                if (jName) {
                  const char* c = env->GetStringUTFChars(jName, nullptr);
                  info.name = c ? c : "Unknown";
                  if (c) env->ReleaseStringUTFChars(jName, c);
                  env->DeleteLocalRef(jName);
                }
              } else { info.name = "Unknown"; }

              if (getVersionM) {
                jstring jVer = (jstring)env->CallObjectMethod(plugin, getVersionM);
                if (jVer) {
                  const char* c = env->GetStringUTFChars(jVer, nullptr);
                  info.version = c ? c : "?";
                  if (c) env->ReleaseStringUTFChars(jVer, c);
                  env->DeleteLocalRef(jVer);
                }
              } else { info.version = "?"; }

              if (getAuthorM) {
                jstring jAuth = (jstring)env->CallObjectMethod(plugin, getAuthorM);
                if (jAuth) {
                  const char* c = env->GetStringUTFChars(jAuth, nullptr);
                  info.author = c ? c : "Unknown";
                  if (c) env->ReleaseStringUTFChars(jAuth, c);
                  env->DeleteLocalRef(jAuth);
                }
              } else { info.author = "Unknown"; }

              if (isEnabledM) {
                info.enabled = env->CallBooleanMethod(plugin, isEnabledM);
              } else { info.enabled = false; }
              
              if (env->ExceptionCheck()) env->ExceptionClear();

              plugins.push_back(info);
              env->DeleteLocalRef(pluginCls);
              env->DeleteLocalRef(plugin);
            }
          }
          env->DeleteLocalRef(listClass);
          env->DeleteLocalRef(list);
        }
      }
      env->DeleteLocalRef(pmClass);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    s_debugLogged = true;
  }

  if (plugins.empty()) {
    g_guiFont.drawString(cx, cy + 10, "No plugins loaded.",
                         applyAlpha(0xFFA0A0A5, alpha), 0.45f);
    g_guiFont.drawString(cx, cy + 30,
                         "Drop .jar files into the plugins folder and click Reload.",
                         applyAlpha(0xFF707075, alpha), 0.38f);
    cy += 60;
  } else {
    for (size_t i = 0; i < plugins.size(); i++) {
      const auto& p = plugins[i];
      float cardH = 80.0f;
      float cardX = mainX + 190;
      float cardW = g_w - 210;
      bool hCard = isHovered(mx, my, cardX, cy - 5, cardW, cardH);

      glDisable(GL_TEXTURE_2D);
      drawThemeCard(cardX, cy - 5, cardW, cardH, hCard, alpha);
      glEnable(GL_TEXTURE_2D);

      glDisable(GL_TEXTURE_2D);
      DWORD iconBg = p.enabled ? 0xFF4A9EFF : 0xFF555560;
      RenderUtils::drawRoundedRect(cx, cy + 8, 42.0f, 42.0f, 10.0f, iconBg,
                                   alpha * 0.85f);
      glEnable(GL_TEXTURE_2D);
      
      std::string initial(1, p.name.empty() ? '?' : (char)toupper(p.name[0]));
      float iw = g_guiFont.getStringWidth(initial.c_str());
      g_guiFont.drawString(cx + 21 - iw * 0.5f, cy + 18, initial.c_str(),
                           applyAlpha(0xFFFFFFFF, alpha), 0.55f);

      float textX = cx + 55;
      g_guiFont.drawString(textX, cy + 5, p.name.c_str(),
                           applyAlpha(0xFFFFFFFF, alpha), 0.50f);
      
      std::string meta = "v" + p.version + "  by " + p.author;
      g_guiFont.drawString(textX, cy + 24, meta.c_str(),
                           applyAlpha(0xFF8888AA, alpha), 0.38f);
      
      std::string status = p.enabled ? "Enabled" : "Disabled";
      DWORD statusCol = p.enabled ? 0xFF55FF77 : 0xFFFF5555;
      g_guiFont.drawString(textX, cy + 42, status.c_str(),
                           applyAlpha(statusCol, alpha), 0.36f);

      float swX = mainX + g_w - 65;
      glDisable(GL_TEXTURE_2D);
      drawSwitch(200 + (int)i, swX, cy + 18, p.enabled, hCard, alpha);
      glEnable(GL_TEXTURE_2D);

      if (clickEvent && hCard) {
        if (env) {
          jclass pmCls = PluginLoader::getPluginManagerClass(env);
          if (pmCls) {
            jmethodID getPluginsM = env->GetStaticMethodID(pmCls, "getPlugins", "()Ljava/util/List;");
            jobject plist = env->CallStaticObjectMethod(pmCls, getPluginsM);
            if (plist) {
              jclass listCls = env->GetObjectClass(plist);
              jmethodID getM = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
              jobject pluginObj = env->CallObjectMethod(plist, getM, (jint)i);
              if (pluginObj) {
                jclass pCls = env->GetObjectClass(pluginObj);
                jmethodID setEn = env->GetMethodID(pCls, "setEnabled", "(Z)V");
                if (setEn) {
                  env->CallVoidMethod(pluginObj, setEn, p.enabled ? JNI_FALSE : JNI_TRUE);
                }
                env->DeleteLocalRef(pCls);
                env->DeleteLocalRef(pluginObj);
              }
              env->DeleteLocalRef(listCls);
              env->DeleteLocalRef(plist);
            }
            env->DeleteLocalRef(pmCls);
          }
          if (env->ExceptionCheck()) env->ExceptionClear();
        }
        NotificationManager::getInstance()->add(
            "Plugin",
            p.enabled ? (p.name + " disabled") : (p.name + " enabled"),
            p.enabled ? NotificationType::Warning : NotificationType::Success);
      }

      cy += cardH + 10;
    }
  }

  cy += 10;
  g_guiFont.drawString(cx, cy, "Plugin Folder:",
                       applyAlpha(0xFF8888AA, alpha), 0.36f);
  g_guiFont.drawString(cx, cy + 16, "%LOCALAPPDATA%\\OVson\\plugins",
                       applyAlpha(0xFF707075, alpha), 0.34f);
  cy += 50;
}

} // namespace Tabs
} // namespace Render
