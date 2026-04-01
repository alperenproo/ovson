#include "ReplaySpammer.h"
#include "../Chat/ChatSDK.h"
#include "../Utils/Logger.h"
#include <vector>
#include <windows.h>

#include "../Java.h"

namespace Utils {

ReplaySpammer &ReplaySpammer::getInstance() {
  static ReplaySpammer instance;
  return instance;
}

void ReplaySpammer::toggle() {
  enabled = !enabled;
  if (enabled) {
    state = 0;
    spamCount = 0;
    ChatSDK::showPrefixed("§aReplay Spammer Enabled");
  } else {
    ChatSDK::showPrefixed("§cReplay Spammer Disabled. §7Spammed: " +
                          std::to_string(spamCount) + " times.");
  }
}

void ReplaySpammer::setEnabled(bool e) {
  if (enabled != e)
    toggle();
}

bool ReplaySpammer::isEnabled() const { return enabled; }

struct JNIFrameGuard {
  JNIEnv *env;
  bool pushed;
  JNIFrameGuard(JNIEnv *e, jint cap) : env(e), pushed(false) {
    if (env->PushLocalFrame(cap) == 0)
      pushed = true;
  }
  ~JNIFrameGuard() {
    if (pushed)
      env->PopLocalFrame(nullptr);
  }
};

void ReplaySpammer::tick() {
  if (!enabled || !lc)
    return;

  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  ULONGLONG now = GetTickCount64();
  if (now - lastAction < 300)
    return;

  JNIFrameGuard frameGuard(env, 256);
  if (!frameGuard.pushed)
    return;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return;
  jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                        "Lnet/minecraft/client/Minecraft;",
                                        "field_71432_P", "S", "Lave;");
  if (!theMc)
    return;
  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return;

  jfieldID f_screen = lc->GetFieldID(mcCls, "currentScreen",
                                     "Lnet/minecraft/client/gui/GuiScreen;",
                                     "field_71462_r", "m", "Laxu;");
  if (!f_screen) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    f_screen = lc->GetFieldID(mcCls, "currentScreen",
                              "Lnet/minecraft/client/gui/GuiScreen;",
                              "field_71462_r", "ay", "Laxu;");
  }
  jobject screen = f_screen ? env->GetObjectField(mcObj, f_screen) : nullptr;

  jfieldID f_player = lc->GetFieldID(
      mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
      "field_71439_g", "h", "Lbew;");
  jobject player = f_player ? env->GetObjectField(mcObj, f_player) : nullptr;

  jfieldID f_pc =
      lc->GetFieldID(mcCls, "playerController",
                     "Lnet/minecraft/client/multiplayer/PlayerControllerMP;",
                     "field_71442_b", "c", "Lbda;");
  jobject pc = f_pc ? env->GetObjectField(mcObj, f_pc) : nullptr;

  if (!player || !pc)
    return;

  if (state == 0) {
    if (!screen) {
      jmethodID m_rightClick = lc->GetMethodID(mcCls, "rightClickMouse", "()V",
                                               "func_147121_ag", "ax");
      if (m_rightClick) {
        env->CallVoidMethod(mcObj, m_rightClick);
        if (env->ExceptionCheck())
          env->ExceptionClear();
      }
      state = 1;
      lastAction = now;
    } else {
      jclass guiContainerCls =
          lc->GetClass("net.minecraft.client.gui.inventory.GuiContainer");
      if (guiContainerCls && env->IsInstanceOf(screen, guiContainerCls)) {
        state = 1;
        lastAction = now;
      } else {
        jmethodID m_close =
            lc->GetMethodID(nullptr, "closeScreen", "()V", "func_71053_j", "n");
        if (m_close) {
          env->CallVoidMethod(player, m_close);
          if (env->ExceptionCheck())
            env->ExceptionClear();
        }
        lastAction = now;
      }
    }
  } else if (state == 1 || state == 2) {
    if (screen) {
      jclass guiContainerCls =
          lc->GetClass("net.minecraft.client.gui.inventory.GuiContainer");
      if (guiContainerCls && env->IsInstanceOf(screen, guiContainerCls)) {
        jfieldID f_invSlots =
            lc->GetFieldID(guiContainerCls, "inventorySlots",
                           "Lnet/minecraft/inventory/Container;",
                           "field_147002_h", "h", "Lxi;");

        jobject container =
            f_invSlots ? env->GetObjectField(screen, f_invSlots) : nullptr;
        if (container) {
          jclass containerCls = env->GetObjectClass(container);
          jfieldID f_windowId = lc->GetFieldID(containerCls, "windowId", "I",
                                               "field_75152_c", "d");

          int windowId =
              f_windowId ? env->GetIntField(container, f_windowId) : 0;

          int targetSlot = -1;
          std::string allNames = "";
          bool foundReportIconInState2 = false;

          jmethodID m_getSlot = lc->GetMethodID(
              containerCls, "getSlot", "(I)Lnet/minecraft/inventory/Slot;",
              "func_75139_a", "a", "(I)Lyg;");

          jfieldID f_invSlotsList =
              lc->GetFieldID(containerCls, "inventorySlots", "Ljava/util/List;",
                             "field_75151_b", "c", "Ljava/util/List;");

          jobject slotsList =
              f_invSlotsList ? env->GetObjectField(container, f_invSlotsList)
                             : nullptr;

          if (slotsList && m_getSlot) {
            jclass listCls = env->GetObjectClass(slotsList);
            jmethodID m_size = env->GetMethodID(listCls, "size", "()I");

            int size = env->CallIntMethod(slotsList, m_size);
            if (env->ExceptionCheck())
              env->ExceptionClear();

            for (int i = 0; i < size; i++) {
              jobject slotObj = env->CallObjectMethod(container, m_getSlot, i);
              if (env->ExceptionCheck())
                env->ExceptionClear();

              if (slotObj) {
                jclass slotCls = env->GetObjectClass(slotObj);
                jmethodID m_getStack = lc->GetMethodID(
                    slotCls, "getStack", "()Lnet/minecraft/item/ItemStack;",
                    "func_75211_c", "d", "()Lzx;");

                jobject itemStack = nullptr;
                if (m_getStack) {
                  itemStack = env->CallObjectMethod(slotObj, m_getStack);
                  if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    itemStack = nullptr;
                  }
                }

                if (itemStack) {
                  jclass stackCls = env->GetObjectClass(itemStack);
                  jmethodID m_getDisplayName = lc->GetMethodID(
                      stackCls, "getDisplayName", "()Ljava/lang/String;",
                      "func_82833_r", "q");

                  std::string dispName = "";
                  if (m_getDisplayName) {
                    jstring jName = (jstring)env->CallObjectMethod(
                        itemStack, m_getDisplayName);
                    if (env->ExceptionCheck()) {
                      env->ExceptionClear();
                    } else if (jName) {
                      const char *nameStr = env->GetStringUTFChars(jName, 0);
                      if (nameStr) {
                        dispName = nameStr;
                        env->ReleaseStringUTFChars(jName, nameStr);
                      }
                      env->DeleteLocalRef(jName);
                    }
                  }

                  std::string cleanName = "";
                  for (size_t k = 0; k < dispName.length();) {
                    unsigned char c = dispName[k];
                    if (c == 0xC2 && k + 2 < dispName.length() &&
                        (unsigned char)dispName[k + 1] == 0xA7) {
                      k += 3;
                    } else if (c == 0xA7 && k + 1 < dispName.length()) {
                      k += 2;
                    } else {
                      cleanName += std::tolower(c);
                      k++;
                    }
                  }

                  if (!cleanName.empty() && allNames.length() < 200) {
                    allNames += cleanName + ", ";
                  }

                  if (state == 1) {
                    if (cleanName.find("report") != std::string::npos ||
                        cleanName.find("anvil") != std::string::npos ||
                        cleanName.find("confirm") != std::string::npos) {
                      targetSlot = i;
                    }
                  } else if (state == 2) {
                    if (cleanName.find("cheating") != std::string::npos) {
                      targetSlot = i;
                    } else if (cleanName.find("report") != std::string::npos ||
                               cleanName.find("anvil") != std::string::npos) {
                      foundReportIconInState2 = true;
                    }
                  }
                  env->DeleteLocalRef(stackCls);
                  env->DeleteLocalRef(itemStack);
                }
                env->DeleteLocalRef(slotCls);
                env->DeleteLocalRef(slotObj);
              }
              if (targetSlot != -1)
                break;
            }
            env->DeleteLocalRef(listCls);
            env->DeleteLocalRef(slotsList);
          }

          if (state == 2 && targetSlot == -1 && foundReportIconInState2) {
            state = 1;
            lastAction = now;
          } else if (targetSlot != -1) {
            jclass pcCls = env->GetObjectClass(pc);
            jmethodID m_windowClick =
                lc->GetMethodID(pcCls, "windowClick",
                                "(IIIILnet/minecraft/entity/player/"
                                "EntityPlayer;)Lnet/minecraft/item/ItemStack;",
                                "func_78753_a", "a", "(IIIILwn;)Lzx;");

            if (m_windowClick) {
              env->CallObjectMethod(pc, m_windowClick, windowId, targetSlot, 0,
                                    0, player);
              if (env->ExceptionCheck())
                env->ExceptionClear();
              state++;
              if (state > 2) {
                state = 3;
              }
              lastAction = now;
            } else {
              state = 0;
            }
            env->DeleteLocalRef(pcCls);
          } else {
            if (now - lastAction > 600) {
              ChatSDK::showPrefixed("§c[Spammer] Timed out in state " +
                                    std::to_string(state) +
                                    ". Found: " + allNames);
              state = 0;
            }
          }
          env->DeleteLocalRef(containerCls);
          env->DeleteLocalRef(container);
        }
      } else {
        if (now - lastAction > 500) {
          state = 0;
        }
      }
    } else {
      state = 0;
    }
  } else if (state == 3) {
    jmethodID m_close =
        lc->GetMethodID(nullptr, "closeScreen", "()V", "func_71053_j", "n");
    if (m_close) {
      env->CallVoidMethod(player, m_close);
      if (env->ExceptionCheck())
        env->ExceptionClear();
    }

    spamCount++;
    state = 0;
    lastAction = now + 100;
  }
}
} // namespace Utils
