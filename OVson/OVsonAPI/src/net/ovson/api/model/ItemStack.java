package net.ovson.api.model;

import java.util.List;
import java.util.Map;

public final class ItemStack {
    public final String type;
    public final String name;
    public final String displayName;
    public final int stackSize;
    public final int maxStackSize;
    public final int durability;
    public final int maxDurability;
    public final int meta;
    public final boolean isBlock;

    public ItemStack(String type, String name, String displayName, int stackSize, int maxStackSize, int durability, int maxDurability, int meta, boolean isBlock) {
        this.type = type;
        this.name = name;
        this.displayName = displayName;
        this.stackSize = stackSize;
        this.maxStackSize = maxStackSize;
        this.durability = durability;
        this.maxDurability = maxDurability;
        this.meta = meta;
        this.isBlock = isBlock;
    }

    public boolean isEmpty() {
        return type == null || "air".equals(type) || stackSize <= 0;
    }

    public boolean isDamageable() {
        return maxDurability > 0;
    }

    public float getDurabilityPercent() {
        if (maxDurability > 0) {
            return (float) durability / maxDurability;
        }
        return 1.0f;
    }

    public native List<String> getTooltip();
    public native Map<String, Integer> getEnchantments();
    public native boolean isEnchanted();

    @Override
    public String toString() {
        return "ItemStack{" +
                "type='" + type + '\'' +
                ", stackSize=" + stackSize +
                '}';
    }
}
