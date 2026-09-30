package net.ovson.api.model;

public final class RaycastResult {
    public final Vec3 hitPosition;
    public final Vec3 hitOffset;
    public final String side;
    public final Entity hitEntity;
    public final double distance;

    public RaycastResult(Vec3 hitPosition, Vec3 hitOffset, String side, Entity hitEntity, double distance) {
        this.hitPosition = hitPosition;
        this.hitOffset = hitOffset;
        this.side = side;
        this.hitEntity = hitEntity;
        this.distance = distance;
    }

    public boolean isBlock() {
        return hitEntity == null;
    }

    public boolean isEntity() {
        return hitEntity != null;
    }
}
