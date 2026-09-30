package net.ovson.api.event.render;

import net.ovson.api.event.Event;
import net.ovson.api.model.Entity;

public class PostRenderEntityEvent extends Event {
    private final Entity entity;

    public PostRenderEntityEvent(Entity entity) {
        this.entity = entity;
    }

    public Entity getEntity() {
        return entity;
    }
}
