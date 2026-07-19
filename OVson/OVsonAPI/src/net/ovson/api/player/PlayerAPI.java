package net.ovson.api.player;

public final class PlayerAPI {
    private PlayerAPI() {}

    /**
     * Gets the local player's X coordinate.
     */
    public static native double getX();

    /**
     * Gets the local player's Y coordinate.
     */
    public static native double getY();

    /**
     * Gets the local player's Z coordinate.
     */
    public static native double getZ();

    /**
     * Gets the local player's health (0-20).
     */
    public static native float getHealth();

    /**
     * Sets the local player's motion (velocity).
     */
    public static native void setMotion(double motionX, double motionY, double motionZ);

    /**
     * Simulates a left-click (swing item).
     */
    public static native void swingItem();

    /**
     * Returns true if the player is currently on the ground.
     */
    public static native boolean onGround();
}
