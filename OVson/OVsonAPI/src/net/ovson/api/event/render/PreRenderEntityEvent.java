package net.ovson.api.event.render;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;
import net.ovson.api.model.Entity;

public class PreRenderEntityEvent extends Event implements Cancellable {
    private final Entity entity;
    private boolean cancelled;

    public PreRenderEntityEvent(Entity entity) {
        this.entity = entity;
    }

    public Entity getEntity() {
        return entity;
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
