#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "../../Chat/ChatSDK.h"
#include "../../Config/Config.h"
#include "../../Java.h"
#include "../../Logic/StatsTracker.h"
#include "../../Render/RenderHook.h"
#include "../Logger.h"
#include "../SafeGuard.h"
#include "AcInternal.h"
#include "Anticheat.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Anticheat {

static std::ofstream g_dbgLog;
static std::mutex g_dbgLogMutex;
static std::ofstream g_abLog;
static std::mutex g_abLogMutex;
static std::ofstream g_mapLog;
static std::mutex g_mapLogMutex;

static std::string resolveMapLogPath() {
  char buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\mapping_debug.log";
  n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\mapping_debug.log";
  return "mapping_debug.log";
}

void mapLog(const char *fmt, ...) {
  std::lock_guard<std::mutex> lk(g_mapLogMutex);
  if (!g_mapLog.is_open()) {
    std::string path = resolveMapLogPath();
    g_mapLog.open(path, std::ios::app);
    if (g_mapLog.is_open()) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      g_mapLog << "\n[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
               << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
               << "] ===== Mapping session start (pid="
               << GetCurrentProcessId() << ", path=" << path << ") =====\n";
    }
  }
  if (!g_mapLog.is_open()) return;
  SYSTEMTIME st;
  GetLocalTime(&st);
  char prefix[40];
  sprintf_s(prefix, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute,
            st.wSecond, st.wMilliseconds);
  char body[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);
  g_mapLog << prefix << body << "\n";
  g_mapLog.flush();
}

static std::string resolveAbLogPath() {
  char buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\autoblock_debug.log";
  n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\autoblock_debug.log";
  return "autoblock_debug.log";
}

void abLog(const char *fmt, ...) {
  // return (writes go to %TEMP%\autoblock_debug.log).
  (void)fmt;
  return;
  /*
  std::lock_guard<std::mutex> lk(g_abLogMutex);
  if (!g_abLog.is_open()) {
    std::string path = resolveAbLogPath();
    g_abLog.open(path, std::ios::app);
    if (g_abLog.is_open()) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      g_abLog << "\n[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
              << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
              << "] ===== AutoBlock session start (pid="
              << GetCurrentProcessId() << ", path=" << path << ") =====\n";
    }
  }
  if (!g_abLog.is_open()) return;
  SYSTEMTIME st;
  GetLocalTime(&st);
  char prefix[40];
  sprintf_s(prefix, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute,
            st.wSecond, st.wMilliseconds);
  char body[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);
  g_abLog << prefix << body << "\n";
  g_abLog.flush();
  */
}

static std::string resolveAcLogPath() {
  char buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("TEMP", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\anticheat_debug.log";
  n = GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH);
  if (n > 0 && n < MAX_PATH) return std::string(buf) + "\\anticheat_debug.log";
  return "anticheat_debug.log";
}

void debugLog(const char *fmt, ...) {
  (void)fmt;
  return;
  /*
  std::lock_guard<std::mutex> lk(g_dbgLogMutex);
  if (!g_dbgLog.is_open()) {
    std::string path = resolveAcLogPath();
    g_dbgLog.open(path, std::ios::app);
    if (g_dbgLog.is_open()) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      g_dbgLog << "\n[" << st.wYear << "-" << st.wMonth << "-" << st.wDay
               << " " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
               << "] ===== Anticheat session start (pid="
               << GetCurrentProcessId() << ", path=" << path << ") =====\n";
    }
  }
  if (!g_dbgLog.is_open()) return;

  SYSTEMTIME st;
  GetLocalTime(&st);
  char prefix[40];
  sprintf_s(prefix, "[%02d:%02d:%02d.%03d t=%lu] ", st.wHour, st.wMinute,
            st.wSecond, st.wMilliseconds, GetCurrentThreadId());

  char body[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(body, sizeof(body), fmt, ap);
  va_end(ap);

  g_dbgLog << prefix << body << "\n";
  g_dbgLog.flush();
  */
}

struct JCache {
  bool ready = false;
  bool failed = false;
  jclass mcCls = nullptr;
  jfieldID f_theMc = nullptr;
  jfieldID f_theWorld = nullptr;
  jfieldID f_thePlayer = nullptr;
  jclass worldCls = nullptr;
  jfieldID f_playerEntities = nullptr;
  jclass listCls = nullptr;
  jmethodID m_listSize = nullptr;
  jmethodID m_listGet = nullptr;
  jclass entityCls = nullptr;
  jfieldID f_posX = nullptr, f_posY = nullptr, f_posZ = nullptr;
  jfieldID f_yaw = nullptr, f_pitch = nullptr;
  jfieldID f_onGround = nullptr;
  jfieldID f_isSwingInProgress = nullptr;
  jfieldID f_swingProgress = nullptr;
  jfieldID f_hurtTime = nullptr;
  jfieldID f_swingProgressInt = nullptr;
  jmethodID m_isSneaking = nullptr;
  jmethodID m_isSprinting = nullptr;
  jmethodID m_isRiding = nullptr;
  jmethodID m_getEntityId = nullptr;
  jmethodID m_getName = nullptr;
  jmethodID m_getHealth = nullptr;
  jfieldID f_ticksExisted = nullptr;
  jfieldID f_dataWatcher = nullptr;
  jfieldID f_isSneaking = nullptr;
  jmethodID m_getWatchableObjectByte = nullptr;
  jclass epCls = nullptr;
  jmethodID m_isBlocking = nullptr;
  jmethodID m_isUsingItem = nullptr;
  jmethodID m_getHeldItem = nullptr;
  jfieldID f_inventory = nullptr;
  jmethodID m_getCurrentItem = nullptr;
  jclass itemStackCls = nullptr;
  jmethodID m_isGetItem = nullptr;
  jclass itemCls = nullptr;
  jmethodID m_itemGetIdStatic = nullptr;

  static constexpr int kMaxSwingCandidates = 8;
  jfieldID swingCandidates[kMaxSwingCandidates] = {};
  int      swingCandidateCount = 0;
};
static JCache g_jc;

static jmethodID mid(jclass cls, const char *mcp, const char *srg,
                     const char *notch, const char *sig) {
  return lc->GetMethodID(cls, mcp, sig, srg, notch);
}
static jfieldID fid(jclass cls, const char *mcp, const char *srg,
                    const char *notch, const char *sig) {
  return lc->GetFieldID(cls, mcp, sig, srg, notch);
}

static constexpr int kMaxListCandidates = 12;
static jfieldID g_worldListCandidates[kMaxListCandidates] = {};
static int      g_worldListCandidateCount = 0;

static constexpr int kMaxRotCandidates = 16;
struct RotProbe {
  jfieldID id = nullptr;
  float minV = 1e9f, maxV = -1e9f;
  int   samples = 0;
};
struct SwingProbe {
  jfieldID id = nullptr;
  float lastV = 0.0f;
  int hits = 0;
};
static constexpr int kMaxSwingProbes = 32;
static RotProbe g_rotProbes[kMaxRotCandidates] = {};
static int      g_rotProbeCount = 0;
static SwingProbe g_swingProbes[kMaxSwingProbes] = {};
static int      g_swingProbeCount = 0;
static int      g_rotProbeTicks = 0;

static constexpr int kMaxBoolProbes = 64;
struct BoolProbe {
  jfieldID id = nullptr;
  char name[16] = {0};
  int lastV = -1;
};
static BoolProbe g_boolProbes[kMaxBoolProbes] = {};
static int      g_boolProbeCount = 0;

static constexpr int kMaxMethodProbes = 128;
struct MethodProbe {
  jmethodID id = nullptr;
  char name[16] = {0};
  int lastV = -1;
};
static MethodProbe g_methodProbes[kMaxMethodProbes] = {};
static int g_methodProbeCount = 0;
static bool     g_rotHealed = false;

static void ensureJni(JNIEnv *env) {
  if (g_jc.ready || g_jc.failed)
    return;

  jclass mc = lc->GetClass("net.minecraft.client.Minecraft");
  if (!mc) {
    g_jc.failed = true;
    return;
  }
  g_jc.mcCls = (jclass)env->NewGlobalRef(mc);
  g_jc.f_theMc = lc->GetStaticFieldID(g_jc.mcCls, "theMinecraft",
                                      "Lnet/minecraft/client/Minecraft;",
                                      "field_71432_P", "S", "Lave;");
  g_jc.f_theWorld = lc->GetFieldID(
      g_jc.mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
      "field_71441_e", "f", "Lbdb;");
  g_jc.f_thePlayer = lc->GetFieldID(
      g_jc.mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
      "field_71439_g", "h", "Lbew;");

  jclass world = lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
  if (world) {
    g_jc.worldCls = (jclass)env->NewGlobalRef(world);
    const char *peCandidates[] = {
        "playerEntities", "field_73010_i",
        "j", "k", "l", "i", "h", "m", "n", "g", "f", nullptr};
    for (int i = 0; peCandidates[i] && !g_jc.f_playerEntities; i++) {
      g_jc.f_playerEntities = env->GetFieldID(
          g_jc.worldCls, peCandidates[i], "Ljava/util/List;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (lc->jvmti) {
      jclass cur = env->GetSuperclass(g_jc.worldCls);
      int depth = 1;
      while (cur && depth < 4) {
        jint fcount = 0;
        jfieldID *fields = nullptr;
        if (lc->jvmti->GetClassFields(cur, &fcount, &fields) ==
            JVMTI_ERROR_NONE) {
          for (int i = 0; i < fcount; i++) {
            char *fName = nullptr, *fSig = nullptr;
            lc->jvmti->GetFieldName(cur, fields[i], &fName, &fSig, nullptr);
            if (fSig && strcmp(fSig, "Ljava/util/List;") == 0) {
              debugLog("ensureJni: world-parent List candidate name=%s "
                       "depth=%d", fName ? fName : "?", depth);
              if (fName &&
                  g_worldListCandidateCount < kMaxListCandidates) {
                jfieldID candId = env->GetFieldID(
                    g_jc.worldCls, fName, "Ljava/util/List;");
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (candId) {
                  bool dup = false;
                  for (int k = 0; k < g_worldListCandidateCount; k++)
                    if (g_worldListCandidates[k] == candId) {
                      dup = true; break;
                    }
                  if (!dup) {
                    g_worldListCandidates[g_worldListCandidateCount++] =
                        candId;
                  }
                  if (!g_jc.f_playerEntities) g_jc.f_playerEntities = candId;
                }
              }
            }
            if (fName) lc->jvmti->Deallocate((unsigned char *)fName);
            if (fSig)  lc->jvmti->Deallocate((unsigned char *)fSig);
          }
          if (fields) lc->jvmti->Deallocate((unsigned char *)fields);
        }
        jclass next = env->GetSuperclass(cur);
        env->DeleteLocalRef(cur);
        cur = next;
        depth++;
      }
      if (cur) env->DeleteLocalRef(cur);
    }
  }

  jclass jlist = env->FindClass("java/util/List");
  if (jlist) {
    g_jc.listCls = (jclass)env->NewGlobalRef(jlist);
    g_jc.m_listSize = env->GetMethodID(g_jc.listCls, "size", "()I");
    g_jc.m_listGet =
        env->GetMethodID(g_jc.listCls, "get", "(I)Ljava/lang/Object;");
    env->DeleteLocalRef(jlist);
  }

  auto tryFind = [&](const char *name) -> jclass {
    if (env->ExceptionCheck())
      env->ExceptionClear();
    jclass c = env->FindClass(name);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      c = nullptr;
    }
    return c;
  };
  // these mappings are frying me
  jclass entity = lc->GetClass("net.minecraft.entity.Entity");
  if (!entity)
    entity = tryFind("pk");
  if (!entity)
    entity = tryFind("sa");
  if (entity) {
    g_jc.entityCls = (jclass)env->NewGlobalRef(entity);
    g_jc.f_posX = fid(g_jc.entityCls, "posX", "field_70165_t", "p", "D");
    g_jc.f_posY = fid(g_jc.entityCls, "posY", "field_70163_u", "q", "D");
    g_jc.f_posZ = fid(g_jc.entityCls, "posZ", "field_70161_v", "r", "D");
    g_jc.f_yaw = fid(g_jc.entityCls, "rotationYaw", "field_70177_z", "y", "F");
    g_jc.f_pitch =
        fid(g_jc.entityCls, "rotationPitch", "field_70125_A", "z", "F");

    if (g_jc.f_yaw) g_rotProbes[g_rotProbeCount++].id = g_jc.f_yaw;
    if (g_jc.f_pitch &&
        g_rotProbeCount < kMaxRotCandidates &&
        g_rotProbes[0].id != g_jc.f_pitch)
      g_rotProbes[g_rotProbeCount++].id = g_jc.f_pitch;
    const char *fNotchCandidates[] = {
        "y", "z", "A", "B", "g", "h", "v", "w",
        "u", "x", "C", "D", "E", "F", nullptr};
    for (int i = 0; fNotchCandidates[i] &&
         g_rotProbeCount < kMaxRotCandidates; i++) {
      jfieldID fid_ = env->GetFieldID(g_jc.entityCls,
                                      fNotchCandidates[i], "F");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (!fid_) continue;
      bool dup = false;
      for (int k = 0; k < g_rotProbeCount; k++)
        if (g_rotProbes[k].id == fid_) { dup = true; break; }
      if (!dup) g_rotProbes[g_rotProbeCount++].id = fid_;
    }
    debugLog("ensureJni: rotation probes=%d", g_rotProbeCount);
    g_jc.f_onGround =
        fid(g_jc.entityCls, "onGround", "field_70122_E", "F", "Z");

    g_jc.f_swingProgress =
        fid(g_jc.entityCls, "swingProgress", "field_70733_aJ", "aH", "F");
    if (!g_jc.f_swingProgress) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_swingProgress =
          fid(g_jc.entityCls, "swingProgress", "field_70733_aJ", "aI", "F");
    }
    if (!g_jc.f_swingProgress) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_swingProgress =
          fid(g_jc.entityCls, "swingProgress", "field_70733_aJ", "aD", "F");
    }
    if (!g_jc.f_swingProgress && g_jc.epCls) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_swingProgress =
          fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "aH", "F");
      if (!g_jc.f_swingProgress)
        g_jc.f_swingProgress =
            fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "aI", "F");
      if (!g_jc.f_swingProgress)
        g_jc.f_swingProgress =
            fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "aD", "F");
      if (!g_jc.f_swingProgress)
        g_jc.f_swingProgress = lc->FindFieldBySignature(g_jc.epCls, "F", false);
    }

    g_jc.m_isSneaking =
        mid(g_jc.entityCls, "isSneaking", "func_70093_af", "aw", "()Z");
    if (!g_jc.m_isSneaking) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isSneaking =
          mid(g_jc.entityCls, "isSneaking", "func_70093_af", "ax", "()Z");
    }
    if (!g_jc.m_isSneaking) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isSneaking =
          mid(g_jc.entityCls, "isSneaking", "func_70093_af", "av", "()Z");
    }

    g_jc.m_isSprinting =
        mid(g_jc.entityCls, "isSprinting", "func_70051_ag", "aR", "()Z");
    if (!g_jc.m_isSprinting) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isSprinting =
          mid(g_jc.entityCls, "isSprinting", "func_70051_ag", "ax", "()Z");
    }
    if (!g_jc.m_isSprinting) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isSprinting =
          mid(g_jc.entityCls, "isSprinting", "func_70051_ag", "ay", "()Z");
    }

    g_jc.m_isRiding =
        mid(g_jc.entityCls, "isRiding", "func_70026_G", "au", "()Z");
    if (!g_jc.m_isRiding) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isRiding =
          mid(g_jc.entityCls, "isRiding", "func_70026_G", "av", "()Z");
    }

    const char *eidNames[] = {
        "getEntityId", "func_145782_y",
        "S", "F", "aa", "ab", "ac", "ad", "ae", "af", "ag", nullptr};
    for (int i = 0; eidNames[i] && !g_jc.m_getEntityId; i++) {
      g_jc.m_getEntityId =
          env->GetMethodID(g_jc.entityCls, eidNames[i], "()I");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }

    const char *nameMethods[] = {
        "getName", "func_70005_c_",
        "h_", "e_", "f_", "g_", "i_", "j_", "k_", nullptr};
    for (int i = 0; nameMethods[i] && !g_jc.m_getName; i++) {
      g_jc.m_getName = env->GetMethodID(g_jc.entityCls, nameMethods[i],
                                        "()Ljava/lang/String;");
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
  }
  if (env->ExceptionCheck())
    env->ExceptionClear();

  jclass ep = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
  if (!ep)
    ep = tryFind("wn");
  if (!ep)
    ep = tryFind("ahd");
  if (!ep)
    ep = tryFind("xe");
  if (!ep && g_jc.f_theMc && g_jc.f_thePlayer) {
    jobject mcObj = env->GetStaticObjectField(g_jc.mcCls, g_jc.f_theMc);
    if (mcObj) {
      jobject player = env->GetObjectField(mcObj, g_jc.f_thePlayer);
      if (player) {
        jclass playerCls = env->GetObjectClass(player);
        if (playerCls) {
          jclass superCls = env->GetSuperclass(playerCls);
          if (superCls) {
            jclass superSuper = env->GetSuperclass(superCls);
            if (superSuper) {
              ep = superSuper;
            } else {
              ep = superCls;
            }
          } else {
            ep = playerCls;
          }
        }
        env->DeleteLocalRef(player);
      }
      env->DeleteLocalRef(mcObj);
    }
  }
  if (env->ExceptionCheck())
    env->ExceptionClear();

  if (ep) {
    g_jc.epCls = (jclass)env->NewGlobalRef(ep);

    const char *siCandidates[] = {
        "ar", "aP", "i", "isSwingInProgress", "field_82175_bq",
        "ap", "aY", nullptr};
    for (int i = 0; siCandidates[i] &&
         g_jc.swingCandidateCount < g_jc.kMaxSwingCandidates; i++) {
      jfieldID f = env->GetFieldID(g_jc.epCls, siCandidates[i], "Z");
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (f) {
        bool dup = false;
        for (int k = 0; k < g_jc.swingCandidateCount; k++)
          if (g_jc.swingCandidates[k] == f) { dup = true; break; }
        if (!dup) {
          g_jc.swingCandidates[g_jc.swingCandidateCount++] = f;
          if (!g_jc.f_isSwingInProgress) g_jc.f_isSwingInProgress = f;
        }
      }
    }
    debugLog("ensureJni: swing Z candidates=%d (resolved on epCls)",
             g_jc.swingCandidateCount);

    g_jc.m_isBlocking =
        mid(g_jc.epCls, "isBlocking", "func_71041_bz", "bD", "()Z");
    if (!g_jc.m_isBlocking) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isBlocking =
          mid(g_jc.epCls, "isBlocking", "func_71041_bz", "br", "()Z");
    }
    if (!g_jc.m_isBlocking) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isBlocking =
          mid(g_jc.epCls, "isBlocking", "func_71041_bz", "bw", "()Z");
    }
    if (!g_jc.m_isBlocking) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isBlocking =
          mid(g_jc.epCls, "isBlocking", "func_71041_bz", "bs", "()Z");
    }

    g_jc.m_isUsingItem =
        mid(g_jc.epCls, "isUsingItem", "func_71039_bw", "bJ", "()Z");
    if (!g_jc.m_isUsingItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isUsingItem =
          mid(g_jc.epCls, "isUsingItem", "func_71039_bw", "bv", "()Z");
    }

    g_jc.m_getHeldItem = mid(g_jc.epCls, "getHeldItem", "func_70694_bm", "bA",
                             "()Lnet/minecraft/item/ItemStack;");

    g_jc.f_isSwingInProgress = fid(g_jc.epCls, "isSwingInProgress", "field_82175_bq", "ar", "Z");
    if (!g_jc.f_isSwingInProgress) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_jc.f_isSwingInProgress = fid(g_jc.epCls, "isSwingInProgress", "field_82175_bq", "aP", "Z");
    }
    if (!g_jc.f_isSwingInProgress) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_jc.f_isSwingInProgress = fid(g_jc.epCls, "isSwingInProgress", "field_82175_bq", "ap", "Z");
    }

    g_jc.f_swingProgress =
        fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "aH", "F");
    if (!g_jc.f_swingProgress) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      g_jc.f_swingProgress =
          fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "at", "F");
    }
    if (!g_jc.f_swingProgress) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      g_jc.f_swingProgress =
          fid(g_jc.epCls, "swingProgress", "field_70733_aJ", "aI", "F");
    }

    if (lc->jvmti && g_jc.epCls) {
      jclass cur = g_jc.epCls;
      int depth = 0;
      while (cur && depth < 3) {
        jint fcount = 0;
        jfieldID *fields = nullptr;
        if (lc->jvmti->GetClassFields(cur, &fcount, &fields) == JVMTI_ERROR_NONE) {
          for (int i = 0; i < fcount && g_swingProbeCount < kMaxSwingProbes; i++) {
            char *fSig = nullptr;
            lc->jvmti->GetFieldName(cur, fields[i], nullptr, &fSig, nullptr);
            if (fSig && strcmp(fSig, "F") == 0) {
              g_swingProbes[g_swingProbeCount++].id = fields[i];
            }
            if (fSig) lc->jvmti->Deallocate((unsigned char *)fSig);
          }
          if (fields) lc->jvmti->Deallocate((unsigned char *)fields);
        }
        jclass next = env->GetSuperclass(cur);
        if (cur != g_jc.epCls) env->DeleteLocalRef(cur);
        cur = next; depth++;
      }
      if (cur && cur != g_jc.epCls) env->DeleteLocalRef(cur);
      debugLog("ensureJni: swing probes=%d", g_swingProbeCount);
    }

    if (lc->jvmti && g_jc.epCls) {
      jclass cur = g_jc.epCls;
      int depth = 0;
      while (cur && depth < 4 && g_boolProbeCount < kMaxBoolProbes) {
        jint fcount = 0; jfieldID *fields = nullptr;
        if (lc->jvmti->GetClassFields(cur, &fcount, &fields) == JVMTI_ERROR_NONE) {
          for (int i = 0; i < fcount && g_boolProbeCount < kMaxBoolProbes; i++) {
            char *fName = nullptr, *fSig = nullptr;
            lc->jvmti->GetFieldName(cur, fields[i], &fName, &fSig, nullptr);
            if (fSig && strcmp(fSig, "Z") == 0 && fName) {
              jfieldID fid = env->GetFieldID(g_jc.epCls, fName, "Z");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (fid) {
                bool dup = false;
                for (int k = 0; k < g_boolProbeCount; k++)
                  if (g_boolProbes[k].id == fid) { dup = true; break; }
                if (!dup) {
                  g_boolProbes[g_boolProbeCount].id = fid;
                  strncpy_s(g_boolProbes[g_boolProbeCount].name,
                            sizeof(g_boolProbes[g_boolProbeCount].name),
                            fName, _TRUNCATE);
                  g_boolProbeCount++;
                }
              }
            }
            if (fName) lc->jvmti->Deallocate((unsigned char *)fName);
            if (fSig)  lc->jvmti->Deallocate((unsigned char *)fSig);
          }
          if (fields) lc->jvmti->Deallocate((unsigned char *)fields);
        }
        jclass next = env->GetSuperclass(cur);
        if (cur != g_jc.epCls) env->DeleteLocalRef(cur);
        cur = next; depth++;
      }
      if (cur && cur != g_jc.epCls) env->DeleteLocalRef(cur);
      mapLog("boolProbes: collected %d Z fields on EntityPlayer hierarchy",
             g_boolProbeCount);
    }

    if (lc->jvmti && g_jc.epCls) {
      jclass cur = g_jc.epCls;
      int depth = 0;
      while (cur && depth < 4 && g_methodProbeCount < kMaxMethodProbes) {
        jint mcount = 0; jmethodID *methods = nullptr;
        if (lc->jvmti->GetClassMethods(cur, &mcount, &methods) ==
            JVMTI_ERROR_NONE) {
          for (int i = 0; i < mcount && g_methodProbeCount < kMaxMethodProbes; i++) {
            char *mName = nullptr, *mSig = nullptr;
            lc->jvmti->GetMethodName(methods[i], &mName, &mSig, nullptr);
            if (mName && mSig && strcmp(mSig, "()Z") == 0) {
              jmethodID mid = env->GetMethodID(g_jc.epCls, mName, "()Z");
              if (env->ExceptionCheck()) env->ExceptionClear();
              if (mid) {
                bool dup = false;
                for (int k = 0; k < g_methodProbeCount; k++)
                  if (g_methodProbes[k].id == mid) { dup = true; break; }
                if (!dup) {
                  g_methodProbes[g_methodProbeCount].id = mid;
                  strncpy_s(g_methodProbes[g_methodProbeCount].name,
                            sizeof(g_methodProbes[g_methodProbeCount].name),
                            mName, _TRUNCATE);
                  g_methodProbeCount++;
                }
              }
            }
            if (mName) lc->jvmti->Deallocate((unsigned char *)mName);
            if (mSig)  lc->jvmti->Deallocate((unsigned char *)mSig);
          }
          if (methods) lc->jvmti->Deallocate((unsigned char *)methods);
        }
        jclass next = env->GetSuperclass(cur);
        if (cur != g_jc.epCls) env->DeleteLocalRef(cur);
        cur = next; depth++;
      }
      if (cur && cur != g_jc.epCls) env->DeleteLocalRef(cur);
      mapLog("methodProbes: collected %d ()Z methods on EntityPlayer hierarchy",
             g_methodProbeCount);
    }

    g_jc.f_swingProgressInt = fid(g_jc.epCls, "swingProgressInt", "field_70738_aO", "as", "I");
    if (!g_jc.f_swingProgressInt) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_jc.f_swingProgressInt = fid(g_jc.epCls, "swingProgressInt", "field_70738_aO", "au", "I");
    }

    g_jc.m_isSneaking =
        mid(g_jc.epCls, "isSneaking", "func_70093_af", "aw", "()Z");
    g_jc.m_isSprinting =
        mid(g_jc.epCls, "isSprinting", "func_70051_ag", "aR", "()Z");

    if (!g_jc.m_getEntityId) {
      const char *eidNames[] = {
          "getEntityId", "func_145782_y",
          "S", "F", "aa", "ab", "ac", "ad", "ae", "af", "ag", nullptr};
      for (int i = 0; eidNames[i] && !g_jc.m_getEntityId; i++) {
        g_jc.m_getEntityId =
            env->GetMethodID(g_jc.epCls, eidNames[i], "()I");
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
    if (!g_jc.m_getName) {
      const char *nameMethods[] = {
          "getName", "func_70005_c_",
          "h_", "e_", "f_", "g_", "i_", "j_", "k_", nullptr};
      for (int i = 0; nameMethods[i] && !g_jc.m_getName; i++) {
        g_jc.m_getName = env->GetMethodID(g_jc.epCls, nameMethods[i],
                                          "()Ljava/lang/String;");
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
    g_jc.f_posX = fid(g_jc.epCls, "posX", "field_70165_t", "p", "D");
    g_jc.f_posY = fid(g_jc.epCls, "posY", "field_70163_u", "q", "D");
    g_jc.f_posZ = fid(g_jc.epCls, "posZ", "field_70161_v", "r", "D");
    g_jc.f_yaw = fid(g_jc.epCls, "rotationYaw", "field_70177_z", "y", "F");
    g_jc.f_pitch = fid(g_jc.epCls, "rotationPitch", "field_70125_A", "z", "F");
    g_jc.f_onGround = fid(g_jc.epCls, "onGround", "field_70122_E", "C", "Z");
    if (lc->jvmti && g_jc.epCls) {
      jclass cur = g_jc.epCls;
      int depth = 0;
      jfieldID deepestOnGround = nullptr;
      jclass deepestClass = nullptr;
      std::string deepestName = "";
      while (cur && depth < 6) {
        jint fcount = 0; jfieldID *fields = nullptr;
        if (lc->jvmti->GetClassFields(cur, &fcount, &fields) == JVMTI_ERROR_NONE) {
          for (int i = 0; i < fcount; i++) {
            char *fName = nullptr, *fSig = nullptr;
            lc->jvmti->GetFieldName(cur, fields[i], &fName, &fSig, nullptr);
            if (fName && fSig && strcmp(fSig, "Z") == 0 &&
                (strcmp(fName, "onGround") == 0 || strcmp(fName, "C") == 0)) {
              deepestOnGround = fields[i];
              deepestName = fName;
              if (deepestClass && deepestClass != g_jc.epCls)
                env->DeleteLocalRef(deepestClass);
              deepestClass = (jclass)env->NewLocalRef(cur);
            }
            if (fName) lc->jvmti->Deallocate((unsigned char *)fName);
            if (fSig)  lc->jvmti->Deallocate((unsigned char *)fSig);
          }
          if (fields) lc->jvmti->Deallocate((unsigned char *)fields);
        }
        jclass next = env->GetSuperclass(cur);
        if (cur != g_jc.epCls) env->DeleteLocalRef(cur);
        cur = next; depth++;
      }
      if (cur && cur != g_jc.epCls) env->DeleteLocalRef(cur);
      if (deepestOnGround && deepestClass && !deepestName.empty()) {
        jfieldID idFromDeepest = env->GetFieldID(deepestClass, deepestName.c_str(), "Z");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (idFromDeepest && idFromDeepest != g_jc.f_onGround) {
          mapLog("f_onGround SELF-HEAL: replacing shadowed %p with "
                 "parent-class %p (name: %s)", (void*)g_jc.f_onGround,
                 (void*)idFromDeepest, deepestName.c_str());
          g_jc.f_onGround = idFromDeepest;
        }
        env->DeleteLocalRef(deepestClass);
      }
    }

    g_jc.m_getHealth =
        mid(g_jc.epCls, "getHealth", "func_110143_aJ", "bn", "()F");
    g_jc.f_hurtTime = fid(g_jc.epCls, "hurtTime", "field_70737_aN", "aW", "I");
    g_jc.f_ticksExisted = fid(g_jc.entityCls, "ticksExisted", "field_70173_aa", "W", "I");
    
    g_jc.f_isSneaking = fid(g_jc.epCls, "isSneaking", "field_70093_af", "bP", "Z");
    if (!g_jc.f_isSneaking) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        jclass elpCls = env->FindClass("net/minecraft/entity/EntityLivingBase");
        if (elpCls) {
            g_jc.f_isSneaking = env->GetFieldID(elpCls, "bP", "Z");
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(elpCls);
        }
    }
    if (!g_jc.f_isSneaking) {
        g_jc.f_isSneaking = fid(g_jc.entityCls, "isSneaking", "field_70093_af", "bP", "Z");
    }
    if (!g_jc.f_isSneaking) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_jc.f_isSneaking = fid(g_jc.epCls, "isSneaking", "field_70093_af", "aw", "Z");
    }
    if (!g_jc.f_isSneaking) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        static const char *lunarSneakNames[] = {
            "aB", "bN", "bO", "bQ", "aA", "ax", "ay", nullptr};
        for (int i = 0; lunarSneakNames[i] && !g_jc.f_isSneaking; i++) {
            g_jc.f_isSneaking =
                env->GetFieldID(g_jc.epCls, lunarSneakNames[i], "Z");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        if (!g_jc.f_isSneaking && g_jc.entityCls) {
            for (int i = 0; lunarSneakNames[i] && !g_jc.f_isSneaking; i++) {
                g_jc.f_isSneaking =
                    env->GetFieldID(g_jc.entityCls, lunarSneakNames[i], "Z");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
        }
    }

    g_jc.f_dataWatcher = fid(g_jc.entityCls, "dataWatcher", "field_70180_af", "L", "Lnet/minecraft/entity/DataWatcher;");
    if (!g_jc.f_dataWatcher) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_jc.f_dataWatcher = fid(g_jc.entityCls, "dataWatcher", "field_70180_af", "L", "Lnf;");
    }
    jclass dwCls = lc->GetClass("net.minecraft.entity.DataWatcher");
    if (dwCls) {
        g_jc.m_getWatchableObjectByte = env->GetMethodID(dwCls, "getWatchableObjectByte", "(I)B");
        if (!g_jc.m_getWatchableObjectByte) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            g_jc.m_getWatchableObjectByte = env->GetMethodID(dwCls, "func_75683_a", "(I)B");
        }
        if (!g_jc.m_getWatchableObjectByte) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            g_jc.m_getWatchableObjectByte = env->GetMethodID(dwCls, "a", "(I)B");
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    if ((!g_jc.f_dataWatcher || !g_jc.m_getWatchableObjectByte) && lc->jvmti
        && g_jc.entityCls) {
      jclass cur = g_jc.entityCls;
      env->NewGlobalRef(cur); // hold a ref so we can walk safely (not needed, just safety)
      int depth = 0;
      while (cur && depth < 4 && (!g_jc.f_dataWatcher || !g_jc.m_getWatchableObjectByte)) {
        jint fcount = 0; jfieldID *fields = nullptr;
        if (lc->jvmti->GetClassFields(cur, &fcount, &fields) == JVMTI_ERROR_NONE) {
          for (int i = 0; i < fcount && (!g_jc.f_dataWatcher || !g_jc.m_getWatchableObjectByte); i++) {
            char *fName = nullptr, *fSig = nullptr;
            lc->jvmti->GetFieldName(cur, fields[i], &fName, &fSig, nullptr);
            if (fSig && fSig[0] == 'L') {
              size_t slen = strlen(fSig);
              bool looksLikeDw =
                  (slen > 2 && slen < 60) && // short obfuscated or fully qualified
                  (strstr(fSig, "DataWatcher") != nullptr ||
                   strstr(fSig, "Watcher") != nullptr ||
                   // obfuscated short class name "Lnf;" or "Lnu;" etc (length <= 5)
                   slen <= 5);
              if (looksLikeDw && fName) {
                std::string internal(fSig + 1, slen - 2);
                jclass cand = env->FindClass(internal.c_str());
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (cand) {
                  jmethodID m = env->GetMethodID(cand, "getWatchableObjectByte", "(I)B");
                  if (env->ExceptionCheck()) env->ExceptionClear();
                  if (!m) { m = env->GetMethodID(cand, "func_75683_a", "(I)B"); if (env->ExceptionCheck()) env->ExceptionClear(); }
                  if (!m) { m = env->GetMethodID(cand, "a", "(I)B");             if (env->ExceptionCheck()) env->ExceptionClear(); }
                  if (m) {
                    jfieldID dwFid = env->GetFieldID(g_jc.entityCls, fName, fSig);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (dwFid) {
                      g_jc.f_dataWatcher = dwFid;
                      g_jc.m_getWatchableObjectByte = m;
                      debugLog("ensureJni: DataWatcher self-heal — field='%s' sig='%s' (I)B method resolved", fName, fSig);
                    }
                  }
                  env->DeleteLocalRef(cand);
                }
              }
            }
            if (fName) lc->jvmti->Deallocate((unsigned char *)fName);
            if (fSig)  lc->jvmti->Deallocate((unsigned char *)fSig);
          }
          if (fields) lc->jvmti->Deallocate((unsigned char *)fields);
        }
        jclass next = env->GetSuperclass(cur);
        if (cur != g_jc.entityCls) env->DeleteLocalRef(cur);
        cur = next;
        depth++;
      }
      if (cur && cur != g_jc.entityCls) env->DeleteLocalRef(cur);
    }
    debugLog("ensureJni: DataWatcher status f_dataWatcher=%p m_getWatchableObjectByte=%p",
             (void*)g_jc.f_dataWatcher, (void*)g_jc.m_getWatchableObjectByte);
    if (!g_jc.f_hurtTime) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_hurtTime =
          fid(g_jc.epCls, "hurtTime", "field_70737_aN", "au", "I");
    }

    if (!g_jc.m_isUsingItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_isUsingItem =
          mid(g_jc.epCls, "isUsingItem", "func_71039_bw", "bw", "()Z");
    }

    g_jc.m_getHeldItem = mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bZ",
                             "()Lnet/minecraft/item/ItemStack;");
    if (!g_jc.m_getHeldItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_getHeldItem = mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bA",
                               "()Lnet/minecraft/item/ItemStack;");
    }
    if (!g_jc.m_getHeldItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_getHeldItem = mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bD",
                               "()Lnet/minecraft/item/ItemStack;");
    }
    if (!g_jc.m_getHeldItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_getHeldItem =
          mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bZ", "()Lzx;");
    }
    if (!g_jc.m_getHeldItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_getHeldItem =
          mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bA", "()Lzx;");
    }
    if (!g_jc.m_getHeldItem) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.m_getHeldItem =
          mid(g_jc.epCls, "getHeldItem", "func_71045_bC", "bD", "()Lzx;");
    }

    g_jc.f_inventory =
        fid(g_jc.epCls, "inventory", "field_71071_by", "bi",
            "Lnet/minecraft/entity/player/InventoryPlayer;");
    if (!g_jc.f_inventory) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      g_jc.f_inventory =
          fid(g_jc.epCls, "inventory", "field_71071_by", "bi", "Lbgz;");
    }
    if (!g_jc.f_inventory) {
      if (env->ExceptionCheck()) env->ExceptionClear();
      g_jc.f_inventory =
          fid(g_jc.epCls, "inventory", "field_71071_by", "bi", "Lwm;");
    }

    if (g_jc.f_inventory) {
      jobject mcObj = env->GetStaticObjectField(g_jc.mcCls, g_jc.f_theMc);
      if (mcObj) {
        jobject player = env->GetObjectField(mcObj, g_jc.f_thePlayer);
        if (player) {
          jobject inv = env->GetObjectField(player, g_jc.f_inventory);
          if (inv) {
            jclass invCls = env->GetObjectClass(inv);
            g_jc.m_getCurrentItem =
                mid(invCls, "getCurrentItem", "func_70448_g", "h",
                    "()Lnet/minecraft/item/ItemStack;");
            if (!g_jc.m_getCurrentItem) {
              if (env->ExceptionCheck()) env->ExceptionClear();
              g_jc.m_getCurrentItem =
                  mid(invCls, "getCurrentItem", "func_70448_g", "h", "()Lzx;");
            }
            env->DeleteLocalRef(invCls);
            env->DeleteLocalRef(inv);
          }
          env->DeleteLocalRef(player);
        }
        env->DeleteLocalRef(mcObj);
      }
    }

    g_jc.f_hurtTime = fid(g_jc.epCls, "hurtTime", "field_70737_aN", "aW", "I");
    if (!g_jc.f_hurtTime) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_hurtTime =
          fid(g_jc.epCls, "hurtTime", "field_70737_aN", "au", "I");
    }
    if (!g_jc.f_hurtTime) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_hurtTime =
          fid(g_jc.epCls, "hurtTime", "field_70737_aN", "at", "I");
    }
    if (!g_jc.f_hurtTime) {
      if (env->ExceptionCheck())
        env->ExceptionClear();
      g_jc.f_hurtTime =
          fid(g_jc.epCls, "hurtTime", "field_70737_aN", "aB", "I");
    }
  }
  if (env->ExceptionCheck())
    env->ExceptionClear();

  jclass itemStack = lc->GetClass("net.minecraft.item.ItemStack");
  if (!itemStack)
    itemStack = tryFind("zx");
  if (itemStack) {
    g_jc.itemStackCls = (jclass)env->NewGlobalRef(itemStack);
    const char *itemSigs[] = {
        "()Lnet/minecraft/item/Item;", "()Ladb;", "()Lzw;",
        "()Lzv;", "()Lzu;", "()Lzx;", nullptr};
    const char *itemNames[] = {"getItem", "func_77973_b",
                               "b", "c", "d", nullptr};
    for (int s = 0; itemSigs[s] && !g_jc.m_isGetItem; s++) {
      for (int n = 0; itemNames[n] && !g_jc.m_isGetItem; n++) {
        g_jc.m_isGetItem =
            env->GetMethodID(g_jc.itemStackCls, itemNames[n], itemSigs[s]);
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
  }
  if (env->ExceptionCheck())
    env->ExceptionClear();

  jclass item = lc->GetClass("net.minecraft.item.Item");
  if (!item) item = tryFind("adb");
  if (!item) item = tryFind("zw");
  if (item) {
    g_jc.itemCls = (jclass)env->NewGlobalRef(item);
    const char *paramSigs[] = {
        "(Lnet/minecraft/item/Item;)I", "(Ladb;)I", "(Lzw;)I",
        "(Lzv;)I", nullptr};
    const char *gidNames[] = {"getIdFromItem", "func_150891_b",
                              "b", "c", "a", nullptr};
    for (int s = 0; paramSigs[s] && !g_jc.m_itemGetIdStatic; s++) {
      for (int n = 0; gidNames[n] && !g_jc.m_itemGetIdStatic; n++) {
        g_jc.m_itemGetIdStatic = env->GetStaticMethodID(
            g_jc.itemCls, gidNames[n], paramSigs[s]);
        if (env->ExceptionCheck()) env->ExceptionClear();
      }
    }
  }

  if (env->ExceptionCheck())
    env->ExceptionClear();
  g_jc.ready = true;

  debugLog(
      "ensureJni: ready=1 mcCls=%p worldCls=%p listCls=%p entityCls=%p "
      "epCls=%p f_theMc=%p f_theWorld=%p f_thePlayer=%p "
      "f_playerEntities=%p m_listSize=%p m_listGet=%p m_getEntityId=%p "
      "m_getName=%p f_posX=%p f_yaw=%p f_pitch=%p f_swingProgress=%p m_isBlocking=%p "
      "m_isUsingItem=%p m_getHeldItem=%p m_isGetItem=%p m_itemGetIdStatic=%p",
      (void *)g_jc.mcCls, (void *)g_jc.worldCls, (void *)g_jc.listCls,
      (void *)g_jc.entityCls, (void *)g_jc.epCls, (void *)g_jc.f_theMc,
      (void *)g_jc.f_theWorld, (void *)g_jc.f_thePlayer,
      (void *)g_jc.f_playerEntities, (void *)g_jc.m_listSize,
      (void *)g_jc.m_listGet, (void *)g_jc.m_getEntityId,
      (void *)g_jc.m_getName, (void *)g_jc.f_posX, (void *)g_jc.f_yaw,
      (void *)g_jc.f_pitch, (void *)g_jc.f_swingProgress,
      (void *)g_jc.m_isBlocking, (void *)g_jc.m_isUsingItem,
      (void *)g_jc.m_getHeldItem, (void *)g_jc.m_isGetItem,
      (void *)g_jc.m_itemGetIdStatic);

  mapLog("===== ensureJni Eagle-critical snapshot =====");
  mapLog("Classes: entityCls=%p epCls=%p", (void*)g_jc.entityCls,
         (void*)g_jc.epCls);
  mapLog("Rotation: f_yaw=%p f_pitch=%p (rotProbeCount=%d)",
         (void*)g_jc.f_yaw, (void*)g_jc.f_pitch, g_rotProbeCount);
  mapLog("State: f_onGround=%p f_isSneaking=%p f_isSwingInProgress=%p "
         "f_swingProgress=%p (swingProbeCount=%d)",
         (void*)g_jc.f_onGround, (void*)g_jc.f_isSneaking,
         (void*)g_jc.f_isSwingInProgress, (void*)g_jc.f_swingProgress,
         g_swingProbeCount);
  mapLog("Methods: m_isSneaking=%p m_isSprinting=%p m_isBlocking=%p "
         "m_getHeldItem=%p", (void*)g_jc.m_isSneaking,
         (void*)g_jc.m_isSprinting, (void*)g_jc.m_isBlocking,
         (void*)g_jc.m_getHeldItem);
  mapLog("DataWatcher: f_dataWatcher=%p m_getWatchableObjectByte=%p",
         (void*)g_jc.f_dataWatcher, (void*)g_jc.m_getWatchableObjectByte);
  bool sneakSrc = g_jc.f_isSneaking || g_jc.m_isSneaking ||
                  (g_jc.f_dataWatcher && g_jc.m_getWatchableObjectByte);
  bool pitchSrc = g_jc.f_pitch || g_rotProbeCount > 0;
  bool swingSrc = g_jc.f_isSwingInProgress || g_swingProbeCount > 0;
  mapLog("EAGLE READINESS: sneak-source=%s pitch-source=%s "
         "swing-source=%s heldItem=%s",
         sneakSrc ? "YES" : "NO",
         pitchSrc ? "YES" : "NO",
         swingSrc ? "YES" : "NO",
         g_jc.m_getHeldItem ? "YES" : "NO");
  mapLog("===== end Eagle snapshot =====");

  abLog("MAPPING_SUMMARY:");
  abLog("  m_isBlocking            = %p %s", (void *)g_jc.m_isBlocking,
        g_jc.m_isBlocking ? "OK" : "MISSING (local isBlocking unread)");
  abLog("  m_isUsingItem           = %p %s", (void *)g_jc.m_isUsingItem,
        g_jc.m_isUsingItem ? "OK" : "MISSING (local isUsingItem unread)");
  abLog("  f_isSwingInProgress     = %p %s", (void *)g_jc.f_isSwingInProgress,
        g_jc.f_isSwingInProgress ? "OK" : "MISSING (swing edge detection broken)");
  abLog("  f_swingProgress         = %p %s", (void *)g_jc.f_swingProgress,
        g_jc.f_swingProgress ? "OK" : "(optional fallback)");
  abLog("  f_dataWatcher           = %p %s", (void *)g_jc.f_dataWatcher,
        g_jc.f_dataWatcher ? "OK" : "MISSING (remote isBlocking/isUsingItem broken)");
  abLog("  m_getWatchableObjectByte= %p %s", (void *)g_jc.m_getWatchableObjectByte,
        g_jc.m_getWatchableObjectByte ? "OK" : "MISSING (remote dwFallback broken)");
  abLog("  m_getHeldItem           = %p %s", (void *)g_jc.m_getHeldItem,
        g_jc.m_getHeldItem ? "OK" : "MISSING (heldItemId always 0)");
  abLog("  m_isGetItem             = %p %s", (void *)g_jc.m_isGetItem,
        g_jc.m_isGetItem ? "OK" : "MISSING (heldItemId always 0)");
  abLog("  m_itemGetIdStatic       = %p %s", (void *)g_jc.m_itemGetIdStatic,
        g_jc.m_itemGetIdStatic ? "OK" : "MISSING (heldItemId always 0)");
  bool localOK = g_jc.m_isBlocking && g_jc.m_isUsingItem &&
                 g_jc.f_isSwingInProgress && g_jc.m_getHeldItem;
  bool remoteOK = g_jc.f_dataWatcher && g_jc.m_getWatchableObjectByte &&
                  g_jc.f_isSwingInProgress && g_jc.m_getHeldItem;
  abLog("VERDICT: local-player check %s, remote-player check %s",
        localOK ? "READY" : "BROKEN", remoteOK ? "READY" : "BROKEN");
}

static std::mutex g_playersMutex;
static std::unordered_map<int /*entityId*/, PlayerData> g_players;
static std::mutex g_flaggedMutex;
static std::unordered_set<std::string> g_flaggedPlayers;
static std::atomic<long> g_currentTick{0};
static std::atomic<bool> g_started{false};
static std::vector<std::unique_ptr<Check>> g_checks;

void flag(PlayerData &p, Check *check, const std::string &info, double vl) {
  auto &st = p.checks[check->name()];

  ULONGLONG now = GetTickCount64();
  if (st.lastFlagMs > 0) {
    double elapsedSec = (double)(now - st.lastFlagMs) / 1000.0;
    double decay = elapsedSec * 1.5;
    if (decay > 0 && st.vl > 0) {
      st.vl = std::max(0.0, st.vl - decay);
    }
  }
  st.lastFlagMs = now;

  int threshold = Config::getAnticheatVl();
  if (threshold <= 0)
    threshold = 5;

  double prevVl = st.vl;
  st.vl += vl;
  double vlCap = (double)threshold * 2.0;
  if (st.vl > vlCap) st.vl = vlCap;
  debugLog("flag: %s '%s' +%.2f (vl %.2f -> %.2f) info='%s'", check->name(),
           p.name.c_str(), vl, prevVl, st.vl, info.c_str());
  long cooldownMs = (long)Config::getAnticheatCooldownSec() * 1000L;
  if (cooldownMs <= 0)
    cooldownMs = 4000;

  if (st.vl < threshold) {
    debugLog("flag: %s '%s' below threshold (%d), no alert", check->name(),
             p.name.c_str(), threshold);
    return;
  }

  bool bypassCooldown = (strcmp(check->name(), "Eagle") == 0);
  if (!bypassCooldown &&
      (long)(now - st.lastAlertMs) < cooldownMs) {
    debugLog("flag: %s '%s' in cooldown (%dms remaining)", check->name(),
             p.name.c_str(), (int)(cooldownMs - (long)(now - st.lastAlertMs)));
    return;
  }
  st.lastAlertMs = now;
  st.vl = (double)threshold * 0.5;
  p.isFlagged = true;
  {
    std::string target;
    for (char c : p.name) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_') {
        target += (char)std::tolower((unsigned char)c);
      }
    }
    if (!target.empty()) {
      std::lock_guard<std::mutex> flagLock(g_flaggedMutex);
      g_flaggedPlayers.insert(target);
      debugLog("flaggedSet: inserted '%s' (from p.name='%s'), size=%d",
               target.c_str(), p.name.c_str(), (int)g_flaggedPlayers.size());
    }
  }
  debugLog("flag: %s '%s' ALERT FIRED (vl=%.2f threshold=%d)", check->name(),
           p.name.c_str(), st.vl, threshold);

  char vlBuf[16];
  sprintf_s(vlBuf, "%.1f", st.vl);

  std::string msg = ChatSDK::formatPrefix();
  msg += "\xC2\xA7"
         "8[\xC2\xA7"
         "cAC\xC2\xA7"
         "8] ";
  msg += "\xC2\xA7"
         "7" +
         p.name + " ";
  msg += "\xC2\xA7"
         "ffailed \xC2\xA7"
         "c" +
         std::string(check->name());
  if (!info.empty()) {
    msg += " \xC2\xA7"
           "7(" +
           info + ")";
  }
  msg += " \xC2\xA7"
         "c[VL: " +
         std::string(vlBuf) + "]";

  if (OVson::isInHypixelGame()) {
    msg += " \xC2\xA7"
           "8[\xC2\xA7"
           "c/wdr " +
           p.name +
           "\xC2\xA7"
           "8]";
  }
  ChatSDK::showClientMessage(msg);
}

static void tickOnce(JNIEnv *env) {
  ensureJni(env);
  if (!g_jc.ready) {
    debugLog("tickOnce: SKIP — JCache not ready (failed=%d)",
             g_jc.failed ? 1 : 0);
    return;
  }
  if (!Config::isAnticheatEnabled()) {
    static int s_offCount = 0;
    if ((s_offCount++ % 100) == 0)
      debugLog("tickOnce: SKIP — anticheat disabled in config");
    return;
  }

  jobject mc = env->GetStaticObjectField(g_jc.mcCls, g_jc.f_theMc);
  if (!mc) {
    debugLog("tickOnce: SKIP — Minecraft.theMinecraft is null");
    return;
  }
  jobject world = env->GetObjectField(mc, g_jc.f_theWorld);
  jobject localPlayer = env->GetObjectField(mc, g_jc.f_thePlayer);
  env->DeleteLocalRef(mc);
  if (!world) {
    if (localPlayer)
      env->DeleteLocalRef(localPlayer);
    static int s_noWorld = 0;
    if ((s_noWorld++ % 50) == 0)
      debugLog("tickOnce: SKIP — theWorld is null (not in a game?)");
    return;
  }

  jobject playerList = env->GetObjectField(world, g_jc.f_playerEntities);
  if (env->ExceptionCheck()) env->ExceptionClear();

  long tick = ++g_currentTick;
  jint count = 0;
  if (playerList) {
    count = env->CallIntMethod(playerList, g_jc.m_listSize);
    if (env->ExceptionCheck()) { env->ExceptionClear(); count = 0; }
  }

  static std::atomic<bool> s_selfHealDone{false};
  if (count == 0 && !s_selfHealDone.load() &&
      g_worldListCandidateCount > 1) {
    s_selfHealDone.store(true);
    for (int i = 0; i < g_worldListCandidateCount; i++) {
      if (g_worldListCandidates[i] == g_jc.f_playerEntities) continue;
      jobject alt = env->GetObjectField(world, g_worldListCandidates[i]);
      if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
      if (!alt) continue;
      jint altCount = env->CallIntMethod(alt, g_jc.m_listSize);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(alt);
        continue;
      }
      if (altCount > 0) {
        debugLog("tickOnce: f_playerEntities self-healed to candidate[%d] "
                 "(size=%d)", i, (int)altCount);
        g_jc.f_playerEntities = g_worldListCandidates[i];
        if (playerList) env->DeleteLocalRef(playerList);
        playerList = alt;
        count = altCount;
        break;
      }
      env->DeleteLocalRef(alt);
    }
  }

  env->DeleteLocalRef(world);
  if (!playerList) {
    if (localPlayer) env->DeleteLocalRef(localPlayer);
    static int s_skipCount = 0;
    if ((s_skipCount++ % 100) == 0)
      debugLog("tickOnce: SKIP — playerList null (count=%d)", (int)count);
    return;
  }
  if ((tick % 50) == 1)
    debugLog("tickOnce: tick=%ld count=%d checks=%d localPlayer=%p", tick,
             (int)count, (int)g_checks.size(), (void *)localPlayer);

  std::lock_guard<std::mutex> lock(g_playersMutex);

  std::unordered_map<int, bool> seen;

  for (jint i = 0; i < count; ++i) {
    jobject ent = env->CallObjectMethod(playerList, g_jc.m_listGet, i);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    if (!ent)
      continue;

    bool isLocal = env->IsSameObject(ent, localPlayer);

    jint eid = env->CallIntMethod(ent, g_jc.m_getEntityId);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      env->DeleteLocalRef(ent);
      continue;
    }
    seen[eid] = true;

    PlayerData &p = g_players[eid];
    p.entityId = (int)eid;
    p.isLocalPlayer = isLocal;
    
    if (g_jc.f_ticksExisted) {
        p.currentTick = env->GetIntField(ent, g_jc.f_ticksExisted);
        if (env->ExceptionCheck()) env->ExceptionClear();
    } else {
        p.currentTick++;
    }
    long tick = p.currentTick;

    if (p.name.empty() || (tick % 60) == 0) {
      jstring jn = (jstring)env->CallObjectMethod(ent, g_jc.m_getName);
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (jn) {
        const char *u = env->GetStringUTFChars(jn, nullptr);
        if (u) {
          std::string raw = u;
          std::string cleaned;
          for (size_t k = 0; k < raw.length();) {
            if ((unsigned char)raw[k] == 0xC2 && k + 1 < raw.length() &&
                (unsigned char)raw[k + 1] == 0xA7) {
              k += 3;
            } else if ((unsigned char)raw[k] == 0xA7) {
              k += 2;
            } else {
              cleaned += raw[k];
              k++;
            }
          }
          p.name = cleaned;
          env->ReleaseStringUTFChars(jn, u);
        }
        env->DeleteLocalRef(jn);
      }
    }

    p.lastPosX = p.posX;
    p.lastPosY = p.posY;
    p.lastPosZ = p.posZ;
    p.wasBlocking = p.isBlocking;
    p.wasUsingItem = p.isUsingItem;
    p.wasSwingInProgress = p.isSwingInProgress;
    p.wasSneaking = p.isSneaking;

    p.posX = env->GetDoubleField(ent, g_jc.f_posX);
    p.posY = env->GetDoubleField(ent, g_jc.f_posY);
    p.posZ = env->GetDoubleField(ent, g_jc.f_posZ);
    p.rotYaw = env->GetFloatField(ent, g_jc.f_yaw);
    p.rotPitch = env->GetFloatField(ent, g_jc.f_pitch);

    {
      bool swingAny = false;
      if (g_jc.f_isSwingInProgress) {
          if (env->GetBooleanField(ent, g_jc.f_isSwingInProgress) == JNI_TRUE) {
              swingAny = true;
          }
      }

      if (!swingAny) {
          for (int sc = 0; sc < g_jc.swingCandidateCount; sc++) {
            jboolean v = env->GetBooleanField(ent, g_jc.swingCandidates[sc]);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (v == JNI_TRUE) { swingAny = true; break; }
          }
      }

      if (!swingAny && g_jc.f_swingProgress) {
        jfloat sp = env->GetFloatField(ent, g_jc.f_swingProgress);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (sp > 0.0f && sp < 1.0f) swingAny = true;
      }
      if (!swingAny) {
          jfieldID fidInt = g_jc.f_swingProgressInt;
          if (!fidInt) {
              static jfieldID f_au = env->GetFieldID(g_jc.epCls, "au", "I");
              if (env->ExceptionCheck()) env->ExceptionClear();
              fidInt = f_au;
          }
          if (fidInt) {
              jint iv = env->GetIntField(ent, fidInt);
              if (env->ExceptionCheck()) env->ExceptionClear();
              else if (iv > 0 && iv < 10) swingAny = true;
          }
      }

      if (!swingAny && isLocal && g_swingProbeCount > 0) {
        for (int i = 0; i < g_swingProbeCount; i++) {
          jfloat v = env->GetFloatField(ent, g_swingProbes[i].id);
          if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
          if (v > 0.0f && v < 1.0f && g_swingProbes[i].lastV == 0.0f) {
            g_swingProbes[i].hits++;
            if (g_swingProbes[i].hits > 3) {
              g_jc.f_swingProgress = g_swingProbes[i].id;
              debugLog("swingHeal: swingProgress FINAL probe[%d]", i);
            }
          }
          g_swingProbes[i].lastV = v;
        }
      }

      if (swingAny) {
          p.lastSwingTick = tick;
      }
      p.isSwingInProgress = (tick - p.lastSwingTick < 15);
    }

    if (g_rotProbeCount > 0) {
      for (int i = 0; i < g_rotProbeCount; i++) {
        float v = env->GetFloatField(ent, g_rotProbes[i].id);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (v < g_rotProbes[i].minV) g_rotProbes[i].minV = v;
        if (v > g_rotProbes[i].maxV) g_rotProbes[i].maxV = v;
        g_rotProbes[i].samples++;
      }
    }
    if (isLocal && g_rotProbeCount > 0) {
      g_rotProbeTicks++;

      static bool s_yawPicked = false;
      if (!s_yawPicked && g_rotProbeTicks >= 20) {
        int bestYaw = -1; float bestRange = -1.0f;
        for (int i = 0; i < g_rotProbeCount; i++) {
          float range = g_rotProbes[i].maxV - g_rotProbes[i].minV;
          if (range > bestRange) { bestRange = range; bestYaw = i; }
        }
        if (bestYaw >= 0 && bestRange > 1.0f) {
          g_jc.f_yaw = g_rotProbes[bestYaw].id;
          debugLog("rotHeal: yaw FINAL probe[%d] range=%.1f [%.1f..%.1f]",
                   bestYaw, bestRange, g_rotProbes[bestYaw].minV,
                   g_rotProbes[bestYaw].maxV);
          s_yawPicked = true;
        }
      }

      static bool s_pitchPicked = false;
      if (s_yawPicked && !s_pitchPicked && g_rotProbeTicks >= 20) {
        int bestPitch = -1; float bestRange = 0.0f;
        for (int i = 0; i < g_rotProbeCount; i++) {
          if (g_rotProbes[i].id == g_jc.f_yaw) continue;
          float range = g_rotProbes[i].maxV - g_rotProbes[i].minV;
          if (range >= 1.0f &&
              g_rotProbes[i].maxV <= 90.0f &&
              g_rotProbes[i].minV >= -90.0f &&
              range > bestRange) {
            bestRange = range; bestPitch = i;
          }
        }
        if (bestPitch >= 0) {
          g_jc.f_pitch = g_rotProbes[bestPitch].id;
          debugLog("rotHeal: pitch FINAL probe[%d] range=%.1f [%.1f..%.1f]",
                   bestPitch, bestRange, g_rotProbes[bestPitch].minV,
                   g_rotProbes[bestPitch].maxV);
          s_pitchPicked = true;
          g_rotHealed = true;
        }
        if ((g_rotProbeTicks % 100) == 0 && !s_pitchPicked) {
          debugLog("rotHeal: still looking for pitch — local player "
                   "hasn't looked up/down enough (tick=%d, probes=%d)",
                   g_rotProbeTicks, g_rotProbeCount);
        }
      }
    }
    p.onGround = env->GetBooleanField(ent, g_jc.f_onGround) == JNI_TRUE;
    p.isSwingInProgress =
        env->GetBooleanField(ent, g_jc.f_isSwingInProgress) == JNI_TRUE;
    p.hurtTime = env->GetIntField(ent, g_jc.f_hurtTime);

    if (g_jc.f_isSneaking) {
      p.isSneaking = env->GetBooleanField(ent, g_jc.f_isSneaking) == JNI_TRUE;
    } else if (g_jc.f_dataWatcher && g_jc.m_getWatchableObjectByte) {
      jobject dw = env->GetObjectField(ent, g_jc.f_dataWatcher);
      if (dw) {
        jbyte b = env->CallByteMethod(dw, g_jc.m_getWatchableObjectByte, 0);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else p.isSneaking = (b & 0x02) != 0;
        if (!isLocal) {
          static std::unordered_map<int, int> s_lastByte;
          auto it = s_lastByte.find(p.entityId);
          int prev = (it != s_lastByte.end()) ? it->second : -1;
          int curB = (int)((unsigned char)b);
          if (prev != curB) {
            mapLog("remoteDW [%s eid=%d]: byte0=0x%02x sneak-bit=%d "
                   "(prev=0x%02x)", p.name.c_str(), p.entityId, curB,
                   (curB & 0x02) ? 1 : 0,
                   prev < 0 ? 0xFF : prev);
            s_lastByte[p.entityId] = curB;
          }
        }
        env->DeleteLocalRef(dw);
      }
    } else if (g_jc.m_isSneaking) {
      p.isSneaking = env->CallBooleanMethod(ent, g_jc.m_isSneaking) == JNI_TRUE;
    }

    /*
    if (isLocal && g_boolProbeCount > 0) {
      for (int i = 0; i < g_boolProbeCount; i++) {
        jboolean v = env->GetBooleanField(ent, g_boolProbes[i].id);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        int cur = (v == JNI_TRUE) ? 1 : 0;
        if (g_boolProbes[i].lastV == -1) {
          g_boolProbes[i].lastV = cur;
        } else if (g_boolProbes[i].lastV != cur) {
          mapLog("boolProbe TOGGLE: field='%s' %d -> %d (tick=%ld)",
                 g_boolProbes[i].name, g_boolProbes[i].lastV, cur,
                 (long)p.currentTick);
          g_boolProbes[i].lastV = cur;
        }
      }
    }
    if (isLocal && g_methodProbeCount > 0) {
      for (int i = 0; i < g_methodProbeCount; i++) {
        jboolean v = env->CallBooleanMethod(ent, g_methodProbes[i].id);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        int cur = (v == JNI_TRUE) ? 1 : 0;
        if (g_methodProbes[i].lastV == -1) {
          g_methodProbes[i].lastV = cur;
        } else if (g_methodProbes[i].lastV != cur) {
          mapLog("methodProbe TOGGLE: method='%s' %d -> %d (tick=%ld)",
                 g_methodProbes[i].name, g_methodProbes[i].lastV, cur,
                 (long)p.currentTick);
          g_methodProbes[i].lastV = cur;
        }
      }
    }
    */

    if (!p.isSneaking && g_jc.f_dataWatcher && g_jc.m_getWatchableObjectByte) {
        jobject dw = env->GetObjectField(ent, g_jc.f_dataWatcher);
        if (dw) {
            jbyte b = env->CallByteMethod(dw, g_jc.m_getWatchableObjectByte, 0);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (b & 0x02) p.isSneaking = true;
            env->DeleteLocalRef(dw);
        }
    }

    {
        static std::unordered_map<void*, long long> s_lastVals;

        static bool mappingsLocked = false;
        if (!mappingsLocked) {
            jclass epCls = env->GetObjectClass(ent);
            auto setIfNull = [&](const char *label, jfieldID &slot,
                                    const char *name, const char *sig) {
                if (slot) {
                    mapLog("lockdown[%s]: already resolved (%p), skipping '%s'", label, (void*)slot, name);
                    return;
                }
                jfieldID id = env->GetFieldID(epCls, name, sig);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (id) {
                    slot = id;
                    mapLog("lockdown[%s]: '%s' RESOLVED id=%p", label, name, (void*)id);
                } else {
                    mapLog("lockdown[%s]: '%s' NOT FOUND", label, name);
                }
            };
            mapLog("===== Lockdown pass start =====");
            setIfNull("f_yaw",        g_jc.f_yaw,        "f",  "F");
            setIfNull("f_pitch",      g_jc.f_pitch,      "g",  "F");
            setIfNull("f_onGround",   g_jc.f_onGround,   "ar", "Z");
            setIfNull("f_isSneaking", g_jc.f_isSneaking, "bP", "Z");
            mapLog("Lockdown FINAL: f_yaw=%p f_pitch=%p f_onGround=%p f_isSneaking=%p",
                   (void*)g_jc.f_yaw, (void*)g_jc.f_pitch,
                   (void*)g_jc.f_onGround, (void*)g_jc.f_isSneaking);
            mapLog("===== Lockdown pass end =====");
            env->DeleteLocalRef(epCls);
            mappingsLocked = true;
            debugLog("[Anticheat] CORE MAPPINGS LOCKED (Fallback pass completed)");
        }

        if (g_jc.f_posX) p.posX = env->GetDoubleField(ent, g_jc.f_posX);
        if (g_jc.f_posY) p.posY = env->GetDoubleField(ent, g_jc.f_posY);
        if (g_jc.f_posZ) p.posZ = env->GetDoubleField(ent, g_jc.f_posZ);
        if (g_jc.f_yaw)   p.rotYaw   = env->GetFloatField(ent, g_jc.f_yaw);
        if (g_jc.f_pitch) p.rotPitch = env->GetFloatField(ent, g_jc.f_pitch);
        if (g_jc.f_onGround) {
            p.onGround = env->GetBooleanField(ent, g_jc.f_onGround) == JNI_TRUE;
            if (p.onGround) p.lastOnGroundTick = tick;
        }
        if (g_jc.f_isSneaking) {
            p.isSneaking = env->GetBooleanField(ent, g_jc.f_isSneaking) == JNI_TRUE;
        }

        p.isBlocking = false;
        p.isUsingItem = false;
        if (p.isLocalPlayer) {
            if (g_jc.m_isBlocking) {
                jboolean v = env->CallBooleanMethod(ent, g_jc.m_isBlocking);
                if (env->ExceptionCheck()) env->ExceptionClear();
                else p.isBlocking = (v == JNI_TRUE);
            }
            if (g_jc.m_isUsingItem) {
                jboolean v = env->CallBooleanMethod(ent, g_jc.m_isUsingItem);
                if (env->ExceptionCheck()) env->ExceptionClear();
                else p.isUsingItem = (v == JNI_TRUE);
            }
        }

        if (!p.isLocalPlayer) {
            static int s_dwLogCount = 0;
            static bool s_dwMissingLogged = false;
            if (!g_jc.f_dataWatcher || !g_jc.m_getWatchableObjectByte) {
                if (!s_dwMissingLogged) {
                    s_dwMissingLogged = true;
                    debugLog("dwFallback MISSING: f_dataWatcher=%p m=%p — remote sneak detection disabled",
                             (void*)g_jc.f_dataWatcher,
                             (void*)g_jc.m_getWatchableObjectByte);
                }
            } else {
                jobject dw = env->GetObjectField(ent, g_jc.f_dataWatcher);
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (dw) {
                    jbyte b = env->CallByteMethod(dw, g_jc.m_getWatchableObjectByte, 0);
                    if (env->ExceptionCheck()) {
                        env->ExceptionClear();
                    } else {
                        p.isSneaking = (b & 0x02) != 0;
                        p.isSprinting = (b & 0x08) != 0;
                        if (b & 0x10) {
                            p.isUsingItem = true;
                            int id = p.heldItemId;
                            bool isSword = (id == 267 || id == 268 ||
                                            id == 272 || id == 276 ||
                                            id == 283);
                            if (isSword) p.isBlocking = true;
                        }
                        if (s_dwLogCount < 5 && (b & 0x02)) {
                            debugLog("dwFallback eid=%d byte0=0x%02x sneak=1", p.entityId, (int)(b & 0xFF));
                            s_dwLogCount++;
                        }
                    }
                    env->DeleteLocalRef(dw);
                }
            }
        }

        if (p.isSneaking && !p.wasSneaking) {
            p.lastCrouchStartTick = tick;
        }
        if (!p.isSneaking && p.wasSneaking) {
            p.lastCrouchEndTick = tick;
            int dur = (int)(p.lastCrouchEndTick - p.lastCrouchStartTick);
            if (dur > 0 && dur < 20) {
                p.crouchDurations.insert(p.crouchDurations.begin(), dur);
                if (p.crouchDurations.size() > 10) p.crouchDurations.pop_back();
            }
        }

        if (g_jc.m_isSprinting) p.isSprinting = env->CallBooleanMethod(ent, g_jc.m_isSprinting) == JNI_TRUE;
        if (p.isSwingInProgress) p.lastSwingTick = tick;

        p.heldItemId = 0;
        if (g_jc.m_getHeldItem) {
            jobject stack = env->CallObjectMethod(ent, g_jc.m_getHeldItem);
            if (stack) {
                jobject item = env->CallObjectMethod(stack, g_jc.m_isGetItem);
                if (item) {
                    p.heldItemId = (int)env->CallStaticIntMethod(g_jc.itemCls, g_jc.m_itemGetIdStatic, item);
                    env->DeleteLocalRef(item);
                }
                env->DeleteLocalRef(stack);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
        }

        for (auto& chk : g_checks) {
            if (chk) chk->onPlayerTick(p, env, ent);
        }


        p.wasSneaking = p.isSneaking;
        p.wasBlocking = p.isBlocking;
        p.wasSwingInProgress = p.isSwingInProgress;

        env->DeleteLocalRef(ent);
    }
  }



    for (auto it = g_players.begin(); it != g_players.end();) {
        if (seen.find(it->first) == seen.end())
            it = g_players.erase(it);
        else
            ++it;
    }

    env->DeleteLocalRef(playerList);
    if (localPlayer)
        env->DeleteLocalRef(localPlayer);
}


void initialize() {
  abLog("INIT: Anticheat::initialize() called, pid=%lu",
        GetCurrentProcessId());
  if (g_started.exchange(true)) {
    debugLog("initialize: already started, skipping");
    abLog("INIT: already started, skipping");
    return;
  }

  g_checks.emplace_back(Checks::makeNoSlow());
  g_checks.emplace_back(Checks::makeAutoBlock());
  g_checks.emplace_back(Checks::makeEagle());
  g_checks.emplace_back(Checks::makeScaffold());

  debugLog(
      "initialize: %d checks loaded — NoSlow AutoBlock Eagle Scaffold",
      (int)g_checks.size());
  debugLog("initialize: config master=%d noslow=%d autoblock=%d eagle=%d "
           "scaffold=%d vl=%d cooldown=%d",
           (int)Config::isAnticheatEnabled(),
           (int)Config::isAnticheatNoSlowEnabled(),
           (int)Config::isAnticheatAutoBlockEnabled(),
           (int)Config::isAnticheatEagleEnabled(),
           (int)Config::isAnticheatScaffoldEnabled(),
           Config::getAnticheatVl(),
           Config::getAnticheatCooldownSec());
  Logger::info("Anticheat initialized with %d checks", (int)g_checks.size());
}

void shutdown() {
  g_started.store(false);
  std::lock_guard<std::mutex> lock(g_playersMutex);
  g_players.clear();
  g_checks.clear();
}

void clearAllPlayers() {
  std::unique_lock<std::mutex> lock(g_playersMutex, std::try_to_lock);
  if (!lock.owns_lock())
    return;
  g_players.clear();
}

void tickFromRenderThread() {
  static bool s_loggedFirstCall = false;
  if (!s_loggedFirstCall) {
    s_loggedFirstCall = true;
    debugLog("tickFromRenderThread: first call from render thread "
             "(started=%d enabled=%d)",
             (int)g_started.load(), (int)Config::isAnticheatEnabled());
    abLog("TICK: first call from render thread (started=%d enabled=%d "
          "autoblock=%d)",
          (int)g_started.load(), (int)Config::isAnticheatEnabled(),
          (int)Config::isAnticheatAutoBlockEnabled());
  }

  if (!g_started.load()) {
    static bool s_loggedNotStarted = false;
    if (!s_loggedNotStarted) {
      s_loggedNotStarted = true;
      abLog("TICK: BAILING — g_started=false (initialize() never ran)");
    }
    return;
  }
  if (!Config::isAnticheatEnabled()) {
    static bool s_loggedDisabled = false;
    if (!s_loggedDisabled) {
      s_loggedDisabled = true;
      abLog("TICK: BAILING — Config::isAnticheatEnabled()=false");
    }
    return;
  }

  static ULONGLONG lastTickMs = 0;
  ULONGLONG nowMs = GetTickCount64();
  if (nowMs - lastTickMs < 20)
    return;
  lastTickMs = nowMs;

  SafeGuard::installSehTranslator();
  SafeGuard::run("Anticheat::tick", []() {
    JNIEnv *env = lc ? lc->getEnv() : nullptr;
    if (!env) {
      static int s_noEnv = 0;
      if ((s_noEnv++ % 30) == 0)
        debugLog("tickFromRenderThread: lc->getEnv() returned null");
      return;
    }
    tickOnce(env);
  });
}

static std::string cleanAcName(const std::string &s) {
  std::string stripped;
  stripped.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    unsigned char c0 = (unsigned char)s[i];
    if (c0 == 0xC2 && i + 2 < s.size() &&
        (unsigned char)s[i + 1] == 0xA7) {
      i += 3;
      continue;
    }
    if (c0 == 0xA7 && i + 1 < s.size()) {
      i += 2;
      continue;
    }
    stripped += s[i];
    i++;
  }
  std::string res;
  for (char c : stripped) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') {
      res += (char)std::tolower((unsigned char)c);
    }
  }
  return res;
}

bool isPlayerSneaking(const std::string &name) {
  std::string target = cleanAcName(name);
  if (target.empty()) return false;
  std::unique_lock<std::mutex> lock(g_playersMutex, std::try_to_lock);
  if (!lock.owns_lock()) return false;
  for (const auto &kv : g_players) {
    if (cleanAcName(kv.second.name) == target)
      return kv.second.isSneaking;
  }
  return false;
}

void *getWorldPlayerEntitiesFieldID() {
  return (void *)g_jc.f_playerEntities;
}

size_t getTrackedPlayersSnapshot(std::string &outJoined) {
  std::unique_lock<std::mutex> lock(g_playersMutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    outJoined = "<mutex-busy>";
    return 0;
  }
  outJoined.clear();
  for (const auto &kv : g_players) {
    if (!outJoined.empty()) outJoined += ",";
    outJoined += cleanAcName(kv.second.name);
    if (kv.second.isSneaking) outJoined += "(snk)";
  }
  return g_players.size();
}

bool isPlayerFlagged(const std::string &name) {
  auto clean = [](const std::string &s) {
    std::string stripped;
    stripped.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
      unsigned char c0 = (unsigned char)s[i];
      if (c0 == 0xC2 && i + 2 < s.size() &&
          (unsigned char)s[i + 1] == 0xA7) {
        i += 3;
        continue;
      }
      if (c0 == 0xA7 && i + 1 < s.size()) {
        i += 2;
        continue;
      }
      stripped += s[i];
      i++;
    }
    std::string res;
    for (char c : stripped) {
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_') {
        res += (char)std::tolower((unsigned char)c);
      }
    }
    return res;
  };

  std::string target = clean(name);
  if (target.empty())
    return false;

  std::lock_guard<std::mutex> lock(g_flaggedMutex);
  bool found = g_flaggedPlayers.count(target) > 0;
  static std::unordered_set<std::string> s_loggedMiss;
  static std::unordered_set<std::string> s_loggedHit;
  if (!found && !g_flaggedPlayers.empty() && s_loggedMiss.insert(target).second) {
    std::string contents;
    for (const auto &e : g_flaggedPlayers) {
      if (!contents.empty()) contents += ",";
      contents += e;
    }
    debugLog("isPlayerFlagged MISS: lookup='%s' (raw='%s') set={%s}",
             target.c_str(), name.c_str(), contents.c_str());
  } else if (found && s_loggedHit.insert(target).second) {
    debugLog("isPlayerFlagged HIT: lookup='%s' (raw='%s')",
             target.c_str(), name.c_str());
  }
  return found;
}

} // namespace Anticheat
