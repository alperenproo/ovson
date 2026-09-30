package net.ovson.api.player;

public class SimulatedPlayer {
    public double x, y, z;
    public double motionX, motionY, motionZ;
    public float yaw, pitch;
    public boolean onGround;
    public boolean isSprinting;
    public boolean isSneaking;

    public SimulatedPlayer(double x, double y, double z, float yaw, float pitch) {
        this.x = x;
        this.y = y;
        this.z = z;
        this.yaw = yaw;
        this.pitch = pitch;
    }

    public void tick() {
        // Simple gravity and friction simulation stub
        motionY -= 0.08;
        motionX *= 0.91;
        motionZ *= 0.91;
        
        y += motionY;
        x += motionX;
        z += motionZ;
        
        if (y < 0) {
            y = 0;
            onGround = true;
            motionY = 0;
        } else {
            onGround = false;
        }
    }
}
