package net.ovson.api.packet;
public final class PacketHelper {
    private PacketHelper() {}
    public static native String getPacketType(Object packet);
    public static native double getPacketX(Object packet);
    public static native double getPacketY(Object packet);
    public static native double getPacketZ(Object packet);
    public static native float getPacketYaw(Object packet);
    public static native float getPacketPitch(Object packet);
    public static native boolean getPacketOnGround(Object packet);
    
    public static native int getVelocityEntityId(Object packet);
    public static native double getVelocityX(Object packet);
    public static native double getVelocityY(Object packet);
    public static native double getVelocityZ(Object packet);
    
    public static native double getPosLookX(Object packet);
    public static native double getPosLookY(Object packet);
    public static native double getPosLookZ(Object packet);
    public static native float getPosLookYaw(Object packet);
    public static native float getPosLookPitch(Object packet);
    
    public static native String getChatMessage(Object packet);
    public static native int getChatType(Object packet);
    
    public static native Object getPacketField(Object packet, String fieldName);
    public static native void setPacketField(Object packet, String fieldName, Object value);
}
