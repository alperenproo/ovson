package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public final class UpdateEvent extends Event implements Cancellable {
    private boolean cancelled = false;
    private final boolean pre;
    
    // Player's yaw/pitch that will be sent to the server. Can be modified by the plugin.
    public float yaw;
    public float pitch;
    public boolean onGround;

    public UpdateEvent(boolean pre, float yaw, float pitch, boolean onGround) {
        this.pre = pre;
        this.yaw = yaw;
        this.pitch = pitch;
        this.onGround = onGround;
    }

    public boolean isPre() {
        return pre;
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
