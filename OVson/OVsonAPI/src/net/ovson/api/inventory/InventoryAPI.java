package net.ovson.api.inventory;
import net.ovson.api.model.ItemStack;
public final class InventoryAPI {
    private InventoryAPI() {}
    
    public static native int getCurrentSlot();
    public static native void setCurrentSlot(int slot);
    public static native void click(int windowId, int slot, int button);
    public static native ItemStack getStackInSlot(int slot);
    public static native int getSize();
    
    public static native ItemStack getHotbarItem(int slot);
    public static native int findItemInHotbar(String id);
    
    public static native ItemStack getHelmet();
    public static native ItemStack getChestplate();
    public static native ItemStack getLeggings();
    public static native ItemStack getBoots();
    
    public static native int getChestSize();
    public static native ItemStack getStackInChestSlot(int slot);
    public static native String getContainerTitle();
    public static native boolean isContainerOpen();
    
    public static native ItemStack getCraftResult();
    public static native ItemStack getStackInCraftingSlot(int slot);
    
    public static native int countItem(String id);
    public static native int findItem(String id);
    public static native void dropItem(int slot);
    public static native void dropAllItems(String id);
    public static native void swapSlots(int fromSlot, int toSlot);
    
    public static native void openInventory();
    public static native void closeScreen();
}
