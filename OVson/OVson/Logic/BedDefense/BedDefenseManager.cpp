// probably the shittiest bedplates
// logic sucks but im no cheat developer
#include "BedDefenseManager.h"
#include "../../Config/Config.h"
#include "../../Java.h"
#include "../../Utils/Logger.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace BedDefense {
BedDefenseManager *BedDefenseManager::s_instance = nullptr;

BedDefenseManager::BedDefenseManager()
    : m_enabled(Config::isBedDefenseEnabled()), m_lastRevalidation(0),
      m_isScanning(false) {
  Logger::log(Config::DebugCategory::BedDefense,
              "BedDefenseManager initialized");
}

BedDefenseManager::~BedDefenseManager() { clearAllBeds(); }

BedDefenseManager *BedDefenseManager::getInstance() {
  if (!s_instance) {
    s_instance = new BedDefenseManager();
  }
  return s_instance;
}

void BedDefenseManager::destroy() {
  if (s_instance) {
    delete s_instance;
    s_instance = nullptr;
  }
}

void BedDefenseManager::enable() {
  if (Config::isForgeEnvironment())
    return;
  m_enabled = true;
  Logger::log(Config::DebugCategory::BedDefense,
              "Bed defense detection enabled");
}

void BedDefenseManager::disable() {
  m_enabled = false;
  Logger::info("Bed defense detection disabled");
}

void BedDefenseManager::clearAllBeds() {
  std::lock_guard<std::mutex> lock(m_bedMutex);
  m_beds.clear();
  Logger::log(Config::DebugCategory::BedDetection, "Cleared all detected beds");
}

void BedDefenseManager::onWorldChange() {
  clearAllBeds();
  m_lastRevalidation = 0;
}

// ============================================================================
// im a genie in a bottle
// ============================================================================

bool BedDefenseManager::isChunkLoaded(int x, int z) {
  if (!lc)
    return false;

  JNIEnv *env = lc->getEnv();
  if (!env)
    return false;

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                             "Lnet/minecraft/client/Minecraft;",
                                             "field_71432_P", "S", "Lave;");
    if (!mcObj)
      return false;

    jobject world = lc->GetObjectField(
        mcObj, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
        "field_71441_e", "f", "Lbdb;");
    if (!world) {
      env->DeleteLocalRef(mcObj);
      return false;
    }

    jclass worldCls = lc->GetClass("net.minecraft.world.World");
    jmethodID m_isChunkLoaded = lc->GetMethodID(worldCls, "isChunkLoaded",
                                                "(IIZ)Z", "func_175680_a", "a");

    bool loaded = false;
    if (m_isChunkLoaded) {
      loaded =
          env->CallBooleanMethod(world, m_isChunkLoaded, x >> 4, z >> 4, false);
    }

    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);

    return loaded;
  } catch (...) {
    return false;
  }
}

std::string BedDefenseManager::getBlockName(int x, int y, int z) {
  if (!lc)
    return "minecraft:air";
  JNIEnv *env = lc->getEnv();
  if (!env)
    return "minecraft:air";

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                             "Lnet/minecraft/client/Minecraft;",
                                             "field_71432_P", "S", "Lave;");
    if (!mcObj)
      return "minecraft:air";

    jobject world = lc->GetObjectField(
        mcObj, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
        "field_71441_e", "f", "Lbdb;");
    if (!world) {
      env->DeleteLocalRef(mcObj);
      return "minecraft:air";
    }

    jclass bposCls = lc->GetClass("net.minecraft.util.BlockPos");
    jmethodID bposInit = env->GetMethodID(bposCls, "<init>", "(III)V");
    jobject bpos = env->NewObject(bposCls, bposInit, x, y, z);

    jclass worldCls = lc->GetClass("net.minecraft.world.World");
    jmethodID m_getState =
        lc->GetMethodID(worldCls, "getBlockState",
                        "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/"
                        "state/IBlockState;",
                        "func_180495_p", "p", "(Lcj;)Lalz;");

    jobject state =
        m_getState ? env->CallObjectMethod(world, m_getState, bpos) : nullptr;
    if (!state) {
      env->DeleteLocalRef(bpos);
      env->DeleteLocalRef(world);
      env->DeleteLocalRef(mcObj);
      return "minecraft:air";
    }

    jclass stateCls = lc->GetClass("net.minecraft.block.state.IBlockState");
    jmethodID m_getBlock =
        lc->GetMethodID(stateCls, "getBlock", "()Lnet/minecraft/block/Block;",
                        "func_177230_c", "c", "()Lafh;");

    jobject block =
        m_getBlock ? env->CallObjectMethod(state, m_getBlock) : nullptr;
    if (!block) {
      env->DeleteLocalRef(state);
      env->DeleteLocalRef(bpos);
      env->DeleteLocalRef(world);
      env->DeleteLocalRef(mcObj);
      return "minecraft:air";
    }

    std::string blockName = "minecraft:air";
    bool nameFound = false;

    jclass blockCls = lc->GetClass("net.minecraft.block.Block");
    jmethodID m_getId = lc->GetStaticMethodID(blockCls, "getIdFromBlock",
                                              "(Lnet/minecraft/block/Block;)I",
                                              "func_149682_b", "a", "(Lafh;)I");
    int id = m_getId ? env->CallStaticIntMethod(blockCls, m_getId, block) : -1;

    if (id == 0) {
      blockName = "minecraft:air";
      nameFound = true;
    } else if (id == 26) {
      blockName = "minecraft:bed";
      nameFound = true;
    } else if (id == 49) {
      blockName = "minecraft:obsidian";
      nameFound = true;
    } else if (id == 121) {
      blockName = "minecraft:end_stone";
      nameFound = true;
    } else if (id == 35) {
      blockName = "minecraft:wool";
      nameFound = true;
    } else if (id == 24) {
      blockName = "minecraft:sandstone";
      nameFound = true;
    } else if (id == 5) {
      blockName = "minecraft:planks";
      nameFound = true;
    } else if (id == 159) {
      blockName = "minecraft:stained_hardened_clay";
      nameFound = true;
    } else if (id == 172) {
      blockName = "minecraft:hardened_clay";
      nameFound = true;
    } else if (id == 80) {
      blockName = "minecraft:snow";
      nameFound = true;
    }

    if (!nameFound) {
      jmethodID m_getRegistryName =
          env->GetMethodID(blockCls, "getRegistryName",
                           "()Lnet/minecraft/util/ResourceLocation;");
      if (env->ExceptionCheck())
        env->ExceptionClear();
      if (m_getRegistryName) {
        jobject resLoc = env->CallObjectMethod(block, m_getRegistryName);
        if (resLoc) {
          jclass resLocCls =
              lc->GetClass("net.minecraft.util.ResourceLocation");
          jmethodID m_toString =
              env->GetMethodID(resLocCls, "toString", "()Ljava/lang/String;");
          jstring jstr =
              m_toString ? (jstring)env->CallObjectMethod(resLoc, m_toString)
                         : nullptr;
          if (jstr) {
            const char *utf = env->GetStringUTFChars(jstr, 0);
            if (utf) {
              blockName = utf;
              nameFound = true;
              env->ReleaseStringUTFChars(jstr, utf);
            }
            env->DeleteLocalRef(jstr);
          }
          env->DeleteLocalRef(resLoc);
        }
      }

      if (!nameFound) {
        jfieldID f_registry = lc->GetStaticFieldID(
            blockCls, "blockRegistry",
            "Lnet/minecraft/util/RegistryNamespacedDefaultedByKey;",
            "field_149771_c", "c", "Lco;");

        if (f_registry) {
          jobject registry = env->GetStaticObjectField(blockCls, f_registry);
          if (registry) {
            jclass registryCls = lc->GetClass(
                "net.minecraft.util.RegistryNamespacedDefaultedByKey");
            jmethodID m_getName = lc->GetMethodID(
                registryCls, "getNameForObject",
                "(Ljava/lang/Object;)Ljava/lang/Object;", "func_177774_c", "c",
                "(Ljava/lang/Object;)Ljava/lang/Object;");

            if (m_getName) {
              jobject resLoc =
                  env->CallObjectMethod(registry, m_getName, block);
              if (resLoc) {
                jclass resLocCls =
                    lc->GetClass("net.minecraft.util.ResourceLocation");
                jmethodID m_toString = env->GetMethodID(resLocCls, "toString",
                                                        "()Ljava/lang/String;");
                jstring jstr =
                    (jstring)env->CallObjectMethod(resLoc, m_toString);
                if (jstr) {
                  const char *utf = env->GetStringUTFChars(jstr, 0);
                  if (utf) {
                    blockName = utf;
                    nameFound = true;
                    env->ReleaseStringUTFChars(jstr, utf);
                  }
                  env->DeleteLocalRef(jstr);
                }
                env->DeleteLocalRef(resLoc);
              }
            }
            env->DeleteLocalRef(registry);
          }
        }
      }
    }

    if (!nameFound && id > 0) {
      Logger::log(Config::DebugCategory::BedDefense, "Unmapped block ID: %d",
                  id);
    }

    env->DeleteLocalRef(block);
    env->DeleteLocalRef(state);
    env->DeleteLocalRef(bpos);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
    return blockName;
  } catch (...) {
    return "minecraft:air";
  }
}

int BedDefenseManager::getBlockMetadata(int x, int y, int z) {
  if (!lc)
    return 0;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return 0;

  try {
    if (!isChunkLoaded(x, z))
      return 0;

    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                             "Lnet/minecraft/client/Minecraft;",
                                             "field_71432_P", "S", "Lave;");
    if (!mcObj)
      return 0;

    jobject world = lc->GetObjectField(
        mcObj, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
        "field_71441_e", "f", "Lbdb;");
    if (!world) {
      env->DeleteLocalRef(mcObj);
      return 0;
    }

    jclass bposCls = lc->GetClass("net.minecraft.util.BlockPos");
    jmethodID bposInit = env->GetMethodID(bposCls, "<init>", "(III)V");
    jobject bpos = env->NewObject(bposCls, bposInit, x, y, z);

    jclass worldCls = lc->GetClass("net.minecraft.world.World");
    jmethodID m_getState =
        lc->GetMethodID(worldCls, "getBlockState",
                        "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/"
                        "state/IBlockState;",
                        "func_180495_p", "p", "(Lcj;)Lalz;");

    jobject state =
        m_getState ? env->CallObjectMethod(world, m_getState, bpos) : nullptr;

    int meta = 0;
    if (state) {
      jclass stateCls = lc->GetClass("net.minecraft.block.state.IBlockState");
      jmethodID m_getBlock =
          lc->GetMethodID(stateCls, "getBlock", "()Lnet/minecraft/block/Block;",
                          "func_177230_c", "c", "()Lafh;");

      jobject block =
          m_getBlock ? env->CallObjectMethod(state, m_getBlock) : nullptr;
      if (block) {
        jclass blockCls = lc->GetClass("net.minecraft.block.Block");
        jmethodID m_getMeta =
            lc->GetMethodID(blockCls, "getMetaFromState",
                            "(Lnet/minecraft/block/state/IBlockState;)I",
                            "func_176201_c", "c", "(Lalz;)I");

        if (m_getMeta) {
          meta = env->CallIntMethod(block, m_getMeta, state);
        }
        env->DeleteLocalRef(block);
      }
      env->DeleteLocalRef(state);
    }

    if (bpos)
      env->DeleteLocalRef(bpos);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);

    return meta;
  } catch (...) {
    return 0;
  }
}

bool BedDefenseManager::isAir(int x, int y, int z) {
  return getBlockName(x, y, z) == "minecraft:air";
}

bool BedDefenseManager::isBed(int x, int y, int z) {
  std::string name = getBlockName(x, y, z);
  return name == "minecraft:bed";
}

std::string BedDefenseManager::getBedTeamColor(int x, int y, int z) {
  int meta = getBlockMetadata(x, y, z);

  switch (meta) {
  case 14:
    return "RED";
  case 11:
    return "BLUE";
  case 13:
    return "GREEN";
  case 4:
    return "YELLOW";
  case 1:
    return "ORANGE";
  case 0:
    return "WHITE";
  case 15:
    return "BLACK";
  case 9:
    return "CYAN";
  case 5:
    return "LIME";
  case 10:
    return "PURPLE";
  case 2:
    return "MAGENTA";
  case 6:
    return "PINK";
  case 12:
    return "BROWN";
  case 7:
    return "GRAY";
  case 8:
    return "LIGHT_GRAY";
  case 3:
    return "LIGHT_BLUE";
  default:
    return "UNKNOWN";
  }
}

void BedDefenseManager::detectBed(int x, int y, int z) {
  if (!m_enabled)
    return;
  if (!isBed(x, y, z))
    return;

  for (auto &pair : m_beds) {
    DetectedBed &existing = pair.second;
    if (existing.y == y && existing.distanceSquared(x, y, z) < 2.5) {
      return;
    }
  }

  std::string team = getBedTeamColor(x, y, z);
  DetectedBed bed(x, y, z, team);

  std::string key = bed.getKey();
  m_beds[key] = bed;

  Logger::log(Config::DebugCategory::BedDetection,
              "Detected bed at (%d, %d, %d) - Team: %s", x, y, z, team.c_str());
}

void BedDefenseManager::removeBed(int x, int y, int z) {
  DetectedBed temp(x, y, z, "");
  std::string key = temp.getKey();

  auto it = m_beds.find(key);
  if (it != m_beds.end()) {
    m_beds.erase(it);
    Logger::log(Config::DebugCategory::BedDetection,
                "Removed bed at (%d, %d, %d)", x, y, z);
  }
}

void BedDefenseManager::markBedDirty(int x, int y, int z, int radius) {
  int radiusSquared = radius * radius;

  for (auto &pair : m_beds) {
    DetectedBed &bed = pair.second;
    if (bed.distanceSquared(x, y, z) <= radiusSquared) {
      bed.dirty = true;
    }
  }
}

void BedDefenseManager::onBlockChange(int x, int y, int z) {
  if (!m_enabled)
    return;

  if (isBed(x, y, z)) {
    detectBed(x, y, z);
  } else {
    removeBed(x, y, z);
  }

  markBedDirty(x, y, z, 5);
}

std::string BedDefenseManager::getTeamFromProximity(int bx, int by, int bz) {
  JNIEnv *env = lc->getEnv();
  if (!env)
    return "BED";

  jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
  jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                        "Lnet/minecraft/client/Minecraft;",
                                        "field_71432_P", "S", "Lave;");
  jobject mcObj = env->GetStaticObjectField(mcCls, theMc);
  if (!mcObj)
    return "BED";

  jfieldID f_world = lc->GetFieldID(
      mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
      "field_71441_e", "f", "Lbdb;");
  jobject world = env->GetObjectField(mcObj, f_world);
  if (!world) {
    env->DeleteLocalRef(mcObj);
    return "BED";
  }

  jclass worldCls = lc->GetClass("net.minecraft.world.World");
  jmethodID m_getState = lc->GetMethodID(
      worldCls, "getBlockState",
      "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/state/IBlockState;",
      "func_180495_p", "p", "(Lcj;)Lalz;");

  jclass bposCls = lc->GetClass("net.minecraft.util.BlockPos");
  jmethodID bposInit = env->GetMethodID(bposCls, "<init>", "(III)V");

  jclass stateCls = lc->GetClass("net.minecraft.block.state.IBlockState");
  jmethodID m_getBlock =
      lc->GetMethodID(stateCls, "getBlock", "()Lnet/minecraft/block/Block;",
                      "func_177230_c", "c", "()Lafh;");

  jclass blockCls = lc->GetClass("net.minecraft.block.Block");
  jmethodID m_getId = lc->GetStaticMethodID(blockCls, "getIdFromBlock",
                                            "(Lnet/minecraft/block/Block;)I",
                                            "func_149682_b", "a", "(Lafh;)I");
  jmethodID m_getMeta =
      lc->GetMethodID(blockCls, "getMetaFromState",
                      "(Lnet/minecraft/block/state/IBlockState;)I",
                      "func_176201_c", "c", "(Lalz;)I");

  std::string detectedTeam = "BED";

  for (int x = bx - 1; x <= bx + 1; x++) {
    for (int z = bz - 1; z <= bz + 1; z++) {
      jobject bpos = env->NewObject(bposCls, bposInit, x, by - 1, z);
      jobject state = env->CallObjectMethod(world, m_getState, bpos);
      if (state) {
        jobject block = env->CallObjectMethod(state, m_getBlock);
        if (block) {
          int id =
              m_getId ? env->CallStaticIntMethod(blockCls, m_getId, block) : -1;
          if (id == 35 || id == 159) {
            int meta =
                m_getMeta ? env->CallIntMethod(block, m_getMeta, state) : 0;
            switch (meta) {
            case 14:
              detectedTeam = "RED";
              break;
            case 11:
              detectedTeam = "BLUE";
              break;
            case 13:
              detectedTeam = "GREEN";
              break;
            case 4:
              detectedTeam = "YELLOW";
              break;
            case 1:
              detectedTeam = "ORANGE";
              break;
            case 3:
              detectedTeam = "AQUA";
              break;
            case 10:
              detectedTeam = "PURPLE";
              break;
            case 0:
              detectedTeam = "WHITE";
              break;
            }
          }
          env->DeleteLocalRef(block);
        }
        env->DeleteLocalRef(state);
      }
      env->DeleteLocalRef(bpos);
      if (detectedTeam != "BED")
        break;
    }
    if (detectedTeam != "BED")
      break;
  }

  env->DeleteLocalRef(world);
  env->DeleteLocalRef(mcObj);
  return detectedTeam;
}

void BedDefenseManager::onChunkLoad(int chunkX, int chunkZ) {
  scanChunkInto(chunkX, chunkZ, m_beds);
}
void BedDefenseManager::scanChunkInto(
    int chunkX, int chunkZ,
    std::unordered_map<std::string, DetectedBed> &targetMap) {
  if (!m_enabled || !lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  if (env->PushLocalFrame(100) != 0)
    return;
  int bedsFoundInChunk = 0;

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                             "Lnet/minecraft/client/Minecraft;",
                                             "field_71432_P", "S", "Lave;");
    if (!mcObj) {
      env->PopLocalFrame(nullptr);
      return;
    }

    jobject world = lc->GetObjectField(
        mcObj, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
        "field_71441_e", "f", "Lbdb;");
    if (!world) {
      env->DeleteLocalRef(mcObj);
      env->PopLocalFrame(nullptr);
      return;
    }

    jclass worldCls = lc->GetClass("net.minecraft.world.World");
    jmethodID m_getChunk =
        lc->GetMethodID(worldCls, "getChunkFromChunkCoords",
                        "(II)Lnet/minecraft/world/chunk/Chunk;", "func_72964_e",
                        "a", "(II)Lamy;");
    if (!m_getChunk) {
      env->DeleteLocalRef(world);
      env->DeleteLocalRef(mcObj);
      env->PopLocalFrame(nullptr);
      return;
    }

    jobject chunk = env->CallObjectMethod(world, m_getChunk, chunkX, chunkZ);
    if (chunk) {
      jclass chunkCls = env->GetObjectClass(chunk);
      jobjectArray storageArray = (jobjectArray)lc->GetObjectField(
          chunk, "storageArrays",
          "[Lnet/minecraft/world/chunk/storage/ExtendedBlockStorage;",
          "field_76652_q", "d", "[Lamz;");

      if (storageArray == nullptr) {
        jfieldID f_storage = lc->FindFieldBySignature(
            chunkCls,
            "[Lnet/minecraft/world/chunk/storage/ExtendedBlockStorage;");
        if (!f_storage)
          f_storage = lc->FindFieldBySignature(chunkCls, "[Lamz;");
        if (f_storage)
          storageArray = (jobjectArray)env->GetObjectField(chunk, f_storage);
      }

      if (storageArray) {
        jclass bposCls = lc->GetClass("net.minecraft.util.BlockPos");
        jmethodID bposInit = env->GetMethodID(bposCls, "<init>", "(III)V");

        jclass blockCls = lc->GetClass("net.minecraft.block.Block");
        jmethodID m_getId = lc->GetStaticMethodID(
            blockCls, "getIdFromBlock", "(Lnet/minecraft/block/Block;)I",
            "func_149682_b", "a", "(Lafh;)I");
        jmethodID m_getMeta =
            lc->GetMethodID(blockCls, "getMetaFromState",
                            "(Lnet/minecraft/block/state/IBlockState;)I",
                            "func_176201_c", "c", "(Lalz;)I");
        jmethodID m_getUnlocalized =
            lc->GetMethodID(blockCls, "getUnlocalizedName",
                            "()Ljava/lang/String;", "func_149739_a", "a");

        jmethodID m_getState =
            lc->GetMethodID(worldCls, "getBlockState",
                            "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/"
                            "block/state/IBlockState;",
                            "func_180495_p", "p", "(Lcj;)Lalz;");

        jclass stateCls = lc->GetClass("net.minecraft.block.state.IBlockState");
        jmethodID m_getBlock = lc->GetMethodID(stateCls, "getBlock",
                                               "()Lnet/minecraft/block/Block;",
                                               "func_177230_c", "c", "()Lafh;");

        jclass bedCls = lc->GetClass("net.minecraft.block.BlockBed");

        for (int s = 0; s < 16; s++) {
          jobject storage = env->GetObjectArrayElement(storageArray, s);
          if (!storage)
            continue;
          env->DeleteLocalRef(storage);

          for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
              env->PushLocalFrame(20);
              for (int y = s * 16; y < (s * 16) + 16; y++) {
                int wx = (chunkX * 16) + x;
                int wz = (chunkZ * 16) + z;

                jobject bpos = env->NewObject(bposCls, bposInit, wx, y, wz);
                jobject state =
                    (m_getState)
                        ? env->CallObjectMethod(world, m_getState, bpos)
                        : nullptr;
                if (state) {
                  jobject block = (m_getBlock)
                                      ? env->CallObjectMethod(state, m_getBlock)
                                      : nullptr;
                  if (block) {
                    int id =
                        (m_getId)
                            ? env->CallStaticIntMethod(blockCls, m_getId, block)
                            : -1;
                    bool isABed = (id == 26) ||
                                  (bedCls && env->IsInstanceOf(block, bedCls));

                    if (!isABed && m_getUnlocalized) {
                      jstring jName = (jstring)env->CallObjectMethod(
                          block, m_getUnlocalized);
                      if (jName) {
                        const char *nameStr =
                            env->GetStringUTFChars(jName, nullptr);
                        if (nameStr) {
                          if (strstr(nameStr, "tile.bed") &&
                              !strstr(nameStr, "bedrock"))
                            isABed = true;
                          env->ReleaseStringUTFChars(jName, nameStr);
                        }
                        env->DeleteLocalRef(jName);
                      }
                    }

                    if (isABed) {
                      int bedMeta =
                          (m_getMeta)
                              ? env->CallIntMethod(block, m_getMeta, state)
                              : 0;
                      bool isHead = (bedMeta & 0x8) != 0;

                      if (isHead) {
                        bool exists = false;
                        for (auto &pair : targetMap) {
                          if (pair.second.y == y &&
                              pair.second.distanceSquared(wx, y, wz) < 2.5) {
                            exists = true;
                            break;
                          }
                        }
                        if (!exists) {
                          bedsFoundInChunk++;
                          std::string team = getTeamFromProximity(wx, y, wz);
                          DetectedBed bed(wx, y, wz, team);
                          bed.layers = selectBestLayers(bed);
                          targetMap[bed.getKey()] = bed;
                          Logger::log(Config::DebugCategory::BedDetection,
                                      "Detected %s Bed (Head) at %d, %d, %d",
                                      team.c_str(), wx, y, wz);
                        }
                      }
                    }
                    env->DeleteLocalRef(block);
                  }
                  env->DeleteLocalRef(state);
                }
                env->DeleteLocalRef(bpos);
              }
              env->PopLocalFrame(nullptr);
            }
          }
        }
        env->DeleteLocalRef(storageArray);
      }
      env->DeleteLocalRef(chunkCls);
      env->DeleteLocalRef(chunk);
    }
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(mcObj);
  } catch (...) {
  }

  if (bedsFoundInChunk > 0) {
    Logger::log(Config::DebugCategory::BedDetection,
                "Scan Result: Found %d beds in chunk %d, %d", bedsFoundInChunk,
                chunkX, chunkZ);
  }
  env->PopLocalFrame(nullptr);
}

std::vector<DefenseLayer>
BedDefenseManager::scanDirection(const DetectedBed &bed, int dx, int dy,
                                 int dz) {
  std::vector<DefenseLayer> layers;
  std::string lastBlock;
  const int MAX_DEPTH = 4;

  for (int i = 1; i <= MAX_DEPTH; i++) {
    int x = bed.x + (dx * i);
    int y = bed.y + (dy * i);
    int z = bed.z + (dz * i);
    if (y < bed.y)
      continue;
    std::string blockName = getBlockName(x, y, z);
    if (blockName == "minecraft:air" || blockName.empty())
      continue;
    if (blockName.find("bed") != std::string::npos)
      continue;

    bool isDefense = false;
    if (blockName.find("wool") != std::string::npos)
      isDefense = true;
    else if (blockName.find("stained_hardened_clay") != std::string::npos)
      isDefense = true;
    else if (blockName.find("obsidian") != std::string::npos)
      isDefense = true;
    else if (blockName.find("end_stone") != std::string::npos)
      isDefense = true;
    else if (blockName.find("wood") != std::string::npos ||
             blockName.find("planks") != std::string::npos)
      isDefense = true;
    else if (blockName.find("glass") != std::string::npos)
      isDefense = true;

    if (!isDefense)
      continue;

    if (blockName != lastBlock) {
      int meta = getBlockMetadata(x, y, z);
      DefenseLayer layer(blockName, meta);
      layer.hasTexture = false;
      layers.push_back(layer);
      lastBlock = blockName;
    }
  }
  return layers;
}

void BedDefenseManager::fillRenderData(DefenseLayer &layer) {
  if (!lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;

  if (layer.blockName.find("wool") != std::string::npos ||
      layer.blockName.find("stained_hardened_clay") != std::string::npos) {
    switch (layer.metadata) {
    case 0:
      layer.color = 0xFFFFFFFF;
      break; // White
    case 1:
      layer.color = 0xFFFFA500;
      break; // Orange
    case 2:
      layer.color = 0xFFBF40BF;
      break; // Magenta
    case 3:
      layer.color = 0xFFADD8E6;
      break; // Light Blue
    case 4:
      layer.color = 0xFFFFFF00;
      break; // Yellow
    case 5:
      layer.color = 0xFF32CD32;
      break; // Lime
    case 6:
      layer.color = 0xFFFFC0CB;
      break; // Pink
    case 7:
      layer.color = 0xFF808080;
      break; // Gray
    case 8:
      layer.color = 0xFFC0C0C0;
      break; // Silver
    case 9:
      layer.color = 0xFF00FFFF;
      break; // Cyan
    case 10:
      layer.color = 0xFF800080;
      break; // Purple
    case 11:
      layer.color = 0xFF0000FF;
      break; // Blue
    case 12:
      layer.color = 0xFFA52A2A;
      break; // Brown
    case 13:
      layer.color = 0xFF008000;
      break; // Green
    case 14:
      layer.color = 0xFFFF0000;
      break; // Red
    case 15:
      layer.color = 0xFF000000;
      break; // Black
    }
  } else if (layer.blockName.find("glass") != std::string::npos) {
    switch (layer.metadata) {
    case 0:
      layer.color = 0x80FFFFFF;
      break; // White/Clear
    case 1:
      layer.color = 0x80FFA500;
      break; // Orange
    case 2:
      layer.color = 0x80BF40BF;
      break; // Magenta
    case 3:
      layer.color = 0x80ADD8E6;
      break; // Light Blue
    case 4:
      layer.color = 0x80FFFF00;
      break; // Yellow
    case 5:
      layer.color = 0x8032CD32;
      break; // Lime
    case 6:
      layer.color = 0x80FFC0CB;
      break; // Pink
    case 7:
      layer.color = 0x80808080;
      break; // Gray
    case 8:
      layer.color = 0x80C0C0C0;
      break; // Silver
    case 9:
      layer.color = 0x8000FFFF;
      break; // Cyan
    case 10:
      layer.color = 0x80800080;
      break; // Purple
    case 11:
      layer.color = 0x800000FF;
      break; // Blue
    case 12:
      layer.color = 0x80A52A2A;
      break; // Brown
    case 13:
      layer.color = 0x80008000;
      break; // Green
    case 14:
      layer.color = 0x80FF0000;
      break; // Red
    case 15:
      layer.color = 0x80000000;
      break; // Black
    }
  } else if (layer.blockName.find("wood") != std::string::npos ||
             layer.blockName.find("planks") != std::string::npos) {
    layer.color = 0xFF8B4513; // Brown
  } else if (layer.blockName.find("obsidian") != std::string::npos) {
    layer.color = 0xFF310062; // Deep Purple
  } else if (layer.blockName.find("end_stone") != std::string::npos) {
    layer.color = 0xFFF0E68C; // Khaki
  } else if (layer.blockName.find("glass") != std::string::npos) {
    layer.color = 0xAAFFFFFF; // Translucent White
  } else if (layer.blockName.find("bed") != std::string::npos) {
    layer.color = 0xFFFF0000; // Bright Red
  } else {
    layer.color = 0xFF888888;
  }

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jfieldID theMc = lc->GetStaticFieldID(mcCls, "theMinecraft",
                                          "Lnet/minecraft/client/Minecraft;",
                                          "field_71432_P", "S", "Lave;");
    jobject mcObj = theMc ? env->GetStaticObjectField(mcCls, theMc) : nullptr;

    jmethodID m_getBlockRenderer = lc->GetMethodID(
        mcCls, "getBlockRendererDispatcher",
        "()Lnet/minecraft/client/renderer/BlockRendererDispatcher;",
        "func_175602_ab", "ae", "()Lbgd;");
    jobject dispatcher = env->CallObjectMethod(mcObj, m_getBlockRenderer);

    jclass blockCls = lc->GetClass("net.minecraft.block.Block");
    jmethodID m_getBlockFromName = lc->GetStaticMethodID(
        blockCls, "getBlockFromName",
        "(Ljava/lang/String;)Lnet/minecraft/block/Block;", "func_149684_b", "b",
        "(Ljava/lang/String;)Lafh;");
    jstring jBlockName = env->NewStringUTF(layer.blockName.c_str());
    jobject blockObj =
        env->CallStaticObjectMethod(blockCls, m_getBlockFromName, jBlockName);

    if (blockObj) {
      jmethodID m_getState =
          lc->GetMethodID(blockCls, "getStateFromMeta",
                          "(I)Lnet/minecraft/block/state/IBlockState;",
                          "func_176203_a", "a", "(I)Lalz;");
      jobject state =
          env->CallObjectMethod(blockObj, m_getState, layer.metadata);

      jclass dispatchCls = env->GetObjectClass(dispatcher);
      jmethodID m_getModel =
          lc->GetMethodID(dispatchCls, "getBlockModelShapes",
                          "()Lnet/minecraft/client/renderer/BlockModelShapes;",
                          "func_175023_a", "a", "()Lbgc;");
      jobject shapes = env->CallObjectMethod(dispatcher, m_getModel);

      jclass shapesCls = env->GetObjectClass(shapes);
      jmethodID m_getQuads = lc->GetMethodID(
          shapesCls, "getTexture",
          "(Lnet/minecraft/block/state/IBlockState;)Lnet/minecraft/client/"
          "renderer/texture/TextureAtlasSprite;",
          "func_178122_a", "a", "(Lalz;)Lbmi;");
      jobject sprite = env->CallObjectMethod(shapes, m_getQuads, state);

      if (sprite) {
        jclass spriteCls = env->GetObjectClass(sprite);
        jmethodID m_getU =
            lc->GetMethodID(spriteCls, "getMinU", "()F", "func_94209_e", "e");
        if (!m_getU) {
          if (env->ExceptionCheck())
            env->ExceptionClear();
          m_getU =
              lc->GetMethodID(spriteCls, "getMinU", "()F", "func_94209_e", "e");
        }
        jmethodID m_getMaxU =
            lc->GetMethodID(spriteCls, "getMaxU", "()F", "func_94212_f", "f");
        jmethodID m_getV =
            lc->GetMethodID(spriteCls, "getMinV", "()F", "func_94206_g", "g");
        jmethodID m_getMaxV =
            lc->GetMethodID(spriteCls, "getMaxV", "()F", "func_94207_h", "h");

        layer.minU = env->CallFloatMethod(sprite, m_getU);
        layer.maxU = env->CallFloatMethod(sprite, m_getMaxU);
        layer.minV = env->CallFloatMethod(sprite, m_getV);
        layer.maxV = env->CallFloatMethod(sprite, m_getMaxV);

        if (layer.minU == layer.maxU || layer.minV == layer.maxV ||
            (layer.minU == 0 && layer.maxU == 0)) {
          layer.hasTexture = false;
        } else {
          layer.hasTexture = true;
        }

        env->DeleteLocalRef(sprite);
        env->DeleteLocalRef(spriteCls);
      } else {
        layer.hasTexture = false;
      }
      env->DeleteLocalRef(shapes);
      env->DeleteLocalRef(shapesCls);
      env->DeleteLocalRef(state);
    }
    if (jBlockName)
      env->DeleteLocalRef(jBlockName);
    env->DeleteLocalRef(dispatcher);
    env->DeleteLocalRef(mcObj);
  } catch (...) {
  }
}

bool BedDefenseManager::hasObsidian(const std::vector<DefenseLayer> &layers) {
  for (const auto &layer : layers) {
    if (layer.blockName == "minecraft:obsidian") {
      return true;
    }
  }
  return false;
}

std::vector<DefenseLayer>
BedDefenseManager::selectBestLayers(DetectedBed &bed) {
  std::vector<DefenseLayer> best;
  std::unordered_map<std::string, int> uniqueLayers;

  struct Direction {
    int dx, dy, dz;
  };
  Direction directions[] = {
      {1, 0, 0},  // +X
      {-1, 0, 0}, // -X
      {0, 0, 1},  // +Z
      {0, 0, -1}, // -Z
      {0, 1, 0}   // +Y
  };

  for (const auto &dir : directions) {
    std::vector<DefenseLayer> dirLayers =
        scanDirection(bed, dir.dx, dir.dy, dir.dz);
    for (const auto &l : dirLayers) {
      uniqueLayers[l.blockName] = l.metadata;
    }
  }

  for (auto const &[name, meta] : uniqueLayers) {
    DefenseLayer layer(name, meta);
    fillRenderData(layer);
    best.push_back(layer);
  }

  std::sort(best.begin(), best.end(),
            [](const DefenseLayer &a, const DefenseLayer &b) {
              if (a.blockName.find("obsidian") != std::string::npos)
                return true;
              if (b.blockName.find("obsidian") != std::string::npos)
                return false;
              return a.blockName < b.blockName;
            });

  return best;
}

void BedDefenseManager::forceScan() {
  if (Config::isForgeEnvironment() || !m_enabled || m_isScanning)
    return;

  m_isScanning = true;
  std::thread([this]() { asyncScanTask(); }).detach();
}

void BedDefenseManager::asyncScanTask() {
  if (Config::isForgeEnvironment() || !lc) {
    m_isScanning = false;
    return;
  }
  JNIEnv *env = lc->getEnv();
  if (!env) {
    m_isScanning = false;
    return;
  }

  Logger::log(Config::DebugCategory::BedDefense, "Background scan started...");

  try {
    jclass mcCls = lc->GetClass("net.minecraft.client.Minecraft");
    jobject mcObj = lc->GetStaticObjectField(mcCls, "theMinecraft",
                                             "Lnet/minecraft/client/Minecraft;",
                                             "field_71432_P", "S", "Lave;");
    if (!mcObj) {
      m_isScanning = false;
      return;
    }

    jfieldID f_player = lc->GetFieldID(
        mcCls, "thePlayer", "Lnet/minecraft/client/entity/EntityPlayerSP;",
        "field_71439_g", "h", "Lbew;");
    if (!f_player)
      f_player = lc->FindFieldBySignature(
          mcCls, "Lnet/minecraft/client/entity/EntityPlayerSP;");
    if (!f_player)
      f_player = lc->FindFieldBySignature(mcCls, "Lbew;");

    jobject player = f_player ? env->GetObjectField(mcObj, f_player) : nullptr;
    if (!player) {
      env->DeleteLocalRef(mcObj);
      m_isScanning = false;
      return;
    }

    jclass entityCls = lc->GetClass("net.minecraft.entity.Entity");
    jfieldID f_px =
        lc->GetFieldID(entityCls, "posX", "D", "field_70165_t", "s");
    jfieldID f_pz =
        lc->GetFieldID(entityCls, "posZ", "D", "field_70161_v", "u");

    if (!f_px)
      f_px = lc->FindFieldBySignature(entityCls, "D");
    if (!f_pz)
      f_pz = lc->FindFieldBySignature(entityCls, "D"); // Fallback for posZ

    if (f_px && f_pz) {
      double px = env->GetDoubleField(player, f_px);
      double pz = env->GetDoubleField(player, f_pz);
      int playerCX = (int)px >> 4;
      int playerCZ = (int)pz >> 4;

      jfieldID f_world = lc->GetFieldID(
          mcCls, "theWorld", "Lnet/minecraft/client/multiplayer/WorldClient;",
          "field_71441_e", "f", "Lbdb;");
      if (!f_world)
        f_world = lc->FindFieldBySignature(
            mcCls, "Lnet/minecraft/client/multiplayer/WorldClient;");
      if (!f_world)
        f_world = lc->FindFieldBySignature(mcCls, "Lbdb;");

      jobject world = f_world ? env->GetObjectField(mcObj, f_world) : nullptr;
      if (world) {
        jclass worldClientCls =
            lc->GetClass("net.minecraft.client.multiplayer.WorldClient");
        jfieldID f_cp = lc->GetFieldID(
            worldClientCls, "chunkProvider",
            "Lnet/minecraft/client/multiplayer/ChunkProviderClient;",
            "field_73033_b", "b", "Lbcz;");
        if (!f_cp)
          f_cp = lc->FindFieldBySignature(
              worldClientCls,
              "Lnet/minecraft/client/multiplayer/ChunkProviderClient;");
        if (!f_cp)
          f_cp = lc->FindFieldBySignature(worldClientCls, "Lbcz;");

        if (!f_cp && worldClientCls) {
          if (Config::isGlobalDebugEnabled()) {
            Logger::log(Config::DebugCategory::BedDefense,
                        "FAILED: chunkProvider. Dumping WorldClient fields...");
            jint fc = 0;
            jfieldID *pf = nullptr;
            lc->jvmti->GetClassFields(worldClientCls, &fc, &pf);
            for (int i = 0; i < fc; i++) {
              char *n = nullptr, *s = nullptr;
              lc->jvmti->GetFieldName(worldClientCls, pf[i], &n, &s, nullptr);
              if (n && s)
                Logger::log(Config::DebugCategory::BedDefense, "Field: %s | %s",
                            n, s);
              if (n)
                lc->jvmti->Deallocate((unsigned char *)n);
              if (s)
                lc->jvmti->Deallocate((unsigned char *)s);
            }
            if (pf)
              lc->jvmti->Deallocate((unsigned char *)pf);
          }
        }

        jobject cp = f_cp ? env->GetObjectField(world, f_cp) : nullptr;

        if (cp) {
          jclass cpCls = env->GetObjectClass(cp);

          jclass clsCls = env->FindClass("java/lang/Class");
          jmethodID m_getName =
              env->GetMethodID(clsCls, "getName", "()Ljava/lang/String;");
          jstring jName = (jstring)env->CallObjectMethod(cpCls, m_getName);
          const char *nameStr = env->GetStringUTFChars(jName, nullptr);
          Logger::log(Config::DebugCategory::BedDefense,
                      "ChunkProvider Class: %s", nameStr);

          jfieldID f_cx = nullptr;
          jfieldID f_cz = nullptr;
          int chunksScanned = 0;
          std::unordered_map<std::string, DetectedBed> snapshot;
          bool scanInitialized = false;

          auto initScanVars = [&]() {
            if (scanInitialized)
              return;
            jclass chunkCls = lc->GetClass("net.minecraft.world.chunk.Chunk");
            f_cx = lc->GetFieldID(chunkCls, "xPosition", "I", "field_76635_g",
                                  "g");
            f_cz = lc->GetFieldID(chunkCls, "zPosition", "I", "field_76647_h",
                                  "h");
            scanInitialized = true;
          };

          jfieldID f_listing = lc->GetFieldID(
              cpCls, "chunkListing", "Ljava/util/List;", "field_73239_b", "b");
          if (!f_listing)
            f_listing = lc->FindFieldBySignature(cpCls, "Ljava/util/List;");
          if (!f_listing)
            f_listing =
                lc->FindFieldBySignature(cpCls, "Ljava/util/Collection;");

          if (!f_listing) {
            jfieldID f_mapping = lc->GetFieldID(
                cpCls, "chunkMapping", "Lnet/minecraft/util/LongHashMap;",
                "field_73238_a", "c");
            if (!f_mapping)
              f_mapping = lc->FindFieldBySignature(
                  cpCls, "Lnet/minecraft/util/LongHashMap;");

            if (f_mapping) {
              jobject mapping = env->GetObjectField(cp, f_mapping);
              if (mapping) {
                jclass lhmCls = env->GetObjectClass(mapping);
                jfieldID f_hashArray =
                    lc->GetFieldID(lhmCls, "hashArray",
                                   "[Lnet/minecraft/util/LongHashMap$Entry;",
                                   "field_76159_a", "a");
                if (!f_hashArray)
                  f_hashArray = lc->FindFieldBySignature(
                      lhmCls, "[Lnet/minecraft/util/LongHashMap$Entry;");

                if (f_hashArray) {
                  jobjectArray hashArray =
                      (jobjectArray)env->GetObjectField(mapping, f_hashArray);
                  if (hashArray) {
                    jsize arraySize = env->GetArrayLength(hashArray);
                    jclass entryCls = nullptr;
                    jfieldID f_value = nullptr;
                    jfieldID f_next = nullptr;

                    std::vector<std::pair<int, int>> chunksToScan;
                    int totalChunksFound = 0;

                    for (int i = 0; i < arraySize; i++) {
                      jobject entry = env->GetObjectArrayElement(hashArray, i);
                      while (entry) {
                        if (!entryCls) {
                          entryCls = env->GetObjectClass(entry);
                          f_value = lc->GetFieldID(entryCls, "value",
                                                   "Ljava/lang/Object;",
                                                   "field_76174_b", "b");
                          f_next = lc->GetFieldID(
                              entryCls, "nextEntry",
                              "Lnet/minecraft/util/LongHashMap$Entry;",
                              "field_76175_c", "c");
                        }

                        jobject chunk = env->GetObjectField(entry, f_value);
                        if (chunk) {
                          initScanVars();
                          int cx = env->GetIntField(chunk, f_cx);
                          int cz = env->GetIntField(chunk, f_cz);
                          if (std::abs(cx - playerCX) <= 10 &&
                              std::abs(cz - playerCZ) <= 10) {
                            scanChunkInto(cx, cz, snapshot);
                            chunksScanned++;
                          }
                          totalChunksFound++;
                          env->DeleteLocalRef(chunk);
                        }

                        jobject next = env->GetObjectField(entry, f_next);
                        env->DeleteLocalRef(entry);
                        entry = next;
                      }
                    }
                    Logger::log(Config::DebugCategory::BedDefense,
                                "Scan Complete (Map): %d/%d chunks.",
                                chunksScanned, totalChunksFound);
                    env->DeleteLocalRef(hashArray);
                  }
                }
                env->DeleteLocalRef(lhmCls);
                env->DeleteLocalRef(mapping);
              }
            } else {
              if (Config::isGlobalDebugEnabled()) {
                Logger::log(
                    Config::DebugCategory::BedDefense,
                    "FAILED: chunkListing and chunkMapping. CP Class: %s",
                    nameStr);
              }
            }
          }

          jobject listing =
              f_listing ? env->GetObjectField(cp, f_listing) : nullptr;
          if (listing) {
            jclass listCls = env->FindClass("java/util/List");
            if (!listCls)
              listCls = env->FindClass("java/util/Collection");
            jmethodID m_size = env->GetMethodID(listCls, "size", "()I");
            jmethodID m_get =
                env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
            if (env->ExceptionCheck())
              env->ExceptionClear();

            int size = env->CallIntMethod(listing, m_size);
            const int MAX_CHUNK_RADIUS = 10;

            for (int i = 0; i < size; i++) {
              jobject chunk = env->CallObjectMethod(listing, m_get, i);
              if (chunk) {
                initScanVars();
                int cx = env->GetIntField(chunk, f_cx);
                int cz = env->GetIntField(chunk, f_cz);
                if (std::abs(cx - playerCX) <= MAX_CHUNK_RADIUS &&
                    std::abs(cz - playerCZ) <= MAX_CHUNK_RADIUS) {
                  scanChunkInto(cx, cz, snapshot);
                  chunksScanned++;
                }
                env->DeleteLocalRef(chunk);
              }
            }
            env->DeleteLocalRef(listing);
          }

          if (chunksScanned > 0) {
            std::lock_guard<std::mutex> lock(m_bedMutex);
            m_beds = std::move(snapshot);

            Logger::log(Config::DebugCategory::BedDefense,
                        "Scan Complete: %d chunks. Total Beds: %d",
                        chunksScanned, (int)m_beds.size());
          }
          env->ReleaseStringUTFChars(jName, nameStr);
          env->DeleteLocalRef(jName);
          env->DeleteLocalRef(cpCls);
          env->DeleteLocalRef(cp);
        }
        env->DeleteLocalRef(world);
      }
    }
    env->DeleteLocalRef(player);
    env->DeleteLocalRef(mcObj);
  } catch (...) {
  }

  m_isScanning = false;
}
void BedDefenseManager::tick() {
  if (Config::isForgeEnvironment() || !m_enabled)
    return;
  ULONGLONG now = GetTickCount64();

  static ULONGLONG lastNearbyScan = 0;
  if (now - lastNearbyScan >= 10000) {
    lastNearbyScan = now;
    forceScan();
  }

  if (now - m_lastRevalidation >= 500) {
    m_lastRevalidation = now;
    std::lock_guard<std::mutex> lock(m_bedMutex);
    for (auto &pair : m_beds) {
      DetectedBed &bed = pair.second;
      if (bed.dirty || (now - bed.lastScan > 10000)) {
        bed.layers = selectBestLayers(bed);
        bed.dirty = false;
        bed.lastScan = now;
      }
    }
  }
}
} // namespace BedDefense
