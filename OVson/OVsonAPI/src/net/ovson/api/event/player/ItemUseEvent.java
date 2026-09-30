package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;
import net.ovson.api.model.ItemStack;

public class ItemUseEvent extends Event implements Cancellable {
    private final ItemStack item;
    private boolean cancelled;

    public ItemUseEvent(ItemStack item) {
        this.item = item;
    }

    public ItemStack getItem() {
        return item;
    }

    @Override
    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public void setCancelled(boolean cancelled) {
        this.cancelled = cancelled;
    }
}
