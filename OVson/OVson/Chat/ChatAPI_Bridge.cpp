#include "ChatAPI_Bridge.h"
#include "../Java.h"
#include "ChatSDK.h"
#include <string>

static std::string JStringToStdString(JNIEnv* env, jstring jStr) {
    if (!jStr) return "";
    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    std::string ret(chars);
    env->ReleaseStringUTFChars(jStr, chars);
    return ret;
}


JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_sendChatMessage
  (JNIEnv *env, jclass cls, jstring message)
{
    std::string msg = JStringToStdString(env, message);
    ChatSDK::sendClientChat(msg);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_showClientMessage
  (JNIEnv *env, jclass cls, jstring message)
{
    std::string msg = JStringToStdString(env, message);
    ChatSDK::showClientMessage(msg);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_showPrefixedMessage
  (JNIEnv *env, jclass cls, jstring message)
{
    std::string msg = JStringToStdString(env, message);
    ChatSDK::showPrefixed(msg);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_showJsonMessage
  (JNIEnv *env, jclass cls, jstring json, jstring fallback)
{
    std::string j = JStringToStdString(env, json);
    std::string f = JStringToStdString(env, fallback);
    ChatSDK::showJsonMessage(j, f);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_showActionBar
  (JNIEnv *env, jclass cls, jstring message)
{
    std::string msg = JStringToStdString(env, message);
    ChatSDK::showActionBar(msg);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_showTitle
  (JNIEnv *env, jclass cls, jstring title, jstring subtitle,
   jint fadeIn, jint stay, jint fadeOut)
{
    std::string t = JStringToStdString(env, title);
    std::string s = JStringToStdString(env, subtitle);
    ChatSDK::showTitle(t, s, (int)fadeIn, (int)stay, (int)fadeOut);
}

JNIEXPORT void JNICALL Java_net_ovson_api_chat_ChatAPI_clearChat
  (JNIEnv *env, jclass cls)
{
    ChatSDK::clearChat();
}

JNIEXPORT jobject JNICALL Java_net_ovson_api_chat_ChatAPI_getChatHistory
  (JNIEnv *env, jclass cls, jint maxCount)
{
    auto history = ChatSDK::getChatHistory((int)maxCount);
    jclass listCls = env->FindClass("java/util/ArrayList");
    jmethodID listCtor = env->GetMethodID(listCls, "<init>", "()V");
    jmethodID addMethod = env->GetMethodID(listCls, "add", "(Ljava/lang/Object;)Z");
    jobject list = env->NewObject(listCls, listCtor);
    for (auto& s : history) {
        jstring js = env->NewStringUTF(s.c_str());
        env->CallBooleanMethod(list, addMethod, js);
        env->DeleteLocalRef(js);
    }
    env->DeleteLocalRef(listCls);
    return list;
}

JNIEXPORT jobject JNICALL Java_net_ovson_api_chat_ChatAPI_getSentHistory
  (JNIEnv *env, jclass cls, jint maxCount)
{
    auto history = ChatSDK::getSentHistory((int)maxCount);
    jclass listCls = env->FindClass("java/util/ArrayList");
    jmethodID listCtor = env->GetMethodID(listCls, "<init>", "()V");
    jmethodID addMethod = env->GetMethodID(listCls, "add", "(Ljava/lang/Object;)Z");
    jobject list = env->NewObject(listCls, listCtor);
    for (auto& s : history) {
        jstring js = env->NewStringUTF(s.c_str());
        env->CallBooleanMethod(list, addMethod, js);
        env->DeleteLocalRef(js);
    }
    env->DeleteLocalRef(listCls);
    return list;
}

namespace ChatAPIBridge {
    void registerNatives(JNIEnv* env, jclass cls) {
        if (!cls) {
            return;
        }

        JNINativeMethod methods[] = {
            {(char*)"sendChatMessage", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_chat_ChatAPI_sendChatMessage},
            {(char*)"showClientMessage", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_chat_ChatAPI_showClientMessage},
            {(char*)"showPrefixedMessage", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_chat_ChatAPI_showPrefixedMessage},
            {(char*)"showJsonMessage", (char*)"(Ljava/lang/String;Ljava/lang/String;)V", (void*)&Java_net_ovson_api_chat_ChatAPI_showJsonMessage},
            {(char*)"showActionBar", (char*)"(Ljava/lang/String;)V", (void*)&Java_net_ovson_api_chat_ChatAPI_showActionBar},
            {(char*)"showTitle", (char*)"(Ljava/lang/String;Ljava/lang/String;III)V", (void*)&Java_net_ovson_api_chat_ChatAPI_showTitle},
            {(char*)"clearChat", (char*)"()V", (void*)&Java_net_ovson_api_chat_ChatAPI_clearChat},
            {(char*)"getChatHistory", (char*)"(I)Ljava/util/List;", (void*)&Java_net_ovson_api_chat_ChatAPI_getChatHistory},
            {(char*)"getSentHistory", (char*)"(I)Ljava/util/List;", (void*)&Java_net_ovson_api_chat_ChatAPI_getSentHistory}
        };

        jint res = env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
        if (res != JNI_OK || env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }
}
