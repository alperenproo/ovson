#include "ChatSDK.h"
#include "../Config/Config.h"
#include "../Java.h"
#include "../Utils/Logger.h"
#include <cstdarg>
#include <vector>


static bool callSendChatMessage(const std::string &text) {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;
  CMinecraft mc;
  CPlayer player = mc.GetLocalPlayer();
  if (!player.Get())
    return false;

  jclass playerCls = lc->GetClass("net.minecraft.client.entity.EntityPlayerSP");
  if (!playerCls) {
    player.Cleanup();
    return false;
  }

  jmethodID sendChat =
      lc->GetMethodID(playerCls, "sendChatMessage", "(Ljava/lang/String;)V",
                      "func_71165_d", "e");

  if (!sendChat) {
    player.Cleanup();
    return false;
  }

  jstring jtext = env->NewStringUTF(text.c_str());
  env->CallVoidMethod(player.Get(), sendChat, jtext);
  env->DeleteLocalRef(jtext);
  player.Cleanup();
  return true;
}

static bool callAddChatMessage(const std::string &text) {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;
  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls)
    return false;

  jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                        "Lnet/minecraft/client/Minecraft;",
                                        "field_71432_P", "S", "Lave;");
  if (!theMc)
    return false;

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return false;

  jfieldID f_ingame =
      lc->GetFieldID(mcCls, "ingameGUI", "Lnet/minecraft/client/gui/GuiIngame;",
                     "field_71456_v", "q", "Lavo;");
  if (!f_ingame)
    f_ingame =
        lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
  if (!f_ingame)
    f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");

  if (!f_ingame) {
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jobject ingame = env->GetObjectField(mcObj, f_ingame);
  if (!ingame) {
    env->DeleteLocalRef(mcObj);
    return false;
  }

  // get chat gui
  jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
  if (!igCls) {
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jmethodID getChatGUI = lc->GetMethodID(
      igCls, "getChatGUI", "()Lnet/minecraft/client/gui/GuiNewChat;",
      "func_146158_b", "d", "()Lavt;");
  if (!getChatGUI)
    getChatGUI = lc->FindMethodBySignature(
        igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
  if (!getChatGUI)
    getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");

  if (!getChatGUI) {
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
  if (!chatGui) {
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jclass cctCls = lc->GetClass("net.minecraft.util.ChatComponentText");
  if (!cctCls) {
    env->DeleteLocalRef(chatGui);
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }
  jmethodID cctCtor =
      env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
  if (!cctCtor) {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    env->DeleteLocalRef(chatGui);
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }
  jstring jtext = env->NewStringUTF(text.c_str());
  jobject component = env->NewObject(cctCls, cctCtor, jtext);

  jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
  if (!gncCls) {
    env->DeleteLocalRef(component);
    env->DeleteLocalRef(jtext);
    env->DeleteLocalRef(chatGui);
    env->DeleteLocalRef(ingame);
    env->DeleteLocalRef(mcObj);
    return false;
  }
  jmethodID print = lc->GetMethodID(gncCls, "printChatMessage",
                                    "(Lnet/minecraft/util/IChatComponent;)V",
                                    "func_146227_a", "a", "(Leu;)V");
  if (!print)
    print = lc->FindMethodBySignature(gncCls,
                                      "(Lnet/minecraft/util/IChatComponent;)V");
  if (!print)
    print = lc->FindMethodBySignature(gncCls, "(Leu;)V");

  if (print) {
    env->CallVoidMethod(chatGui, print, component);
  }

  env->DeleteLocalRef(jtext);
  env->DeleteLocalRef(component);
  env->DeleteLocalRef(chatGui);
  env->DeleteLocalRef(ingame);
  env->DeleteLocalRef(mcObj);
  return true;
}

bool ChatSDK::sendClientChat(const std::string &text) {
  return callSendChatMessage(text);
}

bool ChatSDK::showClientMessage(const std::string &text) {
  return callAddChatMessage(text);
}

bool ChatSDK::showJsonMessage(const std::string &json,
                              const std::string &fallback) {
  JNIEnv *env = lc->getEnv();
  if (!env) {
    return fallback.empty() ? false : callAddChatMessage(fallback);
  }

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) goto fall;
  {
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                          "Lnet/minecraft/client/Minecraft;",
                                          "field_71432_P", "S", "Lave;");
    if (!theMc) goto fall;
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) goto fall;

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
                                       "Lnet/minecraft/client/gui/GuiIngame;",
                                       "field_71456_v", "q", "Lavo;");
    if (!f_ingame)
      f_ingame = lc->FindFieldBySignature(
          mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); goto fall; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) goto fall;

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); goto fall; }

    jmethodID getChatGUI = lc->GetMethodID(
        igCls, "getChatGUI", "()Lnet/minecraft/client/gui/GuiNewChat;",
        "func_146158_b", "d", "()Lavt;");
    if (!getChatGUI)
      getChatGUI = lc->FindMethodBySignature(
          igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
    if (!getChatGUI)
      getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");
    if (!getChatGUI) { env->DeleteLocalRef(ingame); goto fall; }

    jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
    env->DeleteLocalRef(ingame);
    if (!chatGui) goto fall;

    jclass serCls = lc->GetClass("net.minecraft.util.IChatComponent$Serializer");
    jmethodID jsonToComp = nullptr;
    if (serCls) {
      jsonToComp = env->GetStaticMethodID(serCls, "jsonToComponent",
          "(Ljava/lang/String;)Lnet/minecraft/util/IChatComponent;");
      if (!jsonToComp) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        jsonToComp = env->GetStaticMethodID(serCls, "func_150699_a",
            "(Ljava/lang/String;)Lnet/minecraft/util/IChatComponent;");
      }
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (!jsonToComp) {
      env->DeleteLocalRef(chatGui);
      goto fall;
    }

    jstring jsonStr = env->NewStringUTF(json.c_str());
    jobject component = env->CallStaticObjectMethod(serCls, jsonToComp,
                                                     jsonStr);
    if (env->ExceptionCheck()) {
      Logger::error("ChatSDK::showJsonMessage: JNI exception occurred when deserializing json = %s", json.c_str());
      env->ExceptionClear();
      env->DeleteLocalRef(jsonStr);
      env->DeleteLocalRef(chatGui);
      goto fall;
    }
    env->DeleteLocalRef(jsonStr);
    if (!component) {
      env->DeleteLocalRef(chatGui);
      goto fall;
    }

    jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
    if (!gncCls) {
      env->DeleteLocalRef(component);
      env->DeleteLocalRef(chatGui);
      goto fall;
    }
    jmethodID print = lc->GetMethodID(gncCls, "printChatMessage",
        "(Lnet/minecraft/util/IChatComponent;)V",
        "func_146227_a", "a", "(Leu;)V");
    if (!print)
      print = lc->FindMethodBySignature(
          gncCls, "(Lnet/minecraft/util/IChatComponent;)V");
    if (!print) print = lc->FindMethodBySignature(gncCls, "(Leu;)V");

    if (print) {
      env->CallVoidMethod(chatGui, print, component);
    }
    env->DeleteLocalRef(component);
    env->DeleteLocalRef(chatGui);
    return true;
  }
fall:
  return fallback.empty() ? false : callAddChatMessage(fallback);
}

bool ChatSDK::showTagsMessage(const std::string &msg, const std::vector<std::pair<std::string, std::string>> &tags) {
  JNIEnv *env = lc->getEnv();
  if (!env) return false;

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mcCls) return false;

  jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                        "Lnet/minecraft/client/Minecraft;",
                                        "field_71432_P", "S", "Lave;");
  if (!theMc) return false;

  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj) return false;

  jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI", "Lnet/minecraft/client/gui/GuiIngame;",
                                     "field_71456_v", "q", "Lavo;");
  if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
  if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");

  if (!f_ingame) {
    env->DeleteLocalRef(mcObj);
    return false;
  }

  jobject ingame = env->GetObjectField(mcObj, f_ingame);
  env->DeleteLocalRef(mcObj);
  if (!ingame) return false;

  jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
  if (!igCls) {
    env->DeleteLocalRef(ingame);
    return false;
  }

  jmethodID getChatGUI = lc->GetMethodID(
      igCls, "getChatGUI", "()Lnet/minecraft/client/gui/GuiNewChat;",
      "func_146158_b", "d", "()Lavt;");
  if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
  if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");

  if (!getChatGUI) {
    env->DeleteLocalRef(ingame);
    return false;
  }

  jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
  env->DeleteLocalRef(ingame);
  if (!chatGui) return false;

  jclass cctCls = lc->GetClass("net.minecraft.util.ChatComponentText");
  if (!cctCls) {
    env->DeleteLocalRef(chatGui);
    return false;
  }

  jmethodID cctCtor = env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
  if (!cctCtor) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(chatGui);
    return false;
  }

  jstring jmsg = env->NewStringUTF(msg.c_str());
  jobject mainComp = env->NewObject(cctCls, cctCtor, jmsg);
  env->DeleteLocalRef(jmsg);

  jclass iccCls = lc->GetClass("net.minecraft.util.IChatComponent");
  jmethodID appendSibling = lc->FindMethodBySignature(iccCls, "(Lnet/minecraft/util/IChatComponent;)Lnet/minecraft/util/IChatComponent;");
  if (!appendSibling) appendSibling = lc->FindMethodBySignature(iccCls, "(Leu;)Leu;");

  jmethodID getChatStyle = lc->FindMethodBySignature(iccCls, "()Lnet/minecraft/util/ChatStyle;");
  if (!getChatStyle) getChatStyle = lc->FindMethodBySignature(iccCls, "()Lez;");

  jclass csCls = lc->GetClass("net.minecraft.util.ChatStyle");
  jmethodID setChatHoverEvent = lc->FindMethodBySignature(csCls, "(Lnet/minecraft/event/HoverEvent;)Lnet/minecraft/util/ChatStyle;");
  if (!setChatHoverEvent) setChatHoverEvent = lc->FindMethodBySignature(csCls, "(Lew;)Lez;");

  jclass heCls = lc->GetClass("net.minecraft.event.HoverEvent");
  jmethodID heCtor = env->GetMethodID(heCls, "<init>", "(Lnet/minecraft/event/HoverEvent$Action;Lnet/minecraft/util/IChatComponent;)V");
  if (!heCtor) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    heCtor = env->GetMethodID(heCls, "<init>", "(Lew$a;Leu;)V");
  }

  jclass actionCls = lc->GetClass("net.minecraft.event.HoverEvent$Action");
  jfieldID showTextField = env->GetStaticFieldID(actionCls, "SHOW_TEXT", "Lnet/minecraft/event/HoverEvent$Action;");
  if (!showTextField) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    showTextField = env->GetStaticFieldID(actionCls, "SHOW_TEXT", "Lew$a;");
  }

  jobject showTextAction = nullptr;
  if (showTextField) {
    showTextAction = env->GetStaticObjectField(actionCls, showTextField);
  }

  if (appendSibling && getChatStyle && setChatHoverEvent && heCtor && showTextAction) {
    for (const auto &tag : tags) {
      jstring jtagText = env->NewStringUTF(tag.first.c_str());
      jobject tagComp = env->NewObject(cctCls, cctCtor, jtagText);
      env->DeleteLocalRef(jtagText);

      if (!tag.second.empty()) {
        jstring jreasonText = env->NewStringUTF(tag.second.c_str());
        jobject reasonComp = env->NewObject(cctCls, cctCtor, jreasonText);
        env->DeleteLocalRef(jreasonText);

        jobject hoverEvent = env->NewObject(heCls, heCtor, showTextAction, reasonComp);
        env->DeleteLocalRef(reasonComp);

        if (hoverEvent) {
          jobject chatStyle = env->CallObjectMethod(tagComp, getChatStyle);
          if (chatStyle) {
            env->CallObjectMethod(chatStyle, setChatHoverEvent, hoverEvent);
            env->DeleteLocalRef(chatStyle);
          }
          env->DeleteLocalRef(hoverEvent);
        }
      }

      env->CallObjectMethod(mainComp, appendSibling, tagComp);
      env->DeleteLocalRef(tagComp);
    }
  } else {
    Logger::error("ChatSDK::showTagsMessage: Failed resolving JNI symbols! append=%p, getStyle=%p, setHover=%p, heCtor=%p, showTextAction=%p",
                  appendSibling, getChatStyle, setChatHoverEvent, heCtor, showTextAction);
  }

  if (showTextAction) env->DeleteLocalRef(showTextAction);

  jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
  jmethodID print = lc->GetMethodID(gncCls, "printChatMessage",
                                    "(Lnet/minecraft/util/IChatComponent;)V",
                                    "func_146227_a", "a", "(Leu;)V");
  if (!print)
    print = lc->FindMethodBySignature(gncCls, "(Lnet/minecraft/util/IChatComponent;)V");
  if (!print)
    print = lc->FindMethodBySignature(gncCls, "(Leu;)V");

  if (print) {
    env->CallVoidMethod(chatGui, print, mainComp);
  }

  env->DeleteLocalRef(mainComp);
  env->DeleteLocalRef(chatGui);

  if (env->ExceptionCheck()) env->ExceptionClear();
  return print != nullptr;
}

bool ChatSDK::showActionBar(const std::string& text) {
    JNIEnv* env = lc->getEnv();
    if (!env) return false;

    jclass cctCls = lc->GetClass("net.minecraft.util.ChatComponentText");
    if (!cctCls) return false;
    jmethodID cctCtor = env->GetMethodID(cctCls, "<init>", "(Ljava/lang/String;)V");
    if (!cctCtor) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
    jstring jtext = env->NewStringUTF(text.c_str());
    jobject component = env->NewObject(cctCls, cctCtor, jtext);
    env->DeleteLocalRef(jtext);

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) { env->DeleteLocalRef(component); return false; }
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
        "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) { env->DeleteLocalRef(component); return false; }
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) { env->DeleteLocalRef(component); return false; }

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
        "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); env->DeleteLocalRef(component); return false; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) { env->DeleteLocalRef(component); return false; }

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); env->DeleteLocalRef(component); return false; }

    jmethodID setRecord = lc->GetMethodID(igCls, "setRecordPlaying",
        "(Lnet/minecraft/util/IChatComponent;Z)V",
        "func_175188_a", "a", "(Leu;Z)V");
    if (!setRecord) setRecord = lc->FindMethodBySignature(igCls, "(Lnet/minecraft/util/IChatComponent;Z)V");
    if (!setRecord) setRecord = lc->FindMethodBySignature(igCls, "(Leu;Z)V");

    if (setRecord) {
        env->CallVoidMethod(ingame, setRecord, component, JNI_TRUE);
    }

    env->DeleteLocalRef(component);
    env->DeleteLocalRef(ingame);
    return setRecord != nullptr;
}

bool ChatSDK::showTitle(const std::string& title, const std::string& subtitle,
                        int fadeIn, int stay, int fadeOut) {
    JNIEnv* env = lc->getEnv();
    if (!env) return false;

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return false;
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
        "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) return false;
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) return false;

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
        "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); return false; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) return false;

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); return false; }

    jmethodID dispTitle = lc->GetMethodID(igCls, "displayTitle",
        "(Ljava/lang/String;Ljava/lang/String;III)V",
        "func_175178_a", "a", "(Ljava/lang/String;Ljava/lang/String;III)V");
    if (!dispTitle) dispTitle = lc->FindMethodBySignature(igCls, "(Ljava/lang/String;Ljava/lang/String;III)V");

    if (dispTitle) {
        jstring jTitle = title.empty() ? nullptr : env->NewStringUTF(title.c_str());
        jstring jSub = subtitle.empty() ? nullptr : env->NewStringUTF(subtitle.c_str());
        env->CallVoidMethod(ingame, dispTitle, jTitle, jSub, (jint)fadeIn, (jint)stay, (jint)fadeOut);
        if (jTitle) env->DeleteLocalRef(jTitle);
        if (jSub) env->DeleteLocalRef(jSub);
    }

    env->DeleteLocalRef(ingame);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return dispTitle != nullptr;
}

bool ChatSDK::clearChat() {
    JNIEnv* env = lc->getEnv();
    if (!env) return false;

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return false;
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
        "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) return false;
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) return false;

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
        "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); return false; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) return false;

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); return false; }
    jmethodID getChatGUI = lc->GetMethodID(igCls, "getChatGUI",
        "()Lnet/minecraft/client/gui/GuiNewChat;", "func_146158_b", "d", "()Lavt;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");
    if (!getChatGUI) { env->DeleteLocalRef(ingame); return false; }

    jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
    env->DeleteLocalRef(ingame);
    if (!chatGui) return false;

    jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
    if (!gncCls) { env->DeleteLocalRef(chatGui); return false; }

    jmethodID clearMsg = lc->GetMethodID(gncCls, "clearChatMessages", "()V",
        "func_146231_a", "a", "()V");
    if (!clearMsg) clearMsg = lc->FindMethodBySignature(gncCls, "()V");

    if (clearMsg) {
        env->CallVoidMethod(chatGui, clearMsg);
    }
    env->DeleteLocalRef(chatGui);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return clearMsg != nullptr;
}

std::vector<std::string> ChatSDK::getChatHistory(int maxCount) {
    std::vector<std::string> result;
    JNIEnv* env = lc->getEnv();
    if (!env) return result;

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return result;
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
        "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) return result;
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) return result;

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
        "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); return result; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) return result;

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); return result; }
    jmethodID getChatGUI = lc->GetMethodID(igCls, "getChatGUI",
        "()Lnet/minecraft/client/gui/GuiNewChat;", "func_146158_b", "d", "()Lavt;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");
    if (!getChatGUI) { env->DeleteLocalRef(ingame); return result; }

    jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
    env->DeleteLocalRef(ingame);
    if (!chatGui) return result;

    jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
    if (!gncCls) { env->DeleteLocalRef(chatGui); return result; }

    jfieldID f_chatLines = lc->GetFieldID(gncCls, "chatLines",
        "Ljava/util/List;", "field_146252_h", "h");
    if (!f_chatLines) f_chatLines = lc->FindFieldBySignature(gncCls, "Ljava/util/List;");
    if (!f_chatLines) { env->DeleteLocalRef(chatGui); return result; }

    jobject chatLines = env->GetObjectField(chatGui, f_chatLines);
    env->DeleteLocalRef(chatGui);
    if (!chatLines) return result;

    jclass listCls = env->GetObjectClass(chatLines);
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    int size = env->CallIntMethod(chatLines, sizeM);
    int count = (maxCount > 0 && maxCount < size) ? maxCount : size;

    for (int i = 0; i < count; i++) {
        jobject chatLine = env->CallObjectMethod(chatLines, getM, (jint)i);
        if (!chatLine) continue;

        jclass clCls = env->GetObjectClass(chatLine);
        jmethodID getComp = lc->GetMethodID(clCls, "getChatComponent",
            "()Lnet/minecraft/util/IChatComponent;", "func_151461_a", "a");
        if (!getComp) getComp = lc->FindMethodBySignature(clCls,
            "()Lnet/minecraft/util/IChatComponent;");
        if (!getComp) getComp = lc->FindMethodBySignature(clCls, "()Leu;");

        if (getComp) {
            jobject comp = env->CallObjectMethod(chatLine, getComp);
            if (comp) {
                jclass compCls = env->GetObjectClass(comp);
                jmethodID getUnfmt = lc->GetMethodID(compCls, "getUnformattedText",
                    "()Ljava/lang/String;", "func_150260_c", "c");
                if (!getUnfmt) getUnfmt = env->GetMethodID(compCls, "getUnformattedText", "()Ljava/lang/String;");
                if (getUnfmt) {
                    jstring jStr = (jstring)env->CallObjectMethod(comp, getUnfmt);
                    if (jStr) {
                        const char* c = env->GetStringUTFChars(jStr, nullptr);
                        if (c) {
                            result.push_back(c);
                            env->ReleaseStringUTFChars(jStr, c);
                        }
                        env->DeleteLocalRef(jStr);
                    }
                }
                env->DeleteLocalRef(compCls);
                env->DeleteLocalRef(comp);
            }
        }
        env->DeleteLocalRef(clCls);
        env->DeleteLocalRef(chatLine);
    }
    env->DeleteLocalRef(listCls);
    env->DeleteLocalRef(chatLines);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return result;
}

std::vector<std::string> ChatSDK::getSentHistory(int maxCount) {
    std::vector<std::string> result;
    JNIEnv* env = lc->getEnv();
    if (!env) return result;

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    if (!mcCls) return result;
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
        "Lnet/minecraft/client/Minecraft;", "field_71432_P", "S", "Lave;");
    if (!theMc) return result;
    jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
    if (!mcObj) return result;

    jfieldID f_ingame = lc->GetFieldID(mcCls, "ingameGUI",
        "Lnet/minecraft/client/gui/GuiIngame;", "field_71456_v", "q", "Lavo;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Lnet/minecraft/client/gui/GuiIngame;");
    if (!f_ingame) f_ingame = lc->FindFieldBySignature(mcCls, "Laxe;");
    if (!f_ingame) { env->DeleteLocalRef(mcObj); return result; }

    jobject ingame = env->GetObjectField(mcObj, f_ingame);
    env->DeleteLocalRef(mcObj);
    if (!ingame) return result;

    jclass igCls = lc->GetClass("net.minecraft.client.gui.GuiIngame");
    if (!igCls) { env->DeleteLocalRef(ingame); return result; }
    jmethodID getChatGUI = lc->GetMethodID(igCls, "getChatGUI",
        "()Lnet/minecraft/client/gui/GuiNewChat;", "func_146158_b", "d", "()Lavt;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lnet/minecraft/client/gui/GuiNewChat;");
    if (!getChatGUI) getChatGUI = lc->FindMethodBySignature(igCls, "()Lavt;");
    if (!getChatGUI) { env->DeleteLocalRef(ingame); return result; }

    jobject chatGui = env->CallObjectMethod(ingame, getChatGUI);
    env->DeleteLocalRef(ingame);
    if (!chatGui) return result;

    jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
    if (!gncCls) { env->DeleteLocalRef(chatGui); return result; }

    jmethodID getSent = lc->GetMethodID(gncCls, "getSentMessages",
        "()Ljava/util/List;", "func_146238_c", "e");
    if (!getSent) getSent = lc->FindMethodBySignature(gncCls, "()Ljava/util/List;");
    if (!getSent) { env->DeleteLocalRef(chatGui); return result; }

    jobject sentList = env->CallObjectMethod(chatGui, getSent);
    env->DeleteLocalRef(chatGui);
    if (!sentList) return result;

    jclass listCls = env->GetObjectClass(sentList);
    jmethodID sizeM = env->GetMethodID(listCls, "size", "()I");
    jmethodID getM = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
    int size = env->CallIntMethod(sentList, sizeM);
    int count = (maxCount > 0 && maxCount < size) ? maxCount : size;

    for (int i = 0; i < count; i++) {
        jstring jStr = (jstring)env->CallObjectMethod(sentList, getM, (jint)i);
        if (jStr) {
            const char* c = env->GetStringUTFChars(jStr, nullptr);
            if (c) {
                result.push_back(c);
                env->ReleaseStringUTFChars(jStr, c);
            }
            env->DeleteLocalRef(jStr);
        }
    }
    env->DeleteLocalRef(listCls);
    env->DeleteLocalRef(sentList);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return result;
}

std::string ChatSDK::formatPrefix() {
  const char *S = "\xC2\xA7";
  return std::string(S) + "0[" + S + "r" + S + "cO" + S + "6V" + S + "es" + S +
         "ao" + S + "bn" + S + "0]" + S + "r ";
}

bool ChatSDK::showPrefixed(const std::string &message) {
  if (message.find("FAILED") != std::string::npos &&
      message.find("CRITICAL") == std::string::npos &&
      !Config::isGlobalDebugEnabled()) {
    return false;
  }
  return showClientMessage(formatPrefix() + "§f" + message);
}

void ChatSDK::initialize() { Lunar::reporter = ChatSDK::showPrefixed; }

bool ChatSDK::showPrefixedf(const char *fmt, ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  return showPrefixed(buf);
}
