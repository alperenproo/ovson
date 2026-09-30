package net.ovson.api.model;

public class PlayerState {
    public double x;
    public double y;
    public double z;
    public float yaw;
    public float pitch;
    public boolean onGround;
    public boolean isSprinting;
    public boolean isSneaking;

    public PlayerState(double x, double y, double z, float yaw, float pitch, boolean onGround, boolean isSprinting, boolean isSneaking) {
        this.x = x;
        this.y = y;
        this.z = z;
        this.yaw = yaw;
        this.pitch = pitch;
        this.onGround = onGround;
        this.isSprinting = isSprinting;
        this.isSneaking = isSneaking;
    }
}
