#pragma once
#include <jni.h>

class PacketHook {
public:
    static void update();
    static void uninstall();
};
