#pragma once
#include "../JNI/jni.h"

namespace JniGuard {

class JniExceptionScope {
public:
  explicit JniExceptionScope(JNIEnv *env) : m_env(env) {}
  ~JniExceptionScope() {
    if (m_env && m_env->ExceptionCheck()) {
      m_env->ExceptionClear();
    }
  }
  JniExceptionScope(const JniExceptionScope &) = delete;
  JniExceptionScope &operator=(const JniExceptionScope &) = delete;

private:
  JNIEnv *m_env;
};

class JniLocalFrame {
public:
  explicit JniLocalFrame(JNIEnv *env, jint capacity = 64) : m_env(env) {
    if (m_env && m_env->PushLocalFrame(capacity) == 0) {
      m_active = true;
    } else if (m_env && m_env->ExceptionCheck()) {
      m_env->ExceptionClear();
    }
  }
  ~JniLocalFrame() {
    if (m_active && m_env) m_env->PopLocalFrame(nullptr);
  }
  JniLocalFrame(const JniLocalFrame &) = delete;
  JniLocalFrame &operator=(const JniLocalFrame &) = delete;

private:
  JNIEnv *m_env;
  bool    m_active = false;
};

class JniScope {
public:
  explicit JniScope(JNIEnv *env, jint capacity = 64)
      : m_frame(env, capacity), m_exc(env) {}

private:
  JniLocalFrame      m_frame;
  JniExceptionScope  m_exc;
};

} // namespace JniGuard
