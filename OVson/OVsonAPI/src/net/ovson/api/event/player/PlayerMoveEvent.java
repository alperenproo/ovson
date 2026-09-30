package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.model.Vec3;

/**
 * Fired when the local player moves. Motion values are mutable.
 */
public class PlayerMoveEvent extends Event {
    public double motionX;
    public double motionY;
    public double motionZ;

    public PlayerMoveEvent(double motionX, double motionY, double motionZ) {
        this.motionX = motionX;
        this.motionY = motionY;
        this.motionZ = motionZ;
    }

    public double getMotionX() { return motionX; }
    public void setMotionX(double x) { this.motionX = x; }

    public double getMotionY() { return motionY; }
    public void setMotionY(double y) { this.motionY = y; }

    public double getMotionZ() { return motionZ; }
    public void setMotionZ(double z) { this.motionZ = z; }

    public Vec3 getMotion() {
        return new Vec3(motionX, motionY, motionZ);
    }
}
