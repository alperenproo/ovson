package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public final class AttackEvent extends Event implements Cancellable {
    private final int targetEntityId;
    private boolean cancelled = false;

    public AttackEvent(int targetEntityId) {
        this.targetEntityId = targetEntityId;
    }

    /**
     * Gets the entity ID being attacked.
     */
    public int getTargetEntityId() {
        return targetEntityId;
    }

    @Override
    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public void setCancelled(boolean cancel) {
        this.cancelled = cancel;
    }
}
