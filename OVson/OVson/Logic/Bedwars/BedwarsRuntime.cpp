#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "BedwarsRuntime.h"

#include "BedwarsConfig.h"
#include "../../Config/Config.h"
#include "../../Java.h"
#include "../../Chat/ChatSDK.h"
#include "../../Render/NotificationManager.h"
#include "../../SDK/McAccess.h"
#include "../../Utils/Logger.h"
#include "../StatsTracker.internal.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace OVson::Bedwars {
namespace {

// A Hypixel match-start announcement can arrive immediately before the
// server moves the player into the game world. Keep it just long enough to
// bridge that transition, but never long enough to leak into a later match.
constexpr Tick kMapAnnouncementLifetimeMs = 20000;

void clearException(JNIEnv *env) {
  if (env && env->ExceptionCheck())
    env->ExceptionClear();
}

std::string fromJavaString(JNIEnv *env, jstring value) {
  if (!env || !value)
    return {};
  const char *characters = env->GetStringUTFChars(value, nullptr);
  if (!characters) {
    clearException(env);
    return {};
  }
  std::string result(characters);
  env->ReleaseStringUTFChars(value, characters);
  return result;
}

std::string itemName(JNIEnv *env, jobject stack) {
  if (!env || !stack || !lc)
    return {};
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  if (!stackClass)
    return {};
  jmethodID getName = lc->GetMethodID(stackClass, "getUnlocalizedName",
                                      "()Ljava/lang/String;", "func_77977_a",
                                      "a");
  if (!getName)
    return {};
  jstring name = static_cast<jstring>(env->CallObjectMethod(stack, getName));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return {};
  }
  std::string result = fromJavaString(env, name);
  if (name)
    env->DeleteLocalRef(name);
  std::replace(result.begin(), result.end(), '_', ' ');
  const std::size_t dot = result.find_last_of('.');
  if (dot != std::string::npos && dot + 1 < result.size())
    result = result.substr(dot + 1);
  return result;
}

std::string itemDisplayName(JNIEnv *env, jobject stack) {
  if (!env || !stack || !lc)
    return {};
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  jmethodID method = stackClass
                         ? lc->GetMethodID(stackClass, "getDisplayName",
                                           "()Ljava/lang/String;",
                                           "func_82833_r", "q")
                         : nullptr;
  if (!method)
    return {};
  jstring name = static_cast<jstring>(env->CallObjectMethod(stack, method));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return {};
  }
  std::string result = fromJavaString(env, name);
  if (name)
    env->DeleteLocalRef(name);
  return stripFormatting(result).substr(0, 64);
}

int itemMetadata(JNIEnv *env, jobject stack) {
  if (!env || !stack || !lc)
    return -1;
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  jmethodID method = stackClass
                         ? lc->GetMethodID(stackClass, "getItemDamage", "()I",
                                           "func_77960_j", "j")
                         : nullptr;
  if (!method)
    return -1;
  const int result = env->CallIntMethod(stack, method);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return -1;
  }
  return result;
}

std::optional<Resource> classifyResource(const std::string &rawName) {
  const std::string name = normalizeText(rawName);
  if (name.find("ingotiron") != std::string::npos ||
      name.find("iron ingot") != std::string::npos || name == "iron")
    return Resource::Iron;
  if (name.find("ingotgold") != std::string::npos ||
      name.find("gold ingot") != std::string::npos || name == "gold")
    return Resource::Gold;
  if (name.find("diamond") != std::string::npos)
    return Resource::Diamond;
  if (name.find("emerald") != std::string::npos)
    return Resource::Emerald;
  return std::nullopt;
}

ResourceSnapshot scanInventory(JNIEnv *env, jobject player) {
  ResourceSnapshot snapshot;
  if (!env || !player || !lc)
    return snapshot;
  jclass playerClass = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
  jclass inventoryClass =
      lc->GetClass("net.minecraft.entity.player.InventoryPlayer");
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  if (!playerClass || !inventoryClass || !stackClass)
    return snapshot;

  jfieldID inventoryField = lc->GetFieldID(
      playerClass, "inventory", "Lnet/minecraft/entity/player/InventoryPlayer;",
      "field_71071_by", "bi", "Lwm;");
  if (!inventoryField)
    inventoryField = lc->FindFieldBySignature(playerClass, "Lwm;");
  if (!inventoryField)
    return snapshot;
  jobject inventory = env->GetObjectField(player, inventoryField);
  clearException(env);
  if (!inventory)
    return snapshot;

  jfieldID mainField = lc->GetFieldID(
      inventoryClass, "mainInventory", "[Lnet/minecraft/item/ItemStack;",
      "field_70462_a", "a", "[Lzx;");
  if (!mainField)
    mainField = lc->FindFieldBySignature(inventoryClass, "[Lzx;");
  jfieldID sizeField = lc->GetFieldID(stackClass, "stackSize", "I",
                                      "field_77994_a", "b");
  if (!mainField || !sizeField) {
    env->DeleteLocalRef(inventory);
    return snapshot;
  }

  auto array = static_cast<jobjectArray>(env->GetObjectField(inventory, mainField));
  clearException(env);
  if (array) {
    const jsize length = std::min<jsize>(env->GetArrayLength(array), 36);
    for (jsize i = 0; i < length; ++i) {
      jobject stack = env->GetObjectArrayElement(array, i);
      if (!stack)
        continue;
      const std::string name = itemName(env, stack);
      const auto resource = classifyResource(name);
      if (resource) {
        const jint count = env->GetIntField(stack, sizeField);
        clearException(env);
        if (count > 0 && count <= 2304)
          snapshot.inventory[static_cast<std::size_t>(*resource)] += count;
      }
      env->DeleteLocalRef(stack);
    }
    env->DeleteLocalRef(array);
    snapshot.inventoryValid = true;
  }
  env->DeleteLocalRef(inventory);

  jobject minecraft = Mc::theMinecraft(env);
  jclass minecraftClass = Mc::minecraftClass();
  if (minecraft && minecraftClass) {
    jfieldID screenField = lc->GetFieldID(
        minecraftClass, "currentScreen", "Lnet/minecraft/client/gui/GuiScreen;",
        "field_71462_r", "m", "Laxu;");
    jobject screen = screenField ? env->GetObjectField(minecraft, screenField)
                                 : nullptr;
    clearException(env);
    if (screen) {
      jclass containerClass =
          lc->GetClass("net.minecraft.client.gui.inventory.GuiContainer");
      snapshot.containerOpen =
          containerClass && env->IsInstanceOf(screen, containerClass);
      env->DeleteLocalRef(screen);
    }
    env->DeleteLocalRef(minecraft);
  }
  return snapshot;
}

// Returns the best worn armour tier, and through `enchantedOut` whether any
// worn piece carries an enchantment. On Hypixel the only armour enchant a
// player can obtain is their team's Protection upgrade, so that flag is a
// direct read of an upgrade enemies are otherwise never told about.
ArmorTier armorTierFor(JNIEnv *env, jobject player, bool *enchantedOut) {
  if (enchantedOut)
    *enchantedOut = false;
  if (!env || !player || !lc)
    return ArmorTier::None;
  jclass playerClass = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
  if (!playerClass)
    return ArmorTier::None;
  jmethodID getArmor = lc->GetMethodID(
      playerClass, "getCurrentArmor", "(I)Lnet/minecraft/item/ItemStack;",
      "func_71124_b", "q", "(I)Lzx;");
  if (!getArmor)
    return ArmorTier::None;
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  jmethodID hasEffect = stackClass
                            ? lc->GetMethodID(stackClass, "hasEffect", "()Z",
                                              "func_77962_s", "s")
                            : nullptr;
  // Slot order in 1.8 is 0 boots, 1 leggings, 2 chestplate, 3 helmet.
  //
  // Only the middle two are evidence of the Protection upgrade. The kit
  // helmet already glints the moment you spawn, and the Feather Falling
  // shop upgrade enchants nothing but the boots -- so counting either of
  // those made every player on the map look like they had bought
  // Protection, which is worse than not reporting it at all. Protection is
  // applied to the whole set, so chestplate and leggings still catch it.
  constexpr int kLeggingsSlot = 1;
  constexpr int kChestplateSlot = 2;

  ArmorTier best = ArmorTier::None;
  for (int slot = 0; slot < 4; ++slot) {
    jobject stack = env->CallObjectMethod(player, getArmor, slot);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (!stack)
      continue;
    if (enchantedOut && hasEffect &&
        (slot == kLeggingsSlot || slot == kChestplateSlot) &&
        env->CallBooleanMethod(stack, hasEffect) == JNI_TRUE) {
      *enchantedOut = true;
    }
    clearException(env);
    const std::string name = normalizeText(itemName(env, stack));
    if (name.find("diamond") != std::string::npos)
      best = ArmorTier::Diamond;
    else if (name.find("iron") != std::string::npos && best < ArmorTier::Iron)
      best = ArmorTier::Iron;
    else if (name.find("chain") != std::string::npos && best < ArmorTier::Chain)
      best = ArmorTier::Chain;
    else if (name.find("leather") != std::string::npos &&
             best < ArmorTier::Leather)
      best = ArmorTier::Leather;
    env->DeleteLocalRef(stack);
  }
  return best;
}

// The starting Bedwars kit's chestplate and helmet are leather pieces dyed
// to the wearer's own team color, and unlike boots/leggings they are never
// swapped out by the Iron/Diamond armor shop upgrades -- they stay the
// same dyed leather for the whole match. That makes their dye color a
// reliable, always-available way to learn our own team that doesn't
// depend on catching the one-time "You are on the X Team!" chat line or
// on the scoreboard sidebar having resolved a prefix yet. This only ever
// fills in g_localTeam as a fallback (see setTeamColorSticky) -- it never
// overrides an already-known value, and it's a no-op once one is known.
void detectLocalTeamFromArmor(JNIEnv *env, jobject localPlayer) {
  if (!env || !localPlayer || !lc || !g_localTeam.empty() ||
      g_localName.empty())
    return;
  jclass playerClass = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  jclass armorClass = lc->GetClass("net.minecraft.item.ItemArmor");
  if (!playerClass || !stackClass || !armorClass)
    return;
  jmethodID getArmor = lc->GetMethodID(
      playerClass, "getCurrentArmor", "(I)Lnet/minecraft/item/ItemStack;",
      "func_71124_b", "q", "(I)Lzx;");
  jmethodID getItem = lc->GetMethodID(stackClass, "getItem",
                                      "()Lnet/minecraft/item/Item;",
                                      "func_77973_b", "b", "()Lzw;");
  jmethodID getColor = lc->GetMethodID(
      armorClass, "getColor", "(Lnet/minecraft/item/ItemStack;)I",
      "func_82814_b", "b", "(Lzx;)I");
  if (!getArmor || !getItem || !getColor)
    return;
  // getCurrentArmor's slot order is boots=0, leggings=1, chestplate=2,
  // helmet=3; helmet is checked first since it's the piece already relied
  // on elsewhere for this kind of dye-color detection.
  for (const int slot : {3, 2}) {
    jobject stack = env->CallObjectMethod(localPlayer, getArmor, slot);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      continue;
    }
    if (!stack)
      continue;
    jobject item = env->CallObjectMethod(stack, getItem);
    clearException(env);
    if (item && env->IsInstanceOf(item, armorClass)) {
      const int color = env->CallIntMethod(item, getColor, stack);
      clearException(env);
      const std::string team = closestTeamColor(color);
      if (!team.empty()) {
        setTeamColorSticky(g_localName, team);
        if (item)
          env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
        return;
      }
    }
    if (item)
      env->DeleteLocalRef(item);
    env->DeleteLocalRef(stack);
  }
}

std::vector<PlayerObservation> scanPlayers(JNIEnv *env, jobject world,
                                           jobject localPlayer, double localX,
                                           double localY, double localZ,
                                           VisibilityMode visibilityMode,
                                           float cameraViewDegrees) {
  std::vector<PlayerObservation> result;
  if (!env || !world || !localPlayer || !lc)
    return result;
  jclass worldClass = lc->GetClass("net.minecraft.world.World");
  jclass entityClass = lc->GetClass("net.minecraft.entity.Entity");
  jclass playerClass = lc->GetClass("net.minecraft.entity.player.EntityPlayer");
  if (!worldClass || !entityClass || !playerClass)
    return result;
  jfieldID playersField = lc->GetFieldID(worldClass, "playerEntities",
                                         "Ljava/util/List;", "field_73010_i",
                                         "j");
  if (!playersField)
    return result;
  jobject players = env->GetObjectField(world, playersField);
  clearException(env);
  if (!players)
    return result;
  jclass listClass = env->FindClass("java/util/List");
  jmethodID listSize =
      listClass ? env->GetMethodID(listClass, "size", "()I") : nullptr;
  jmethodID listGet = listClass
                          ? env->GetMethodID(listClass, "get",
                                             "(I)Ljava/lang/Object;")
                          : nullptr;
  if (!listSize || !listGet) {
    clearException(env);
    if (listClass)
      env->DeleteLocalRef(listClass);
    env->DeleteLocalRef(players);
    return result;
  }

  jfieldID posX = lc->GetFieldID(entityClass, "posX", "D", "field_70165_t", "s");
  jfieldID posY = lc->GetFieldID(entityClass, "posY", "D", "field_70163_u", "t");
  jfieldID posZ = lc->GetFieldID(entityClass, "posZ", "D", "field_70161_v", "u");
  jmethodID getId = lc->GetMethodID(entityClass, "getEntityId", "()I",
                                    "func_145782_y", "F");
  jmethodID getName = lc->GetMethodID(entityClass, "getName",
                                      "()Ljava/lang/String;", "func_70005_c_",
                                      "e_");
  jmethodID isAlive = lc->GetMethodID(entityClass, "isEntityAlive", "()Z",
                                      "func_70089_S", "ai");
  jmethodID canSee = lc->GetMethodID(
      playerClass, "canEntityBeSeen", "(Lnet/minecraft/entity/Entity;)Z",
      "func_70685_l", "t", "(Lpk;)Z");
  jmethodID getHeld = lc->GetMethodID(
      playerClass, "getHeldItem", "()Lnet/minecraft/item/ItemStack;",
      "func_70694_bm", "bA", "()Lzx;");
  jmethodID isUsing = lc->GetMethodID(playerClass, "isUsingItem", "()Z",
                                      "func_71039_bw", "bS");
  // Looked up on EntityPlayer rather than EntityOtherPlayerMP. isSpectator is
  // declared on EntityPlayer, and JNI's GetMethodID already searches
  // superclasses, so the subclass bought nothing -- while costing a class
  // lookup that has no obfuscated-name fallback. On a client that ships
  // obfuscated Minecraft classes that lookup returned null and the guard
  // above aborted the whole player scan, which silently disabled every
  // Bedwars player alert.
  jmethodID isSpectator = lc->GetMethodID(
      playerClass, "isSpectator", "()Z", "func_175149_v", "v");

  // One-shot report of what the client's class layout actually gave us.
  // Every lookup here has a deobfuscated name, an SRG name and an obfuscated
  // fallback, and which of the three lands depends entirely on how the client
  // ships Minecraft. When a client remaps something none of the three cover,
  // the feature does not fail loudly -- it just silently reads nothing -- so
  // this line is the difference between "unsupported client" and a guess.
  {
    static bool s_reported = false;
    if (!s_reported) {
      s_reported = true;
      Logger::info("[Bedwars] JNI mapping: world=%d entity=%d player=%d "
                   "players=%d pos=%d id=%d name=%d alive=%d canSee=%d "
                   "held=%d using=%d spectator=%d",
                   worldClass ? 1 : 0, entityClass ? 1 : 0, playerClass ? 1 : 0,
                   playersField ? 1 : 0, (posX && posY && posZ) ? 1 : 0,
                   getId ? 1 : 0, getName ? 1 : 0, isAlive ? 1 : 0,
                   canSee ? 1 : 0, getHeld ? 1 : 0, isUsing ? 1 : 0,
                   isSpectator ? 1 : 0);
    }
  }

  jclass stackClass = lc->GetClass("net.minecraft.item.ItemStack");
  jmethodID hasEffect = stackClass
                            ? lc->GetMethodID(stackClass, "hasEffect", "()Z",
                                              "func_77948_v", "w")
                            : nullptr;
  jfieldID rotationYaw = lc->GetFieldID(entityClass, "rotationYaw", "F",
                                         "field_70177_z", "y");
  jfieldID rotationPitch = lc->GetFieldID(entityClass, "rotationPitch", "F",
                                           "field_70125_A", "z");
  if (!playersField || !posX || !posY || !posZ || !getId || !getName ||
      !isAlive || !getHeld || !isSpectator) {
    static ULONGLONG lastBindingLog = 0;
    const ULONGLONG now = GetTickCount64();
    if (Configuration::isDebugEnabled() &&
        (lastBindingLog == 0 || now - lastBindingLog >= 10000)) {
      lastBindingLog = now;
      Logger::info("[Bedwars] player scan disabled: essential 1.8.9 binding "
                   "unavailable (list=%d pos=%d id=%d name=%d alive=%d "
                   "held=%d spectator=%d)",
                   playersField ? 1 : 0,
                   posX && posY && posZ ? 1 : 0, getId ? 1 : 0,
                   getName ? 1 : 0, isAlive ? 1 : 0, getHeld ? 1 : 0,
                   isSpectator ? 1 : 0);
    }
    env->DeleteLocalRef(listClass);
    env->DeleteLocalRef(players);
    return result;
  }
  const bool cameraMappingKnown = rotationYaw && rotationPitch;
  float localYaw = rotationYaw ? env->GetFloatField(localPlayer, rotationYaw) : 0.0F;
  float localPitch = rotationPitch ? env->GetFloatField(localPlayer, rotationPitch) : 0.0F;
  clearException(env);
  constexpr double pi = 3.14159265358979323846;
  const double yaw = static_cast<double>(localYaw) * pi / 180.0;
  const double pitch = static_cast<double>(localPitch) * pi / 180.0;
  const double forwardX = -std::sin(yaw) * std::cos(pitch);
  const double forwardY = -std::sin(pitch);
  const double forwardZ = std::cos(yaw) * std::cos(pitch);
  const double cameraThreshold = std::cos(
      std::clamp(static_cast<double>(cameraViewDegrees), 30.0, 170.0) *
      0.5 * pi / 180.0);

  const jint count = std::min<jint>(env->CallIntMethod(players, listSize), 128);
  clearException(env);
  result.reserve(static_cast<std::size_t>(std::max<jint>(0, count)));
  for (jint i = 0; i < count; ++i) {
    jobject player = env->CallObjectMethod(players, listGet, i);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      break;
    }
    if (!player)
      continue;
    if (!env->IsInstanceOf(player, playerClass)) {
      env->DeleteLocalRef(player);
      continue;
    }
    PlayerObservation observation;
    observation.localPlayer = env->IsSameObject(player, localPlayer);
    observation.entityId = getId ? env->CallIntMethod(player, getId) : -1;
    clearException(env);
    if (getName) {
      jstring name = static_cast<jstring>(env->CallObjectMethod(player, getName));
      clearException(env);
      observation.identity = fromJavaString(env, name);
      if (name)
        env->DeleteLocalRef(name);
    }
    const double x = posX ? env->GetDoubleField(player, posX) : localX;
    const double y = posY ? env->GetDoubleField(player, posY) : localY;
    const double z = posZ ? env->GetDoubleField(player, posZ) : localZ;
    clearException(env);
    const double dx = x - localX;
    const double dy = y - localY;
    const double dz = z - localZ;
    observation.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (visibilityMode != VisibilityMode::RangeOnly && canSee) {
      observation.lineOfSightKnown = true;
      observation.hasLineOfSight =
          env->CallBooleanMethod(localPlayer, canSee, player) == JNI_TRUE;
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        observation.lineOfSightKnown = false;
        observation.hasLineOfSight = false;
      }
    }
    if (visibilityMode == VisibilityMode::CameraView && cameraMappingKnown &&
        std::isfinite(observation.distance) && observation.distance > 0.001) {
      const double cameraDistance =
          std::sqrt(dx * dx + (dy + 0.9) * (dy + 0.9) + dz * dz);
      if (std::isfinite(cameraDistance) && cameraDistance > 0.001) {
        observation.cameraViewKnown = true;
        const double dot = (dx * forwardX + (dy + 0.9) * forwardY +
                            dz * forwardZ) /
                           cameraDistance;
        observation.insideCameraView =
            std::isfinite(dot) && dot >= cameraThreshold;
      }
    }
    observation.alive = !isAlive || env->CallBooleanMethod(player, isAlive);
    clearException(env);
    // The IsInstanceOf test against EntityOtherPlayerMP is gone: every entry
    // in world.playerEntities is already a player, and the local player is
    // rejected by identity further along, so the check only narrowed a set
    // that was correct without it.
    observation.spectator =
        isSpectator &&
        env->CallBooleanMethod(player, isSpectator) == JNI_TRUE;
    clearException(env);
    bool armorEnchanted = false;
    observation.armor = armorTierFor(env, player, &armorEnchanted);
    observation.armorEnchanted = armorEnchanted;
    jobject held = getHeld ? env->CallObjectMethod(player, getHeld) : nullptr;
    clearException(env);
    if (held) {
      observation.heldItem.typeName = itemName(env, held);
      observation.heldItem.displayName = itemDisplayName(env, held);
      observation.heldItem.metadata = itemMetadata(env, held);
      observation.heldItem.enchanted =
          hasEffect && env->CallBooleanMethod(held, hasEffect) == JNI_TRUE;
      clearException(env);
      env->DeleteLocalRef(held);
    }
    observation.usingItem =
        isUsing && env->CallBooleanMethod(player, isUsing) == JNI_TRUE;
    clearException(env);
    if (!observation.identity.empty()) {
      std::lock_guard<std::mutex> lock(g_statsMutex);
      const auto team = g_playerTeamColor.find(observation.identity);
      if (team != g_playerTeamColor.end()) {
        observation.team = normalizeTeam(team->second);
        observation.teamAuthoritative = observation.team != TeamId::Unknown;
      }
      const TeamId localTeam = normalizeTeam(g_localTeam);
      if (observation.teamAuthoritative && localTeam != TeamId::Unknown) {
        observation.teammateKnown = true;
        observation.teammate = observation.team == localTeam;
      }
    }
    result.push_back(std::move(observation));
    env->DeleteLocalRef(player);
  }
  env->DeleteLocalRef(listClass);
  env->DeleteLocalRef(players);
  return result;
}

std::string formatDuration(Tick milliseconds) {
  const Tick totalSeconds = (milliseconds + 999) / 1000;
  const Tick minutes = totalSeconds / 60;
  const Tick seconds = totalSeconds % 60;
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "%llu:%02llu",
                static_cast<unsigned long long>(minutes),
                static_cast<unsigned long long>(seconds));
  return buffer;
}

} // namespace

Runtime &Runtime::instance() {
  static Runtime runtime;
  return runtime;
}

void Runtime::onChatMessage(const std::string &message) {
  if (message.empty() || m_shuttingDown.load(std::memory_order_acquire))
    return;
  const std::string bounded = message.substr(0, 512);
  std::lock_guard<std::mutex> lock(m_queueMutex);
  if (m_lines.push({QueuedKind::Chat, bounded, GetTickCount64()}))
    m_totalDroppedLines.fetch_add(1, std::memory_order_relaxed);
}

void Runtime::onScoreboardLine(const std::string &line) {
  if (line.empty() || m_shuttingDown.load(std::memory_order_acquire))
    return;
  const std::string bounded = line.substr(0, 512);
  std::lock_guard<std::mutex> lock(m_queueMutex);
  if (m_lines.push({QueuedKind::Scoreboard, bounded, GetTickCount64()}))
    m_totalDroppedLines.fetch_add(1, std::memory_order_relaxed);
}

void Runtime::drainLines(Tick now) {
  std::deque<QueuedLine> lines;
  std::uint64_t dropped = 0;
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    lines = m_lines.take();
    dropped = m_lines.consumeDropped();
  }
  const auto settings = Configuration::get();
  if (!settings.masterEnabled)
    return;
  for (const auto &line : lines) {
    if (line.kind == QueuedKind::Scoreboard) {
      const std::string clean = stripFormatting(line.text);
      const std::string normalized = normalizeText(clean);
      if (const auto map = parseMapScoreboardLine(clean)) {
        const MapHeightResolution height = resolveMapHeight(*map);
        const std::string displayName =
            height.maximumPlacementY ? height.canonicalName : *map;

        // The pre-game scoreboard is the authoritative map source. Preserve
        // it across the expected match-start world swap just like the /map
        // chat response below; otherwise the lifecycle reset clears the name
        // before the scoreboard line disappears. Refreshing the observation
        // timestamp while the line remains visible also keeps the short TTL
        // tied to real, current scoreboard evidence.
        m_announcedMap = displayName;
        m_announcedMapObserved = line.received;

        if (m_mapName != displayName) {
          m_mapName = displayName;
          m_mapHeight = height;
          if (settings.debug)
            Logger::info("[Bedwars] map=%s placementLimit=%d source=scoreboard-%s",
                         m_mapName.c_str(),
                         m_mapHeight.maximumPlacementY.value_or(-1),
                         m_mapHeight.overridden ? "override" :
                         (m_mapHeight.maximumPlacementY ? "built-in" : "unknown"));
        }
      }
      const std::size_t mode = normalized.find("mode:");
      if (mode != std::string::npos) {
        const std::size_t colon = clean.find(':');
        if (colon != std::string::npos)
          m_modeName = clean.substr(colon + 1);
        const std::string lowerMode = normalizeText(m_modeName);
        if (lowerMode.find("solo") != std::string::npos ||
            lowerMode.find("double") != std::string::npos)
          m_teamCount = 8;
        else if (lowerMode.find("3v3") != std::string::npos ||
                 lowerMode.find("4v4v4v4") != std::string::npos)
          m_teamCount = 4;
        else if (lowerMode.find("4v4") != std::string::npos)
          m_teamCount = 2;
      }
      if (const auto event = EventSchedule::parseScoreboardLine(clean)) {
        m_scoreboardEvent = event;
        m_scoreboardEventObserved = line.received;
      }
      continue;
    }

    // Hypixel drops the "Map:" line from the scoreboard when the match starts.
    // `/map` supplies a second server-originated source in chat:
    //
    //   You are currently playing on Gelato
    //
    // This is a fallback for late joins and manual refreshes; the match-start
    // flow itself does not consistently emit the line through the incoming
    // chat hook, so the pre-game scoreboard remains the primary source.
    {
      const std::string plain = stripFormatting(line.text);
      // Lowercased in place rather than through normalizeText, which also
      // collapses runs of whitespace and trims. That shifts every index, so an
      // offset found in the normalized copy does not point at the same
      // character in `plain` -- and Hypixel's chat lines are padded.
      std::string lowered = plain;
      for (char &c : lowered)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      constexpr char kPhrase[] = "currently playing on";
      const std::size_t at = lowered.find(kPhrase);
      if (at != std::string::npos && (g_inHypixelGame || g_inReplay)) {
        const std::string parsed =
            normalizeMapName(plain.substr(at + sizeof(kPhrase) - 1));
        if (!parsed.empty() && parsed.size() <= 64) {
          const MapHeightResolution height = resolveMapHeight(parsed);
          const std::string displayName =
              height.maximumPlacementY ? height.canonicalName : parsed;
          // The announcement commonly precedes the match-start world swap.
          // Record its age instead of the current world identity so that one
          // expected teleport cannot erase it.
          m_announcedMap = displayName;
          m_announcedMapObserved = line.received;
          m_mapName = displayName;
          m_mapHeight = height;
          if (settings.debug)
            Logger::info("[Bedwars] map=%s placementLimit=%d source=chat",
                         m_mapName.c_str(),
                         m_mapHeight.maximumPlacementY.value_or(-1));
        }
      }
    }

    if (m_context.phase() != GamePhase::Active &&
        m_context.phase() != GamePhase::Spectator)
      continue;
    if (const auto destroyed = parseDestroyedBedTeam(line.text)) {
      m_teams.observeBed(*destroyed, BedState::Destroyed, line.received,
                         "server chat");
      if (settings.debug)
        Logger::info("[Bedwars] %s bed marked destroyed from chat",
                     teamName(*destroyed));
    }
    if (const auto victim = parseDeathVictim(line.text)) {
      // A death resets the player's inventory but NOT their armor: armor is a
      // permanent team upgrade in Bedwars, so they respawn wearing exactly
      // what they died in. Our per-player dedup only alerts once per item or
      // tier for the whole match, so the inventory half has to be cleared --
      // otherwise someone alerted for an Invisibility Potion before dying
      // would never alert again after re-buying one. The armor half must be
      // kept, or every single kill would re-announce armor the user was
      // already told about. forgetPlayerLoadout draws exactly that line;
      // forgetPlayer (full wipe) would get the armor side wrong.
      m_players.forgetPlayerLoadout(*victim);
      if (settings.debug)
        Logger::info("[Bedwars] cleared alert history for %s (died)",
                     victim->c_str());
    }
    const auto signal = m_chat.observe(line.text, line.received);
    if (!signal)
      continue;
    if (signal->kind == ChatSignal::Kind::Upgrade &&
        settings.enabled(Module::UpgradeAlerts)) {
      // Carry the tier through. The parser already counts it, and "Protection
      // III" tells you something "Protection" does not -- which is whether it
      // is still worth pushing. Level 1 stays bare because tier-less upgrades
      // like Heal Pool would otherwise read as "Heal Pool I".
      std::string label = signal->label;
      if (signal->level > 1)
        label += " " + std::to_string(signal->level);
      notify("Team Upgrade", label + " purchased by your team", false, true,
             NoticeKind::Important);
    } else if (signal->kind == ChatSignal::Kind::TrapTriggered &&
               settings.enabled(Module::TrapNotifier)) {
      notify("Trap", "Your trap was triggered", true, true);
      m_lastTrapReminder = now;
    } else if (signal->kind == ChatSignal::Kind::TrapQueued &&
               settings.enabled(Module::TrapNotifier)) {
      notify("Trap", "Trap purchase observed", false, false);
      m_lastTrapReminder = 0;
    } else if (signal->kind == ChatSignal::Kind::TrapMissing &&
               settings.enabled(Module::TrapNotifier)) {
      notify("Trap", "No trap is currently queued", true, false);
      m_lastTrapReminder = now;
    }
  }

  if (settings.debug && dropped > 0) {
    Logger::info("[Bedwars] Bounded input queue dropped %llu old lines",
                 static_cast<unsigned long long>(dropped));
  }
  (void)now;
}

void Runtime::tick() {
  if (m_shuttingDown.load(std::memory_order_acquire) || !lc)
    return;
  JNIEnv *env = lc->getEnv();
  if (!env)
    return;
  const Tick now = GetTickCount64();
  const auto settings = Configuration::get();

  jobject world = Mc::theWorld(env);
  jobject player = Mc::thePlayer(env);
  if (player)
    detectLocalTeamFromArmor(env, player);
  if (world) {
    auto globalWorld = static_cast<jobject>(m_worldReference);
    if (!globalWorld || !env->IsSameObject(globalWorld, world)) {
      if (globalWorld)
        env->DeleteGlobalRef(globalWorld);
      m_worldReference = env->NewGlobalRef(world);
      ++m_worldToken;
    }
  } else if (m_worldReference) {
    env->DeleteGlobalRef(static_cast<jobject>(m_worldReference));
    m_worldReference = nullptr;
    ++m_worldToken;
  }

  int entityId = -1;
  float health = 0.0F;
  int dimension = 0;
  bool playerStateValid = false;
  if (player) {
    jclass entityClass = lc->GetClass("net.minecraft.entity.Entity");
    // getHealth is declared on EntityLivingBase but resolved through the
    // player class for the same reason as isSpectator above: GetMethodID
    // walks superclasses, and EntityLivingBase had no obfuscated fallback, so
    // asking for it directly made local health unreadable on obfuscated
    // clients -- which marked the whole player state invalid.
    jclass livingClass =
        lc->GetClass("net.minecraft.entity.player.EntityPlayer");
    bool positionValid = false;
    bool entityIdValid = false;
    bool healthValid = false;
    if (entityClass) {
      jfieldID posX = lc->GetFieldID(entityClass, "posX", "D", "field_70165_t", "s");
      jfieldID posY = lc->GetFieldID(entityClass, "posY", "D", "field_70163_u", "t");
      jfieldID posZ = lc->GetFieldID(entityClass, "posZ", "D", "field_70161_v", "u");
      jmethodID getId = lc->GetMethodID(entityClass, "getEntityId", "()I",
                                        "func_145782_y", "F");
      jfieldID dimensionField = lc->GetFieldID(
          entityClass, "dimension", "I", "field_71093_bK", "am");
      if (posX && posY && posZ) {
        m_playerX = env->GetDoubleField(player, posX);
        m_playerY = env->GetDoubleField(player, posY);
        m_playerZ = env->GetDoubleField(player, posZ);
        positionValid = !env->ExceptionCheck() && std::isfinite(m_playerX) &&
                        std::isfinite(m_playerY) && std::isfinite(m_playerZ);
        clearException(env);
      }
      if (getId) {
        entityId = env->CallIntMethod(player, getId);
        entityIdValid = !env->ExceptionCheck() && entityId >= 0;
        clearException(env);
      }
      if (dimensionField) {
        dimension = env->GetIntField(player, dimensionField);
        clearException(env);
      }
    }
    if (livingClass) {
      jmethodID getHealth = lc->GetMethodID(livingClass, "getHealth", "()F",
                                            "func_110143_aJ", "bn");
      if (getHealth) {
        health = env->CallFloatMethod(player, getHealth);
        healthValid = !env->ExceptionCheck() && std::isfinite(health);
        clearException(env);
      }
    }
    playerStateValid = positionValid && entityIdValid && healthValid;

    // Why the lifecycle is or is not active, in one line. The phase gate
    // needs featureEnabled && worldValid && playerValid && onHypixel &&
    // bedwarsMode && !replay, and playerValid is itself three separate JNI
    // reads. When any single one of those six inputs is false the whole
    // Bedwars feature set sits idle and every counter downstream reads zero,
    // which looks identical to "nothing happened" -- so the inputs have to be
    // visible, not inferred. Rate limited; every field here is cheap.
    if (settings.debug &&
        (m_lastLifecycleLog == 0 || now < m_lastLifecycleLog ||
         now - m_lastLifecycleLog >= 5000)) {
      m_lastLifecycleLog = now;
      Logger::info(
          "[Bedwars] lifecycle inputs: world=%d player=%d pos=%d id=%d "
          "health=%d (hp=%.1f) entityCls=%d playerCls=%d hypixel=%d "
          "bedwars=%d inGame=%d preGame=%d replay=%d",
          world ? 1 : 0, player ? 1 : 0, positionValid ? 1 : 0,
          entityIdValid ? 1 : 0, healthValid ? 1 : 0,
          static_cast<double>(health),
          lc->GetClass("net.minecraft.entity.Entity") ? 1 : 0,
          lc->GetClass("net.minecraft.entity.player.EntityPlayer") ? 1 : 0,
          (g_inHypixelGame || g_inPreGameLobby) ? 1 : 0, g_mode == 0 ? 1 : 0,
          g_inHypixelGame ? 1 : 0, g_inPreGameLobby ? 1 : 0,
          g_inReplay ? 1 : 0);
    }
  }

  SessionObservation observation;
  observation.featureEnabled = settings.masterEnabled;
  observation.worldValid = world != nullptr;
  observation.playerValid = player != nullptr && playerStateValid;
  observation.onHypixel = g_inHypixelGame || g_inPreGameLobby;
  observation.bedwarsMode = g_mode == 0;
  observation.inGame = g_inHypixelGame;
  observation.preGame = g_inPreGameLobby;
  observation.replay = g_inReplay;
  observation.dead = playerStateValid && health <= 0.0F;
  observation.dimension = dimension;
  observation.localEntityId = entityId;
  observation.worldIdentity = m_worldToken;
  observation.mapName = m_mapName;
  observation.modeName = m_modeName;
  observation.localTeam = g_localTeam;
  observation.teamCount = m_teamCount;
  const LifecycleTransition transition = m_context.observe(observation, now);
  if (transition.reset && !transition.preservedGameClock) {
    m_mapName.clear();
    m_modeName.clear();
    m_teamCount = 0;
  }
  // Put a recent announcement back after the lifecycle reset wiped the map.
  // The server often announces the map immediately before the world identity
  // changes, so tying this to the old world loses the only in-game map signal.
  // An explicit game end clears it, and the short lifetime prevents a name
  // from an abandoned/previous match leaking into a later one.
  if (transition.gameEnded ||
      (m_announcedMapObserved != 0 &&
       (now < m_announcedMapObserved ||
        now - m_announcedMapObserved > kMapAnnouncementLifetimeMs))) {
    m_announcedMap.clear();
    m_announcedMapObserved = 0;
  }
  if (transition.reset || transition.gameStarted || transition.gameEnded)
    resetState(transition.reset
                   ? transition.reason.c_str()
                   : (transition.gameStarted ? "game started" : "game ended"));
  // resetState clears the cached height resolution, so restore both the name
  // and its resolved limit only after the lifecycle reset has finished.
  bool restoredRecentMap = false;
  if (!transition.gameEnded && m_mapName.empty() &&
      !m_announcedMap.empty() && m_announcedMapObserved != 0) {
    m_mapName = m_announcedMap;
    restoredRecentMap = true;
  }
  if (!transition.gameEnded && !m_mapName.empty() &&
      (restoredRecentMap || transition.reset || transition.gameStarted)) {
    m_mapHeight = resolveMapHeight(m_mapName);
    if (settings.debug && restoredRecentMap)
      Logger::info("[Bedwars] map=%s placementLimit=%d source=recent-server",
                   m_mapName.c_str(),
                   m_mapHeight.maximumPlacementY.value_or(-1));
  }
  if (settings.debug && (transition.reset || transition.gameStarted ||
                         transition.gameEnded)) {
    Logger::info("[Bedwars] lifecycle reset=%d start=%d end=%d reason=%s",
                 transition.reset ? 1 : 0, transition.gameStarted ? 1 : 0,
                 transition.gameEnded ? 1 : 0, transition.reason.c_str());
  }

  drainLines(now);
  const bool active = m_context.phase() == GamePhase::Active ||
                      m_context.phase() == GamePhase::Spectator;

  const bool needResources = active &&
                             (settings.enabled(Module::ResourceTracker) ||
                              settings.enabled(Module::PickupAlerts) ||
                              settings.enabled(Module::ShopHelper));
  if (!needResources) {
    m_resources.reset();
    m_latestResources = {};
    m_lastInventoryScan = 0;
  }

  if (needResources && player &&
      (m_lastInventoryScan == 0 || now - m_lastInventoryScan >= 250)) {
    m_lastInventoryScan = now;
    m_latestResources = scanInventory(env, player);
    const auto deltas = m_resources.observe(m_latestResources);
    std::vector<std::string> resourceMessages;
    for (const auto &delta : deltas) {
      const std::size_t index = static_cast<std::size_t>(delta.resource);
      if (index >= settings.resources.size() || !settings.resources[index])
        continue;
      if (settings.enabled(Module::ResourceTracker) ||
          settings.enabled(Module::PickupAlerts)) {
        const std::string message = "+" + std::to_string(delta.amount) + " " +
                                    resourceName(delta.resource);
        if (Configuration::Fixed::kStackedResourceAlerts) {
          notify(delta.enderChest ? "Ender Chest" : "Resources", message,
                 false, settings.enabled(Module::PickupAlerts));
        } else {
          resourceMessages.push_back(message);
        }
      }
    }
    if (!Configuration::Fixed::kStackedResourceAlerts &&
        !resourceMessages.empty()) {
      std::string combined;
      for (const auto &message : resourceMessages) {
        if (!combined.empty())
          combined += ", ";
        combined += message;
      }
      notify("Resources", combined, false,
             settings.enabled(Module::PickupAlerts));
    }
  }

  const bool needPlayers = active &&
                           (settings.enabled(Module::ArmorAlerts) ||
                            settings.enabled(Module::UpgradeAlerts) ||
                            settings.enabled(Module::ConsumeAlerts) ||
                            settings.enabled(Module::ItemAlerts));
  if (!needPlayers) {
    m_players.reset();
    m_lastPlayerScan = 0;
    m_lastScannedPlayers = 0;
  }
  if (needPlayers && world && player &&
      (m_lastPlayerScan == 0 || now - m_lastPlayerScan >= 300)) {
    m_lastPlayerScan = now;
    PlayerAlertOptions options;
    options.armor = settings.enabled(Module::ArmorAlerts);
    options.upgrades = settings.enabled(Module::UpgradeAlerts);
    options.consumes = settings.enabled(Module::ConsumeAlerts);
    options.items = settings.enabled(Module::ItemAlerts);
    options.ignoreOwnTeam = settings.ignoreOwnTeam;
    options.maximumDistance = settings.playerAlertRange;
    options.cooldownMs =
        static_cast<Tick>(Configuration::Fixed::kPlayerAlertCooldownMs);
    options.visibility = settings.visibilityMode;
    options.itemEnabled = settings.itemAlerts;
    const auto scanned = scanPlayers(env, world, player, m_playerX, m_playerY,
                                     m_playerZ, settings.visibilityMode,
                                     Configuration::Fixed::kCameraViewDegrees);
    m_lastScannedPlayers = scanned.size();

    // Raw held-item telemetry. When an alert does not fire there is no way to
    // tell "the item was never in their hand" apart from "it was, and the
    // classifier did not recognise the stack" -- the summary counters only
    // aggregate, and an unrecognised item and an empty hand both leave every
    // counter at zero. One line per visible enemy with the stack exactly as
    // the client sees it, next to what it classified as, settles that
    // immediately. Debug-gated and rate limited, so it costs nothing normally.
    if (settings.debug &&
        (m_lastItemDump == 0 || now < m_lastItemDump ||
         now - m_lastItemDump >= 3000)) {
      m_lastItemDump = now;
      for (const auto &seen : scanned) {
        if (seen.localPlayer ||
            (settings.ignoreOwnTeam && seen.teammateKnown && seen.teammate))
          continue;
        const ImportantItem classified = classifyImportantItem(seen.heldItem);
        Logger::info(
            "[Bedwars] held %s dist=%.1f alive=%d spec=%d type='%s' name='%s' "
            "meta=%d ench=%d using=%d los=%d/%d -> %s",
            seen.identity.c_str(), seen.distance, seen.alive ? 1 : 0,
            seen.spectator ? 1 : 0, seen.heldItem.typeName.c_str(),
            seen.heldItem.displayName.c_str(), seen.heldItem.metadata,
            seen.heldItem.enchanted ? 1 : 0, seen.usingItem ? 1 : 0,
            seen.lineOfSightKnown ? 1 : 0, seen.hasLineOfSight ? 1 : 0,
            classified == ImportantItem::None ? "(unclassified)"
                                              : importantItemName(classified));
      }
    }

    const auto alerts = m_players.observe(scanned, options, now, &m_teams);
    m_teams.expire(now);
    for (const auto &alert : alerts) {
      const bool warning = alert.kind == PlayerAlert::Kind::KnockbackStick;
      notify(alert.kind == PlayerAlert::Kind::Upgrade ? "Team Upgrade"
                                                     : "Player Alert",
             alert.text, warning, true,
             alert.kind == PlayerAlert::Kind::Upgrade ? NoticeKind::Important
                                                       : NoticeKind::Player,
             alert.segments);
    }
  }

  if (active && settings.enabled(Module::TrapNotifier) &&
      m_chat.trapState() == TrapState::Missing) {
    const Tick interval = static_cast<Tick>(settings.trapReminderSeconds) * 1000;
    if (m_context.elapsed(now) >= interval &&
        (m_lastTrapReminder == 0 || now - m_lastTrapReminder >= interval)) {
      m_lastTrapReminder = now;
      notify("Trap", "No trap is currently queued", true, false);
    }
  }

  rebuildSnapshot(now, m_playerY);
  logSummary(now, settings, m_lastScannedPlayers,
             active ? "active" : "lifecycle inactive");
  if (player)
    env->DeleteLocalRef(player);
  if (world)
    env->DeleteLocalRef(world);
}

void Runtime::rebuildSnapshot(Tick now, double playerY) {
  const auto settings = Configuration::get();
  RenderSnapshot snapshot;
  snapshot.active = m_context.phase() == GamePhase::Active ||
                    m_context.phase() == GamePhase::Spectator;
  snapshot.generation = m_context.generation();
  snapshot.mapName = m_mapName;
  switch (m_context.phase()) {
  case GamePhase::Inactive:
    snapshot.lifecycleStatus = !settings.masterEnabled ? "Disabled" : "Waiting for Hypixel";
    break;
  case GamePhase::Lobby: snapshot.lifecycleStatus = "In Bedwars lobby"; break;
  case GamePhase::PreGame: snapshot.lifecycleStatus = "In Bedwars lobby"; break;
  case GamePhase::Active: snapshot.lifecycleStatus = "Active in match"; break;
  case GamePhase::Spectator: snapshot.lifecycleStatus = "Active (spectating)"; break;
  case GamePhase::PostGame: snapshot.lifecycleStatus = "Post-game"; break;
  }
  if (m_context.lastObservation().replay)
    snapshot.lifecycleStatus = "In replay";
  if (!snapshot.active) {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_snapshot = std::move(snapshot);
    return;
  }

  if (settings.enabled(Module::EventTimers)) {
    // The scoreboard reading is a COUNTDOWN, so it keeps being usable long
    // after it was taken -- subtracting the elapsed time is all it needs. It
    // used to be thrown away after two seconds, which meant any hiccup in
    // scoreboard ingestion longer than that flipped a perfectly good timer to
    // "Timing unknown (join observed late)" while the scoreboard on screen was
    // still counting down. It is now kept until its own countdown runs out.
    if (m_scoreboardEvent && now >= m_scoreboardEventObserved &&
        now - m_scoreboardEventObserved < m_scoreboardEvent->remainingMs) {
      const Tick age = now - m_scoreboardEventObserved;
      const Tick remaining = age >= m_scoreboardEvent->remainingMs
                                 ? 0
                                 : m_scoreboardEvent->remainingMs - age;
      snapshot.timerLines.push_back(m_scoreboardEvent->label + "  " +
                                    formatDuration(remaining));
      snapshot.timerUrgency = remaining <= 60000 ? 2 :
                              remaining <= 180000 ? 1 : 0;
    } else if (m_context.gameClockKnown()) {
      for (const auto &event : EventSchedule::countdowns(
               m_context.elapsed(now), settings.onlyNextEvent)) {
        if (snapshot.timerLines.empty())
          snapshot.timerUrgency = event.remainingMs <= 60000 ? 2 :
                                  event.remainingMs <= 180000 ? 1 : 0;
        snapshot.timerLines.push_back(event.label + "  " +
                                      formatDuration(event.remainingMs));
      }
    } else {
      snapshot.timerLines.push_back("Timing unknown (join observed late)");
    }
  }

  if (settings.enabled(Module::HeightOverlay)) {
    // The table lookup is NOT redone here. resolveMapHeight walks 110-odd
    // entries and allocates a lowered copy of each name it compares, and this
    // function runs every frame -- for an answer that only changes when the
    // scoreboard's map line changes, which is where m_mapHeight is filled.
    // The manual override is applied to a local copy so that lowering the
    // slider back to zero restores the detected value instead of having
    // overwritten it.
    MapHeightResolution height = m_mapHeight;
    if (!height.maximumPlacementY && settings.heightLimitOverride > 0) {
      height.maximumPlacementY = settings.heightLimitOverride;
      height.maximumPlayerY = settings.heightLimitOverride + 1;
      height.overridden = true;
    }
    const int currentY = std::isfinite(playerY)
                             ? static_cast<int>(std::floor(playerY))
                             : 0;
    // Reads maximumPlayerY, not maximumPlacementY + 1. The table carries both
    // -- the player-feet ceiling and the highest placeable block one below it
    // -- so deriving one from the other here would duplicate a fact and drift
    // the moment somebody corrects a single row.
    const std::string ceiling =
        height.maximumPlayerY ? std::to_string(*height.maximumPlayerY)
                              : std::string("?");
    // Counted against the same ceiling that is displayed, so the number and
    // the urgency colour can never disagree with each other.
    const std::string left =
        height.maximumPlayerY
            ? std::to_string(std::max(0, *height.maximumPlayerY - currentY))
            : std::string("?");
    switch (settings.heightDisplay) {
    case HeightDisplay::RatioRemaining:
      snapshot.heightLine =
          std::to_string(currentY) + "/" + ceiling + "  " + left;
      break;
    case HeightDisplay::Remaining:
      snapshot.heightLine = left;
      break;
    case HeightDisplay::Limit:
      snapshot.heightLine = ceiling;
      break;
    default:
      snapshot.heightLine = std::to_string(currentY) + "/" + ceiling;
      break;
    }
    if (height.maximumPlacementY)
      snapshot.maximumPlacementY = *height.maximumPlacementY;
    if (height.maximumPlayerY) {
      const int remaining = std::max(0, *height.maximumPlayerY - currentY);
      snapshot.heightUrgency = remaining <= 5 ? 2 : remaining <= 15 ? 1 : 0;
    }
  }

  if (settings.enabled(Module::UpgradeHud)) {
    const UpgradeState &upgrade = m_chat.upgrades();
    auto knownLevel = [](int level, bool known) {
      return known ? std::to_string(level) : std::string("?");
    };
    snapshot.upgradeLines.push_back(
        std::string(Configuration::Fixed::kShortUpgradeLabels ? "Sharp "
                                                              : "Sharpness ") +
        knownLevel(upgrade.sharpness, upgrade.sharpnessKnown));
    snapshot.upgradeLines.push_back(
        std::string(Configuration::Fixed::kShortUpgradeLabels
                        ? "Prot "
                        : "Protection ") +
        knownLevel(upgrade.protection, upgrade.protectionKnown));
    snapshot.upgradeLines.push_back(
        "Forge " + knownLevel(upgrade.forge, upgrade.forgeKnown));
    snapshot.upgradeLines.push_back(
        "Haste " + knownLevel(upgrade.haste, upgrade.hasteKnown));
    snapshot.upgradeLines.push_back(
        std::string(Configuration::Fixed::kShortUpgradeLabels
                        ? "Feather "
                        : "Feather Falling ") +
        knownLevel(upgrade.featherFalling, upgrade.featherFallingKnown));
    if (upgrade.healPool)
      snapshot.upgradeLines.push_back("Heal Pool");
    if (upgrade.dragonBuff)
      snapshot.upgradeLines.push_back("Dragon Buff");
    snapshot.upgradeLines.push_back(
        m_chat.trapState() == TrapState::Queued
            ? "Trap queued"
            : m_chat.trapState() == TrapState::Missing ? "Trap missing"
                                                       : "Trap ?");
  }

  if (settings.enabled(Module::ResourceTracker) &&
      m_latestResources.inventoryValid) {
    for (std::size_t i = 0; i < kResourceCount; ++i) {
      if (settings.resources[i])
        snapshot.resourceLines.push_back(
            std::string(resourceName(static_cast<Resource>(i))) + " " +
            std::to_string(m_latestResources.inventory[i]));
    }
  }

  std::lock_guard<std::mutex> lock(m_snapshotMutex);
  m_snapshot = std::move(snapshot);
}

namespace {

// Maps an overlay segment colour back to the Minecraft formatting code that
// renders the same colour in chat. Alert colours are all chosen from
// Minecraft's own sixteen-colour palette precisely so this mapping is exact
// and the two surfaces cannot drift apart; anything outside it is body text.
const char *argbToFormattingCode(std::uint32_t argb) {
  return formattingCodeForArgb(argb);
}

// Renders an alert as a single chat line: an [OVSON] tag, then the alert
// title, then the message. Segment colours are reused where the alert
// provides them so a player's name keeps their team colour in chat exactly as
// it does on the overlay; alerts without segments fall back to plain white.
std::string chatLine(const std::string &title, const std::string &message,
                     const std::vector<MessageSegment> &segments,
                     bool warning) {
  const char *S = "\xC2\xA7";
  std::string line = std::string(S) + "0[" + S + "r" + S + "cO" + S + "6V" + S +
                     "eS" + S + "aO" + S + "bN" + S + "0]" + S + "r ";
  line += std::string(S) + (warning ? "c" : "7") + title + S + "8: " + S + "r";
  if (segments.empty()) {
    line += std::string(S) + "f" + message;
    return line;
  }
  for (const auto &segment : segments)
    line += std::string(S) + argbToFormattingCode(segment.argb) + segment.text;
  return line;
}

} // namespace

void Runtime::notify(const std::string &title, const std::string &message,
                     bool warning, bool playSound, NoticeKind kind,
                     const std::vector<MessageSegment> &segments) {
  const auto settings = Configuration::get();
  float duration = Configuration::Fixed::kDefaultNotificationSeconds;
  switch (kind) {
  case NoticeKind::Important:
    duration = Configuration::Fixed::kImportantNotificationSeconds; break;
  case NoticeKind::Player:
    duration = Configuration::Fixed::kPlayerNotificationSeconds; break;
  case NoticeKind::Warning:
    duration = Configuration::Fixed::kWarningNotificationSeconds; break;
  default: break;
  }
  if (warning)
    duration = Configuration::Fixed::kWarningNotificationSeconds;
  const AlertOutput output = settings.alertOutput;
  const bool toOverlay =
      output == AlertOutput::Overlay || output == AlertOutput::Both;
  const bool toChat =
      output == AlertOutput::Chat || output == AlertOutput::Both;

  if (toOverlay) {
    if (segments.empty()) {
      Render::NotificationManager::getInstance()->add(
          title, message, warning ? Render::NotificationType::Warning
                                  : Render::NotificationType::Info,
          duration,
          static_cast<std::size_t>(
              Configuration::Fixed::kMaximumVisibleNotifications));
    } else {
      std::vector<Render::NotificationSegment> rich;
      rich.reserve(segments.size());
      for (const auto &segment : segments)
        rich.push_back({segment.text, segment.argb});
      Render::NotificationManager::getInstance()->addRich(
          title, rich, warning ? Render::NotificationType::Warning
                               : Render::NotificationType::Info,
          duration,
          static_cast<std::size_t>(
              Configuration::Fixed::kMaximumVisibleNotifications));
    }
  }

  if (toChat)
    ChatSDK::showClientMessage(chatLine(title, message, segments, warning));
  if (settings.debug)
    // Log the message itself, not just the title. Every Bedwars alert shares
    // one of two titles, so a log of titles alone cannot answer "did the
    // Ender Pearl alert fire?" -- which is the only question anyone ever
    // reads this line to answer.
    Logger::info("[Bedwars] notification title=%s text=\"%s\" duration=%.1fs "
                 "queueLimit=%d",
                 title.c_str(), message.c_str(), duration,
                 Configuration::Fixed::kMaximumVisibleNotifications);
  if (!playSound || !settings.sounds || !lc)
    return;
  JNIEnv *env = lc->getEnv();
  jobject player = env ? Mc::thePlayer(env) : nullptr;
  if (!env || !player)
    return;
  jclass entityClass = lc->GetClass("net.minecraft.entity.Entity");
  jmethodID sound = entityClass
                        ? lc->GetMethodID(entityClass, "playSound",
                                          "(Ljava/lang/String;FF)V",
                                          "func_85030_a", "a")
                        : nullptr;
  if (sound) {
    jstring name = env->NewStringUTF("random.orb");
    if (name) {
      env->CallVoidMethod(player, sound, name, 0.35F, 1.15F);
      clearException(env);
      env->DeleteLocalRef(name);
    }
  }
  env->DeleteLocalRef(player);
}

void Runtime::logSummary(Tick now, const Configuration::Settings &settings,
                         std::size_t scannedPlayers,
                         const char *overlayReason) {
  if (!settings.debug ||
      (m_lastDiagnosticSummary != 0 && now >= m_lastDiagnosticSummary &&
       now - m_lastDiagnosticSummary < 5000))
    return;
  m_lastDiagnosticSummary = now;
  const auto &reject = m_players.rejectionCounts();
  const auto &playerDiag = m_players.diagnostics();
  Logger::info(
      "[Bedwars] summary master=%d phase=%d hypixel=%d mode=%d replay=%d "
      "localEntity=%d localTeam=%s map=%s limit=%d players=%d local=%d "
      "teammate=%d dead=%d spectator=%d range=%d los=%d camera=%d "
      "visibilityUnknown=%d invalidIdentity=%d emitted=%d cooldown=%d "
      "swordGlint=%d sharpDedupe=%d potions=%d potionDedupe=%d kb=%d "
      "kbDedupe=%d unknownItems=%d ignoredItems=%d itemDedupe=%d "
      "resourcesValid=%d overlay=%s "
      "modules=%d%d%d%d%d%d%d%d%d%d%d "
      "hud=%d%d%d%d queueDrops=%llu",
      settings.masterEnabled ? 1 : 0, static_cast<int>(m_context.phase()),
      m_context.lastObservation().onHypixel ? 1 : 0,
      m_context.lastObservation().bedwarsMode ? 1 : 0,
      m_context.lastObservation().replay ? 1 : 0,
      m_context.lastObservation().localEntityId,
      m_context.lastObservation().localTeam.empty()
          ? "Unknown"
          : m_context.lastObservation().localTeam.c_str(),
      m_mapName.empty() ? "Unknown" : m_mapName.c_str(),
      m_mapHeight.maximumPlacementY.value_or(-1),
      static_cast<int>(scannedPlayers),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::LocalPlayer)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::Teammate)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::Dead)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::Spectator)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::OutOfRange)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::NoLineOfSight)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::OutsideCameraView)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::UnknownVisibility)]),
      static_cast<int>(reject[static_cast<std::size_t>(PlayerRejectReason::InvalidIdentity)]),
      static_cast<int>(playerDiag.emitted),
      static_cast<int>(playerDiag.cooldownRejected),
      static_cast<int>(playerDiag.observedSwordGlints),
      static_cast<int>(playerDiag.sharpnessDuplicates),
      static_cast<int>(playerDiag.classifiedPotions),
      static_cast<int>(playerDiag.potionDuplicates),
      static_cast<int>(playerDiag.classifiedKnockback),
      static_cast<int>(playerDiag.knockbackDuplicates),
      static_cast<int>(playerDiag.unknownItems),
      static_cast<int>(playerDiag.ignoredItems),
      static_cast<int>(playerDiag.itemDuplicates),
      m_latestResources.inventoryValid ? 1 : 0,
      overlayReason ? overlayReason : "unknown",
      settings.enabled(Module::EventTimers) ? 1 : 0,
      settings.enabled(Module::ShopHelper) ? 1 : 0,
      settings.enabled(Module::HeightOverlay) ? 1 : 0,
      settings.enabled(Module::UpgradeAlerts) ? 1 : 0,
      settings.enabled(Module::ConsumeAlerts) ? 1 : 0,
      settings.enabled(Module::PickupAlerts) ? 1 : 0,
      settings.enabled(Module::ArmorAlerts) ? 1 : 0,
      settings.enabled(Module::TrapNotifier) ? 1 : 0,
      settings.enabled(Module::ResourceTracker) ? 1 : 0,
      settings.enabled(Module::ItemAlerts) ? 1 : 0,
      settings.enabled(Module::UpgradeHud) ? 1 : 0,
      settings.hud[0].visible ? 1 : 0, settings.hud[1].visible ? 1 : 0,
      settings.hud[2].visible ? 1 : 0, settings.hud[3].visible ? 1 : 0,
      static_cast<unsigned long long>(m_totalDroppedLines.load()));

  std::string rejectionDetails;
  const auto &details = m_players.rejectionDetails();
  for (std::size_t i = 0; i < details.size() && i < 8; ++i) {
    if (!rejectionDetails.empty()) rejectionDetails += ", ";
    rejectionDetails += details[i].identity.empty()
                            ? "entity#" + std::to_string(details[i].entityId)
                            : details[i].identity;
    rejectionDetails += ":";
    rejectionDetails += playerRejectReasonName(details[i].reason);
  }
  if (rejectionDetails.empty()) rejectionDetails = "none";
  if (rejectionDetails != m_lastRejectionDetails) {
    m_lastRejectionDetails = rejectionDetails;
    Logger::info("[Bedwars] player rejection details: %s",
                 rejectionDetails.c_str());
  }
}

void Runtime::resetState(const char *reason) {
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_lines.clear();
  }
  m_totalDroppedLines.store(0, std::memory_order_release);
  m_resources.reset();
  m_chat.reset();
  m_players.reset();
  m_teams.reset();
  m_latestResources = {};
  m_scoreboardEvent.reset();
  m_scoreboardEventObserved = 0;
  m_lastInventoryScan = 0;
  m_lastPlayerScan = 0;
  m_lastTrapReminder = 0;
  m_lastScannedPlayers = 0;
  m_mapHeight = {};
  m_lastRejectionDetails.clear();
  if (Configuration::isDebugEnabled())
    Logger::info("[Bedwars] state reset: %s", reason ? reason : "unknown");
}

void Runtime::reset(const char *reason) {
  resetState(reason);
  m_context.reset(reason);
  m_mapName.clear();
  m_announcedMap.clear();
  m_announcedMapObserved = 0;
  m_modeName.clear();
  m_teamCount = 0;
  std::lock_guard<std::mutex> lock(m_snapshotMutex);
  m_snapshot = {};
}

void Runtime::shutdown() {
  m_shuttingDown.store(true, std::memory_order_release);
  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_lines.clear();
  }
  if (m_worldReference && lc) {
    if (JNIEnv *env = lc->getEnv())
      env->DeleteGlobalRef(static_cast<jobject>(m_worldReference));
  }
  m_worldReference = nullptr;
  reset("shutdown");
}

RenderSnapshot Runtime::snapshot() const {
  std::lock_guard<std::mutex> lock(m_snapshotMutex);
  return m_snapshot;
}

} // namespace OVson::Bedwars
