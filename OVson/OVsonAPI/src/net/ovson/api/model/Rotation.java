package net.ovson.api.model;

public final class Rotation {
    public final float yaw;
    public final float pitch;

    public Rotation(float yaw, float pitch) {
        this.yaw = yaw;
        this.pitch = pitch;
    }

    public Rotation normalize() {
        float newYaw = yaw % 360.0f;
        if (newYaw > 180.0f) newYaw -= 360.0f;
        if (newYaw < -180.0f) newYaw += 360.0f;

        float newPitch = pitch % 360.0f;
        if (newPitch > 180.0f) newPitch -= 360.0f;
        if (newPitch < -180.0f) newPitch += 360.0f;

        return new Rotation(newYaw, newPitch);
    }

    public float distanceTo(Rotation other) {
        float yawDiff = Math.abs(this.yaw - other.yaw) % 360.0f;
        if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;

        float pitchDiff = Math.abs(this.pitch - other.pitch) % 360.0f;
        if (pitchDiff > 180.0f) pitchDiff = 360.0f - pitchDiff;

        return (float) Math.sqrt(yawDiff * yawDiff + pitchDiff * pitchDiff);
    }

    public Rotation lerp(Rotation target, float speed) {
        float yawDiff = target.yaw - this.yaw;
        float pitchDiff = target.pitch - this.pitch;
        
        yawDiff = (yawDiff % 360.0f);
        if (yawDiff > 180.0f) yawDiff -= 360.0f;
        if (yawDiff < -180.0f) yawDiff += 360.0f;

        pitchDiff = (pitchDiff % 360.0f);
        if (pitchDiff > 180.0f) pitchDiff -= 360.0f;
        if (pitchDiff < -180.0f) pitchDiff += 360.0f;

        return new Rotation(this.yaw + yawDiff * speed, this.pitch + pitchDiff * speed).normalize();
    }

    public Rotation clamp(float maxYawDelta, float maxPitchDelta) {
        float newYaw = Math.max(-maxYawDelta, Math.min(maxYawDelta, this.yaw));
        float newPitch = Math.max(-maxPitchDelta, Math.min(maxPitchDelta, this.pitch));
        return new Rotation(newYaw, newPitch);
    }

    public Vec3 toDirection() {
        return Vec3.fromYawPitch(this.yaw, this.pitch);
    }

    public static Rotation fromTo(Vec3 from, Vec3 to) {
        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double dz = to.z - from.z;

        double distance = Math.sqrt(dx * dx + dz * dz);
        float yaw = (float) (Math.toDegrees(Math.atan2(dz, dx)) - 90.0f);
        float pitch = (float) -Math.toDegrees(Math.atan2(dy, distance));

        return new Rotation(yaw, pitch).normalize();
    }
}
