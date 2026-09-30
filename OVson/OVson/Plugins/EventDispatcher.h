#pragma once
#include <jni.h>
#include <string>

namespace EventDispatcher {
    // Call from C++ hooks to fire Java events
    void postRender2DEvent(float partialTicks);
    void postRender3DEvent(float partialTicks);
    void postTickEvent();
    void postChatReceivedEvent(const std::string& message);
    bool postChatSendEvent(const std::string& message); // returns true if cancelled
    void postPlayerJoinEvent(const std::string& playerName);
    bool postPacketSendEvent(jobject packet, const std::string& packetName);
    bool postPacketReceiveEvent(jobject packet, const std::string& packetName);
    bool postAttackEvent(int targetEntityId);
    void postUpdateEvent(bool pre, float yaw, float pitch, bool onGround);
    void postPreMotionEvent(double x, double y, double z, float yaw, float pitch, bool ground, bool sprint, bool sneak);
    void postPostMotionEvent();
    bool postKeyPressEvent(char character, int keyCode);
    void postKeyEvent(const std::string& name, int code, bool pressed, bool inGui);
    bool postMouseEvent(int button, bool pressed, int x, int y, int scroll);
    bool postGuiOpenEvent(const std::string& screenName, bool opening);
    void postGuiCloseEvent(const std::string& screenName);
    void postDisconnectEvent(const std::string& reason);
    void postAnticheatFlagEvent(const std::string& playerName);
    
    // Lifecycle event class/constructor refs on init
    void initialize();
    void shutdown();
}
