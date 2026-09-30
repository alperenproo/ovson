package net.ovson.api.packet;
public final class PacketFactory {
    private PacketFactory() {}
    public static native Object createChatPacket(String message);
    public static native Object createUseEntityPacket(int entityId, String action);
    public static native Object createPlayerPacket(boolean onGround);
    public static native Object createPositionPacket(double x, double y, double z, boolean onGround);
    public static native Object createRotationPacket(float yaw, float pitch, boolean onGround);
    public static native Object createPosRotPacket(double x, double y, double z, float yaw, float pitch, boolean onGround);
    public static native Object createDiggingPacket(int action, int x, int y, int z, int facing);
    public static native Object createBlockPlacePacket(int x, int y, int z, int facing);
    public static native Object createHeldItemPacket(int slot);
    public static native Object createAnimationPacket();
    public static native Object createEntityActionPacket(int actionId);
    public static native Object createCloseWindowPacket(int windowId);
    public static native Object createAbilitiesPacket();
}
