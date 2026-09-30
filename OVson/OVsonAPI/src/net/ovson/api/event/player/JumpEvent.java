package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public class JumpEvent extends Event implements Cancellable {
    private float motionY;
    private boolean cancelled;

    public JumpEvent(float motionY) {
        this.motionY = motionY;
    }

    public float getMotionY() {
        return motionY;
    }

    public void setMotionY(float motionY) {
        this.motionY = motionY;
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
