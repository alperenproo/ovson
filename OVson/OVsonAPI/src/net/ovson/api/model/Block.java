package net.ovson.api.model;

public final class Block {
    public final String type;
    public final String name;
    public final boolean interactable;
    public final int variant;
    public final int x;
    public final int y;
    public final int z;
    public final float width;
    public final float height;
    public final float length;

    public Block(String type, String name, boolean interactable, int variant, int x, int y, int z, float width, float height, float length) {
        this.type = type;
        this.name = name;
        this.interactable = interactable;
        this.variant = variant;
        this.x = x;
        this.y = y;
        this.z = z;
        this.width = width;
        this.height = height;
        this.length = length;
    }

    public boolean isAir() {
        return "air".equals(type);
    }

    public boolean isLiquid() {
        return type != null && (type.contains("water") || type.contains("lava"));
    }

    public boolean isSolid() {
        return !isAir() && !isLiquid();
    }

    public native float getHardness();
    public native int getLightLevel();
    public native boolean isTransparent();

    @Override
    public String toString() {
        return "Block{" +
                "type='" + type + '\'' +
                ", name='" + name + '\'' +
                ", x=" + x +
                ", y=" + y +
                ", z=" + z +
                '}';
    }
}
