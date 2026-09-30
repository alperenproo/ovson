#pragma once
#include <Windows.h>
#include <gl/GL.h>
#include <cstdint>
#include <string>
#include <cstring>
#include <cctype>
#include <jni.h>
#include "../Java.h"
#include "../Config/Config.h"
#include "../Utils/stb_image.h"

namespace Render {

static const uint32_t kMcColors[16] = {
    0x000000, // 0 - Black
    0x0000AA, // 1 - Dark Blue
    0x00AA00, // 2 - Dark Green
    0x00AAAA, // 3 - Dark Aqua
    0xAA0000, // 4 - Dark Red
    0xAA00AA, // 5 - Dark Purple
    0xFFAA00, // 6 - Gold
    0xAAAAAA, // 7 - Gray
    0x555555, // 8 - Dark Gray
    0x5555FF, // 9 - Blue
    0x55FF55, // a - Green
    0x55FFFF, // b - Aqua
    0xFF5555, // c - Red
    0xFF55FF, // d - Light Purple
    0xFFFF55, // e - Yellow
    0xFFFFFF  // f - White
};

static const int kMcCharWidths[256] = {
    6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 6, 6, 4,
    4, 6, 7, 6, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0,
    4, 2, 5, 6, 6, 6, 6, 3, 5, 5, 5, 6, 2, 6, 2, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 2, 2, 5, 6, 5, 6,
    7, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 4, 6, 6,
    3, 6, 6, 6, 6, 6, 5, 6, 6, 2, 6, 5, 3, 6, 6, 6,
    6, 6, 6, 6, 4, 6, 6, 6, 6, 6, 6, 5, 2, 5, 7, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 3, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6,
    6, 3, 6, 6, 6, 6, 6, 6, 6, 7, 6, 6, 6, 2, 6, 6,
    8, 9, 9, 6, 6, 6, 8, 8, 6, 8, 8, 8, 8, 8, 6, 6,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 6, 9, 9, 9, 5, 9, 9,
    8, 7, 7, 8, 7, 8, 8, 8, 7, 8, 8, 7, 9, 9, 6, 7,
    7, 7, 7, 7, 9, 6, 7, 8, 7, 6, 6, 9, 7, 6, 7, 0,
};

struct McFont {
  GLuint fontTextureId = 0;
  GLuint unicodeTex25 = 0;
  GLuint unicodeTex26 = 0;
  GLuint unicodeTex27 = 0;
  int charWidth[256] = {};
  bool ready = false;

  GLuint loadUnicodePage(int page) {
    char name[64];
    snprintf(name, sizeof(name), "unicode_page_%02x.png", page);

    std::vector<std::string> pathsToTry;
    HMODULE hMod = Config::getModuleHandle();
    if (hMod) {
      char dllPath[MAX_PATH] = {0};
      if (GetModuleFileNameA(hMod, dllPath, MAX_PATH) > 0) {
        char *lastSlash = strrchr(dllPath, '\\');
        if (lastSlash) {
          *lastSlash = '\0';
          std::string dllDir = dllPath;
          pathsToTry.push_back(dllDir + "\\assets\\items\\" + name);
          pathsToTry.push_back(dllDir + "\\items\\" + name);
          pathsToTry.push_back(dllDir + "\\" + name);
          pathsToTry.push_back(dllDir + "\\..\\assets\\items\\" + name);
          pathsToTry.push_back(dllDir + "\\..\\items\\" + name);
        }
      }
    }

    char buf[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) > 0) {
      pathsToTry.push_back(std::string(buf) + "\\OVson\\assets\\items\\" + name);
      pathsToTry.push_back(std::string(buf) + "\\OVson\\items\\" + name);
      pathsToTry.push_back(std::string(buf) + "\\.minecraft\\OVson\\assets\\items\\" + name);
    }
    if (GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH) > 0) {
      pathsToTry.push_back(std::string(buf) + "\\OVson\\assets\\items\\" + name);
      pathsToTry.push_back(std::string(buf) + "\\OVson\\items\\" + name);
    }

    pathsToTry.push_back(std::string("assets\\items\\") + name);
    pathsToTry.push_back(std::string("OVson\\assets\\items\\") + name);
    pathsToTry.push_back(std::string("OVson\\items\\") + name);
    pathsToTry.push_back(std::string("items\\") + name);

    int w = 0, h = 0, ch = 0;
    unsigned char *px = nullptr;
    for (const auto &p : pathsToTry) {
      px = stbi_load(p.c_str(), &w, &h, &ch, 4);
      if (px) break;
    }

    if (!px && hMod) {
      char resName[64];
      snprintf(resName, sizeof(resName), "ITEM_unicode_page_%02x", page);
      HRSRC hRes = FindResourceA(hMod, resName, RT_RCDATA);
      if (!hRes) {
        snprintf(resName, sizeof(resName), "unicode_page_%02x", page);
        hRes = FindResourceA(hMod, resName, RT_RCDATA);
      }
      if (hRes) {
        HGLOBAL hData = LoadResource(hMod, hRes);
        if (hData) {
          void *pData = LockResource(hData);
          DWORD sz = SizeofResource(hMod, hRes);
          if (pData && sz > 0) {
            px = stbi_load_from_memory((const stbi_uc *)pData, (int)sz, &w, &h, &ch, 4);
          }
        }
      }
    }

    if (!px) return 0;

    GLuint tid = 0;
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(px);
    return tid;
  }

  void ensureUnicodePages() {
    if (!unicodeTex25) unicodeTex25 = loadUnicodePage(0x25);
    if (!unicodeTex26) unicodeTex26 = loadUnicodePage(0x26);
    if (!unicodeTex27) unicodeTex27 = loadUnicodePage(0x27);
  }

  void initDefaults() {
    std::memcpy(charWidth, kMcCharWidths, sizeof(charWidth));
  }

  void init(JNIEnv *env) {
    if (ready || !env || !lc) return;
    initDefaults();

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) mcCls = env->FindClass("ave");
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (!mcCls) return;

    jobject mc = nullptr;
    jmethodID m_getMc = lc->GetStaticMethodID(mcCls, "getMinecraft", "()Lnet/minecraft/client/Minecraft;", "func_71410_x", "A", "()Lave;");
    if (m_getMc) {
      mc = env->CallStaticObjectMethod(mcCls, m_getMc);
      if (env->ExceptionCheck()) { env->ExceptionClear(); mc = nullptr; }
    }
    if (!mc) {
      jfieldID f_theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                              "Lnet/minecraft/client/Minecraft;",
                                              "field_71432_P", "S", "Lave;");
      if (f_theMc) {
        mc = env->GetStaticObjectField(mcCls, f_theMc);
        if (env->ExceptionCheck()) { env->ExceptionClear(); mc = nullptr; }
      }
    }
    if (!mc) return;

    jfieldID f_fontRenderer = nullptr;
    const char *frNames[] = {"fontRendererObj", "fontRenderer", "field_71466_p", "l"};
    const char *frSigs[] = {"Lnet/minecraft/client/gui/FontRenderer;", "Lavn;"};
    for (const char *fn : frNames) {
      for (const char *fs : frSigs) {
        jfieldID fid = env->GetFieldID(mcCls, fn, fs);
        if (fid) {
          jobject testFr = env->GetObjectField(mc, fid);
          if (testFr) {
            if (!Lunar::isSGAFontRenderer(env, testFr)) {
              f_fontRenderer = fid;
            }
            env->DeleteLocalRef(testFr);
          } else {
            f_fontRenderer = fid; // can't check, assume it's ok LMAO
          }
          if (f_fontRenderer) break;
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
      if (f_fontRenderer) break;
    }

    if (!f_fontRenderer && lc && lc->jvmti) {
      jint fCount = 0;
      jfieldID *fList = nullptr;
      if (lc->jvmti->GetClassFields(mcCls, &fCount, &fList) == JVMTI_ERROR_NONE) {
        for (int i = 0; i < fCount; i++) {
          char *fn = nullptr, *fs = nullptr;
          if (lc->jvmti->GetFieldName(mcCls, fList[i], &fn, &fs, nullptr) == JVMTI_ERROR_NONE) {
            if (fs && (std::strcmp(fs, "Lavn;") == 0 || std::strstr(fs, "FontRenderer;") != nullptr)) {
              std::string nameStr = fn ? fn : "";
              std::string lowerStr = nameStr;
              for (char &c : lowerStr) c = (char)::tolower((unsigned char)c);
              bool isSga = (lowerStr.find("galactic") != std::string::npos ||
                            lowerStr.find("sga") != std::string::npos ||
                            lowerStr.find("enchant") != std::string::npos ||
                            nameStr == "q" || nameStr == "field_71464_q");
              if (!isSga) {
                jobject testFr = env->GetObjectField(mc, fList[i]);
                if (testFr) {
                  bool objIsSga = Lunar::isSGAFontRenderer(env, testFr);
                  env->DeleteLocalRef(testFr);
                  if (objIsSga) isSga = true;
                }
              }
              if (!isSga) {
                f_fontRenderer = fList[i];
                lc->jvmti->Deallocate((unsigned char*)fn);
                lc->jvmti->Deallocate((unsigned char*)fs);
                break;
              }
            }
            if (fn) lc->jvmti->Deallocate((unsigned char*)fn);
            if (fs) lc->jvmti->Deallocate((unsigned char*)fs);
          }
        }
        if (fList) lc->jvmti->Deallocate((unsigned char*)fList);
      }
    }

    jfieldID f_renderEngine = lc->GetFieldID(mcCls, "renderEngine",
                                            "Lnet/minecraft/client/renderer/texture/TextureManager;",
                                            "field_71446_z", "P", "Lbmj;");
    if (!f_renderEngine) {
      f_renderEngine = lc->FindFieldBySignature(mcCls, "Lbmj;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!f_renderEngine) {
      f_renderEngine = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/renderer/texture/TextureManager;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }

    jobject fr = f_fontRenderer ? env->GetObjectField(mc, f_fontRenderer) : nullptr;
    jobject tm = f_renderEngine ? env->GetObjectField(mc, f_renderEngine) : nullptr;

    if (fr && tm) {
      jclass frCls = env->GetObjectClass(fr);

      jfieldID f_cw = lc->GetFieldID(frCls, "charWidth", "[I", "field_78286_d", "d", "[I");
      if (!f_cw) f_cw = lc->FindFieldBySignature(frCls, "[I");
      if (env->ExceptionCheck()) env->ExceptionClear();

      if (f_cw) {
        jintArray cwArr = (jintArray)env->GetObjectField(fr, f_cw);
        if (cwArr) {
          jint *elems = env->GetIntArrayElements(cwArr, nullptr);
          if (elems) {
            for (int i = 0; i < 256; ++i) {
              if (elems[i] > 0 || i == 32) {
                charWidth[i] = elems[i];
              }
            }
            env->ReleaseIntArrayElements(cwArr, elems, JNI_ABORT);
          }
          env->DeleteLocalRef(cwArr);
        }
      }

      jfieldID f_loc = lc->GetFieldID(frCls, "locationFontTexture",
                                      "Lnet/minecraft/util/ResourceLocation;",
                                      "field_111273_g", "g", "Ljy;");
      if (!f_loc) f_loc = lc->FindFieldBySignature(frCls, "Ljy;");
      if (!f_loc) f_loc = lc->FindFieldBySignature(frCls, "Lnet/minecraft/util/ResourceLocation;");
      if (env->ExceptionCheck()) env->ExceptionClear();

      if (f_loc) {
        jobject loc = env->GetObjectField(fr, f_loc);
        if (loc) {
          jclass tmCls = env->GetObjectClass(tm);

          jmethodID m_getTex = lc->GetMethodID(tmCls, "getTexture",
                                              "(Lnet/minecraft/util/ResourceLocation;)Lnet/minecraft/client/renderer/texture/ITextureObject;",
                                              "func_110581_b", "b", "(Ljy;)Lblw;");
          if (!m_getTex) {
            m_getTex = lc->FindMethodBySignature(tmCls, "(Ljy;)Lblw;");
            if (env->ExceptionCheck()) env->ExceptionClear();
          }
          if (m_getTex) {
            jobject texObj = env->CallObjectMethod(tm, m_getTex, loc);
            if (env->ExceptionCheck()) {
              env->ExceptionClear();
              texObj = nullptr;
            }
            if (texObj) {
              jclass toCls = env->GetObjectClass(texObj);
              jmethodID m_getGlId = lc->GetMethodID(toCls, "getGlTextureId", "()I", "func_110552_b", "b", "()I");
              if (!m_getGlId) {
                m_getGlId = lc->FindMethodBySignature(toCls, "()I");
                if (env->ExceptionCheck()) env->ExceptionClear();
              }
              if (m_getGlId) {
                jint glId = env->CallIntMethod(texObj, m_getGlId);
                if (env->ExceptionCheck()) {
                  env->ExceptionClear();
                } else if (glId > 0) {
                  fontTextureId = static_cast<GLuint>(glId);
                  ready = true;
                }
              }
              env->DeleteLocalRef(toCls);
              env->DeleteLocalRef(texObj);
            }
          }

          if (!ready) {
            jmethodID m_bind = lc->GetMethodID(tmCls, "bindTexture",
                                               "(Lnet/minecraft/util/ResourceLocation;)V",
                                               "func_110577_a", "a", "(Ljy;)V");
            if (!m_bind) {
              m_bind = lc->FindMethodBySignature(tmCls, "(Ljy;)V");
              if (env->ExceptionCheck()) env->ExceptionClear();
            }

            if (m_bind) {
              GLint prevTex = 0;
              glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
              glBindTexture(GL_TEXTURE_2D, 0); // Force GlStateManager to dirty its texture binding
              env->CallVoidMethod(tm, m_bind, loc);
              if (env->ExceptionCheck()) {
                env->ExceptionClear();
              } else {
                GLint bound = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
                if (bound > 0) {
                  fontTextureId = static_cast<GLuint>(bound);
                  ready = true;
                }
              }
              if (prevTex > 0) {
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));
              }
            }
          }

          env->DeleteLocalRef(tmCls);
          env->DeleteLocalRef(loc);
        }
      }
      env->DeleteLocalRef(frCls);
    }

    if (tm) env->DeleteLocalRef(tm);
    if (fr) env->DeleteLocalRef(fr);
    env->DeleteLocalRef(mc);
  }

  void shutdown() {
    if (unicodeTex25) { glDeleteTextures(1, &unicodeTex25); unicodeTex25 = 0; }
    if (unicodeTex26) { glDeleteTextures(1, &unicodeTex26); unicodeTex26 = 0; }
    if (unicodeTex27) { glDeleteTextures(1, &unicodeTex27); unicodeTex27 = 0; }
    fontTextureId = 0;
    ready = false;
  }

  float getStringWidth(const std::string &text) const {
    float width = 0.0F;
    bool bold = false;
    for (size_t i = 0; i < text.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(text[i]);
      if (c == 0xC2 && i + 1 < text.size() &&
          static_cast<unsigned char>(text[i + 1]) == 0xA7) {
        i += 2;
        if (i < text.size()) {
          char code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
          if (code == 'l') bold = true;
          else if (code == 'r' || (code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) bold = false;
        }
        continue;
      }
      if (c == 0xA7 || c == 167) {
        i += 1;
        if (i < text.size()) {
          char code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
          if (code == 'l') bold = true;
          else if (code == 'r' || (code >= '0' && code <= '9') || (code >= 'a' && code <= 'f')) bold = false;
        }
        continue;
      }
      if (c >= 0xE0 && c <= 0xEF && i + 2 < text.size()) {
        i += 2;
        width += (bold ? 9.0F : 8.0F);
        continue;
      }
      int cw = charWidth[c];
      float w = cw > 0 ? static_cast<float>(cw) : (c == ' ' ? 4.0F : 0.0F);
      if (bold && c != ' ') w += 1.0F;
      width += w;
    }
    return width;
  }

  void renderPass(const std::string &text, float startX, float startY,
                  uint32_t baseColor, bool shadow) const {
    if (text.empty() || fontTextureId == 0 || !glIsTexture(fontTextureId)) return;

    const_cast<McFont*>(this)->ensureUnicodePages();

    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

    glBindTexture(GL_TEXTURE_2D, fontTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);

    GLint texW = 128, texH = 128;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
    if (texW <= 0) texW = 128;
    if (texH <= 0) texH = 128;
    float texScaleX = (float)texW / 128.0f;
    float texScaleY = (float)texH / 128.0f;

    float curX = startX;
    float curY = startY;
    uint32_t curCol = baseColor;
    bool bold = false;

    auto setColor = [shadow](uint32_t c) {
      float a = ((c >> 24) & 0xFF) / 255.0F;
      if (a <= 0.0F) a = 1.0F;
      float r = ((c >> 16) & 0xFF) / 255.0F;
      float g = ((c >> 8) & 0xFF) / 255.0F;
      float b = (c & 0xFF) / 255.0F;
      if (shadow) {
        r *= 0.25F;
        g *= 0.25F;
        b *= 0.25F;
      }
      glColor4f(r, g, b, a);
    };

    setColor(curCol);

    glBegin(GL_QUADS);
    for (size_t i = 0; i < text.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(text[i]);

      if (c == 0xC2 && i + 1 < text.size() &&
          static_cast<unsigned char>(text[i + 1]) == 0xA7) {
        i += 2;
        if (i < text.size()) {
          char code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
          if (code >= '0' && code <= '9') {
            curCol = (baseColor & 0xFF000000) | kMcColors[code - '0'];
            setColor(curCol);
            bold = false;
          } else if (code >= 'a' && code <= 'f') {
            curCol = (baseColor & 0xFF000000) | kMcColors[code - 'a' + 10];
            setColor(curCol);
            bold = false;
          } else if (code == 'r') {
            curCol = baseColor;
            setColor(curCol);
            bold = false;
          } else if (code == 'l') {
            bold = true;
          }
        }
        continue;
      }

      if (c == 0xA7 || c == 167) {
        i += 1;
        if (i < text.size()) {
          char code = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
          if (code >= '0' && code <= '9') {
            curCol = (baseColor & 0xFF000000) | kMcColors[code - '0'];
            setColor(curCol);
            bold = false;
          } else if (code >= 'a' && code <= 'f') {
            curCol = (baseColor & 0xFF000000) | kMcColors[code - 'a' + 10];
            setColor(curCol);
            bold = false;
          } else if (code == 'r') {
            curCol = baseColor;
            setColor(curCol);
            bold = false;
          } else if (code == 'l') {
            bold = true;
          }
        }
        continue;
      }

      if (c >= 0xE0 && c <= 0xEF && i + 2 < text.size()) {
        unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
        unsigned char c3 = static_cast<unsigned char>(text[i + 2]);
        uint32_t cp = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        i += 2;

        uint32_t page = cp >> 8;
        uint32_t glyph = cp & 0xFF;
        GLuint uTex = 0;
        if (page == 0x25) uTex = unicodeTex25;
        else if (page == 0x26) uTex = unicodeTex26;
        else if (page == 0x27) uTex = unicodeTex27;

        if (uTex != 0) {
          int col = glyph % 16;
          int row = glyph / 16;
          float u1 = (float)(col * 16) / 256.0f;
          float v1 = (float)(row * 16) / 256.0f;
          float u2 = (float)((col + 1) * 16) / 256.0f;
          float v2 = (float)((row + 1) * 16) / 256.0f;

          glEnd();
          glBindTexture(GL_TEXTURE_2D, uTex);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glBegin(GL_QUADS);

          glTexCoord2f(u1, v1); glVertex2f(curX, curY);
          glTexCoord2f(u1, v2); glVertex2f(curX, curY + 7.99f);
          glTexCoord2f(u2, v2); glVertex2f(curX + 8.0f, curY + 7.99f);
          glTexCoord2f(u2, v1); glVertex2f(curX + 8.0f, curY);

          if (bold) {
            glTexCoord2f(u1, v1); glVertex2f(curX + 0.5f, curY);
            glTexCoord2f(u1, v2); glVertex2f(curX + 0.5f, curY + 7.99f);
            glTexCoord2f(u2, v2); glVertex2f(curX + 8.5f, curY + 7.99f);
            glTexCoord2f(u2, v1); glVertex2f(curX + 8.5f, curY);
          }

          glEnd();
          glBindTexture(GL_TEXTURE_2D, fontTextureId);
          glBegin(GL_QUADS);

          curX += (bold ? 9.0F : 8.0F);
        } else {
          glEnd();
          glDisable(GL_TEXTURE_2D);

          glBegin(GL_QUADS);
          auto drawPixelRect = [&](float px, float py, float pw, float ph) {
            glVertex2f(curX + px, curY + py);
            glVertex2f(curX + px, curY + py + ph);
            glVertex2f(curX + px + pw, curY + py + ph);
            glVertex2f(curX + px + pw, curY + py);
          };
          drawPixelRect(3.5f, 1.0f, 1.0f, 1.0f);
          drawPixelRect(3.0f, 2.0f, 2.0f, 1.0f);
          drawPixelRect(1.0f, 3.0f, 6.0f, 1.0f);
          drawPixelRect(2.0f, 4.0f, 4.0f, 1.0f);
          drawPixelRect(2.0f, 5.0f, 1.5f, 1.0f);
          drawPixelRect(4.5f, 5.0f, 1.5f, 1.0f);
          drawPixelRect(1.5f, 6.0f, 1.5f, 1.0f);
          drawPixelRect(5.0f, 6.0f, 1.5f, 1.0f);
          glEnd();

          glEnable(GL_TEXTURE_2D);
          glBindTexture(GL_TEXTURE_2D, fontTextureId);
          glBegin(GL_QUADS);

          curX += (bold ? 9.0F : 8.0F);
        }
        continue;
      }

      if (c == ' ') {
        float sp = charWidth[32] > 0 ? static_cast<float>(charWidth[32]) : 4.0F;
        curX += sp;
        continue;
      }

      int cw = charWidth[c];
      if (cw <= 0) continue;

      float quadW = static_cast<float>(cw > 1 ? cw - 1 : cw);
      float cellW = 8.0f * texScaleX;
      float cellH = 8.0f * texScaleY;
      float colX = (float)(c % 16) * cellW;
      float rowY = (float)(c / 16) * cellH;
      float quadW_px = quadW * texScaleX;

      float u1 = (colX + 0.05F * texScaleX) / (float)texW;
      float v1 = (rowY + 0.05F * texScaleY) / (float)texH;
      float u2 = (colX + quadW_px - 0.05F * texScaleX) / (float)texW;
      float v2 = (rowY + 7.95F * texScaleY) / (float)texH;

      glTexCoord2f(u1, v1); glVertex2f(curX, curY);
      glTexCoord2f(u1, v2); glVertex2f(curX, curY + 7.99F);
      glTexCoord2f(u2, v2); glVertex2f(curX + quadW, curY + 7.99F);
      glTexCoord2f(u2, v1); glVertex2f(curX + quadW, curY);

      if (bold) {
        glTexCoord2f(u1, v1); glVertex2f(curX + 1.0F, curY);
        glTexCoord2f(u1, v2); glVertex2f(curX + 1.0F, curY + 7.99F);
        glTexCoord2f(u2, v2); glVertex2f(curX + 1.0F + quadW, curY + 7.99F);
        glTexCoord2f(u2, v1); glVertex2f(curX + 1.0F + quadW, curY);
      }

      float adv = static_cast<float>(cw);
      if (bold) adv += 1.0F;
      curX += adv;
    }
    glEnd();
    glDisable(GL_ALPHA_TEST);
    if (prevTex > 0) {
      glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));
    }
  }

  void drawString(const std::string &text, float x, float y, uint32_t color) const {
    renderPass(text, x, y, color, false);
  }

  void drawStringWithShadow(const std::string &text, float x, float y, uint32_t color) const {
    renderPass(text, x + 1.0F, y + 1.0F, color, true);
    renderPass(text, x, y, color, false);
  }
};

extern McFont g_mcFont;

} // namespace Render
