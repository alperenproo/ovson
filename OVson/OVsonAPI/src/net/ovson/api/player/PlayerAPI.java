package net.ovson.api.player;
import net.ovson.api.model.*;
import java.util.List;
public final class PlayerAPI {
    private PlayerAPI() {}
    public static native double getX();
    public static native double getY();
    public static native double getZ();
    public static native float getHealth();
    public static native void setMotion(double x, double y, double z);
    public static native void swingItem();
    public static native boolean onGround();
    
    public static native Vec3 getMotion();
    public static native void setSpeed(double speed);
    public static native void multiplyMotion(double multiplier);
    public static native float getForward();
    public static native void setForward(float forward);
    public static native float getStrafe();
    public static native void setStrafe(float strafe);
    public static native boolean isMoving();
    public static native boolean isDiagonal();
    public static native void jump();
    public static native void setJumping(boolean jumping);
    public static native boolean isJumping();
    public static native void setSneaking(boolean sneaking);
    public static native boolean isSneaking();
    public static native void setSprinting(boolean sprinting);
    public static native boolean isSprinting();
    
    public static native float getYaw();
    public static native float getPitch();
    public static native void setYaw(float yaw);
    public static native void setPitch(float pitch);
    public static native void setRotations(float yaw, float pitch);
    public static native float getServerYaw();
    public static native float getServerPitch();
    public static native float[] getRotationsToEntity(Entity entity);
    public static native float[] getRotationsToBlock(Vec3 vec);
    public static native void enableMovementFix();
    public static native void disableMovementFix();
    public static native boolean isMovementFixActive();
    
    public static native RaycastResult raycastBlock(double distance);
    public static native RaycastResult raycastBlock(double distance, float yaw, float pitch);
    public static native RaycastResult raycastEntity(double distance);
    public static native RaycastResult raycastEntity(double distance, float yaw, float pitch);
    
    public static native void attack(Entity entity);
    public static native void swingReset();
    public static native boolean canPlaceBlock(Vec3 pos, String block);
    public static native void placeBlock(Vec3 pos, String block, Vec3 direction);
    public static native void clickBlock(Vec3 pos, String action);
    
    public static native void sendPacket(Object packet);
    public static native void sendPacketNoEvent(Object packet);
    public static native String getServerIP();
    public static native int getPing();
    public static native void disconnect();
    
    public static native boolean isCreative();
    public static native boolean isSpectator();
    public static native boolean isFlying();
    public static native void setFlying(boolean flying);
    public static native boolean allowFlying();
    
    public static native void setTimer(float timer);
    public static native float getTimer();
    
    public static native int getFPS();
    public static native int[] getDisplaySize();
    public static native String getUsername();
    public static native String getUUID();
    public static native long nanoTime();
    public static native Entity getLocalPlayer();
    
    public static native void removePotionEffect(int id);
    public static native boolean hasPotionEffect(int id);
    public static native int getPotionAmplifier(int id);
    
    public static native void addFriend(String name);
    public static native void removeFriend(String name);
    public static native boolean isFriend(String name);
    public static native List<String> getFriends();
    public static native void addEnemy(String name);
    public static native void removeEnemy(String name);
    public static native boolean isEnemy(String name);
}
