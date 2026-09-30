#pragma once

#include <cstdint>
#include <jni.h>
#include <string>

namespace BlockHitSound {

enum class ServerSignalKind : int {
  Hurt = 1,
  Swing = 2,
  Velocity = 3,
  Health = 4,
  Respawn = 5,
  Disconnect = 6,
  Explosion = 7,
};

void enqueueServerSignal(ServerSignalKind kind, int entityId, int data1,
                         int data2, int data3, float value1, float value2,
                         float value3, std::uint64_t receivedAtMs);

void setCallbackAcceptance(bool accepting);
void requestCustomSoundReload();
void requestPreview();
void requestSelectNextCustomSound();
bool openSoundsDirectory();
std::string getSoundsDirectory();
void update(JNIEnv *env);
void shutdown(JNIEnv *env);

} // namespace BlockHitSound
