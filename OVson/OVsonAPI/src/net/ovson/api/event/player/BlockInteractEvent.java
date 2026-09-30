package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;
import net.ovson.api.model.Vec3;

public class BlockInteractEvent extends Event implements Cancellable {
    private final Vec3 pos;
    private final String side;
    private boolean cancelled;

    public BlockInteractEvent(Vec3 pos, String side) {
        this.pos = pos;
        this.side = side;
    }

    public Vec3 getPos() {
        return pos;
    }

    public String getSide() {
        return side;
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
