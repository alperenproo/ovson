package net.ovson.api.player;

public class AnticheatPlayerData {

    public final int entityId;
    public final String name;
    public final String uuid;

    public final double posX;
    public final double posY;
    public final double posZ;

    public final double lastPosX;
    public final double lastPosY;
    public final double lastPosZ;

    public final double motionX;
    public final double motionY;
    public final double motionZ;

    public final float rotationYaw;
    public final float rotationPitch;

    public final boolean onGround;
    public final boolean isSneaking;
    public final boolean isSprinting;
    public final boolean isSwingInProgress;
    public final String heldItemName;

    public final float fallDistance;
    public final int hurtTime;

    public AnticheatPlayerData(int entityId, String name, String uuid,
                               double posX, double posY, double posZ,
                               double lastPosX, double lastPosY, double lastPosZ,
                               double motionX, double motionY, double motionZ,
                               float rotationYaw, float rotationPitch,
                               boolean onGround, boolean isSneaking, boolean isSprinting,
                               boolean isSwingInProgress, String heldItemName,
                               float fallDistance, int hurtTime) {
        this.entityId = entityId;
        this.name = name;
        this.uuid = uuid;
        this.posX = posX;
        this.posY = posY;
        this.posZ = posZ;
        this.lastPosX = lastPosX;
        this.lastPosY = lastPosY;
        this.lastPosZ = lastPosZ;
        this.motionX = motionX;
        this.motionY = motionY;
        this.motionZ = motionZ;
        this.rotationYaw = rotationYaw;
        this.rotationPitch = rotationPitch;
        this.onGround = onGround;
        this.isSneaking = isSneaking;
        this.isSprinting = isSprinting;
        this.isSwingInProgress = isSwingInProgress;
        this.heldItemName = heldItemName != null ? heldItemName : "";
        this.fallDistance = fallDistance;
        this.hurtTime = hurtTime;
    }

    public double getHorizontalSpeed() {
        return Math.sqrt(motionX * motionX + motionZ * motionZ);
    }

    public double getSpeed() {
        return Math.sqrt(motionX * motionX + motionY * motionY + motionZ * motionZ);
    }

    public double getDeltaY() {
        return posY - lastPosY;
    }

    public double getDistanceTo(double x, double y, double z) {
        double dx = posX - x;
        double dy = posY - y;
        double dz = posZ - z;
        return Math.sqrt(dx * dx + dy * dy + dz * dz);
    }
}
