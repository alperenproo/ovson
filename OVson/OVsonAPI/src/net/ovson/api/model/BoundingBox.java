package net.ovson.api.model;

public final class BoundingBox {
    public final double minX;
    public final double minY;
    public final double minZ;
    public final double maxX;
    public final double maxY;
    public final double maxZ;

    public BoundingBox(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        this.minX = minX;
        this.minY = minY;
        this.minZ = minZ;
        this.maxX = maxX;
        this.maxY = maxY;
        this.maxZ = maxZ;
    }

    public Vec3 getCenter() {
        return new Vec3((minX + maxX) / 2.0, (minY + maxY) / 2.0, (minZ + maxZ) / 2.0);
    }

    public double getWidth() {
        return maxX - minX;
    }

    public double getHeight() {
        return maxY - minY;
    }

    public double getDepth() {
        return maxZ - minZ;
    }

    public boolean contains(Vec3 vec) {
        return vec.x >= minX && vec.x <= maxX &&
               vec.y >= minY && vec.y <= maxY &&
               vec.z >= minZ && vec.z <= maxZ;
    }

    public boolean intersects(BoundingBox other) {
        return this.maxX >= other.minX && this.minX <= other.maxX &&
               this.maxY >= other.minY && this.minY <= other.maxY &&
               this.maxZ >= other.minZ && this.minZ <= other.maxZ;
    }

    public BoundingBox expand(double x, double y, double z) {
        return new BoundingBox(minX - x, minY - y, minZ - z, maxX + x, maxY + y, maxZ + z);
    }

    public BoundingBox offset(double x, double y, double z) {
        return new BoundingBox(minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z);
    }

    @Override
    public String toString() {
        return "BoundingBox[" + minX + ", " + minY + ", " + minZ + " -> " + maxX + ", " + maxY + ", " + maxZ + "]";
    }
}
