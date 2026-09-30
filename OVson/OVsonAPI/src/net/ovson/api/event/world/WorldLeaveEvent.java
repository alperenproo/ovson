package net.ovson.api.event.world;

import net.ovson.api.event.Event;
import net.ovson.api.model.Entity;

public class WorldLeaveEvent extends Event {
    private final Entity entity;

    public WorldLeaveEvent(Entity entity) {
        this.entity = entity;
    }

    public Entity getEntity() {
        return entity;
    }
}
