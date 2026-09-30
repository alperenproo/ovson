package net.ovson.api.model;

public class Vec3 {
    public final double x, y, z;

    public Vec3(double x, double y, double z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    public Vec3 add(double x, double y, double z) {
        return new Vec3(this.x + x, this.y + y, this.z + z);
    }

    public Vec3 add(Vec3 other) {
        return add(other.x, other.y, other.z);
    }

    public Vec3 subtract(Vec3 other) {
        return new Vec3(this.x - other.x, this.y - other.y, this.z - other.z);
    }

    public Vec3 multiply(double factor) {
        return new Vec3(this.x * factor, this.y * factor, this.z * factor);
    }

    public Vec3 offset(double x, double y, double z) {
        return add(x, y, z);
    }

    public Vec3 translate(double x, double y, double z) {
        return add(x, y, z);
    }

    public Vec3 ceil() {
        return new Vec3(Math.ceil(this.x), Math.ceil(this.y), Math.ceil(this.z));
    }

    public Vec3 floor() {
        return new Vec3(Math.floor(this.x), Math.floor(this.y), Math.floor(this.z));
    }

    public Vec3 inverse() {
        return new Vec3(-this.x, -this.y, -this.z);
    }

    public double length() {
        return Math.sqrt(x * x + y * y + z * z);
    }

    public double lengthSq() {
        return x * x + y * y + z * z;
    }

    public Vec3 normalize() {
        double len = length();
        if (len == 0) return new Vec3(0, 0, 0);
        return new Vec3(this.x / len, this.y / len, this.z / len);
    }

    public double distanceTo(Vec3 other) {
        return Math.sqrt(distanceToSq(other));
    }

    public double distanceToSq(Vec3 other) {
        double dx = this.x - other.x;
        double dy = this.y - other.y;
        double dz = this.z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }

    public Vec3 cross(Vec3 other) {
        return new Vec3(
            this.y * other.z - this.z * other.y,
            this.z * other.x - this.x * other.z,
            this.x * other.y - this.y * other.x
        );
    }

    public double dot(Vec3 other) {
        return this.x * other.x + this.y * other.y + this.z * other.z;
    }

    public Vec3 lerp(Vec3 target, double t) {
        return new Vec3(
            this.x + (target.x - this.x) * t,
            this.y + (target.y - this.y) * t,
            this.z + (target.z - this.z) * t
        );
    }

    public Vec3 rotateY(float yawDeg) {
        double yawRad = Math.toRadians(yawDeg);
        double cos = Math.cos(yawRad);
        double sin = Math.sin(yawRad);
        return new Vec3(this.x * cos - this.z * sin, this.y, this.x * sin + this.z * cos);
    }

    public static Vec3 fromYawPitch(float yaw, float pitch) {
        double yawRad = Math.toRadians(yaw);
        double pitchRad = Math.toRadians(pitch);
        double cosPitch = Math.cos(pitchRad);
        double x = -Math.sin(yawRad) * cosPitch;
        double y = -Math.sin(pitchRad);
        double z = Math.cos(yawRad) * cosPitch;
        return new Vec3(x, y, z);
    }

    @Override
    public String toString() {
        return "Vec3[" + x + ", " + y + ", " + z + "]";
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof Vec3)) return false;
        Vec3 other = (Vec3) obj;
        return Double.compare(this.x, other.x) == 0 &&
               Double.compare(this.y, other.y) == 0 &&
               Double.compare(this.z, other.z) == 0;
    }

    @Override
    public int hashCode() {
        long xBits = Double.doubleToLongBits(x);
        long yBits = Double.doubleToLongBits(y);
        long zBits = Double.doubleToLongBits(z);
        return (int) (xBits ^ (xBits >>> 32) ^ yBits ^ (yBits >>> 32) ^ zBits ^ (zBits >>> 32));
    }
}
