#pragma once

#include <cstdint>
#include <jni.h>
#include <string>

namespace BlockHitSound {

// These values are shared with PacketFilterHook.java. Only packets received by
// the Netty inbound handler are allowed into this detector.
enum class ServerSignalKind : int {
  Hurt = 1,
  Swing = 2,
  Velocity = 3,
  Health = 4,
  Respawn = 5,
  Disconnect = 6,
  Explosion = 7,
};

// Called on Netty's channel thread. This only copies primitive packet data into
// a bounded queue; all Minecraft/JNI inspection and sound playback happen later
// on the render thread in update().
void enqueueServerSignal(ServerSignalKind kind, int entityId, int data1,
                         int data2, int data3, float value1, float value2,
                         float value3, std::uint64_t receivedAtMs);

// Disables the Netty callback path before pipeline removal during unload.
void setCallbackAcceptance(bool accepting);
void requestCustomSoundReload();
void requestPreview();
void requestSelectNextCustomSound();
bool openSoundsDirectory();
std::string getSoundsDirectory();
void update(JNIEnv *env);
void shutdown(JNIEnv *env);

} // namespace BlockHitSound
