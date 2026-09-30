package net.ovson.api.event.world;

import net.ovson.api.event.Event;
import net.ovson.api.model.Entity;

public class WorldJoinEvent extends Event {
    private final Entity entity;

    public WorldJoinEvent(Entity entity) {
        this.entity = entity;
    }

    public Entity getEntity() {
        return entity;
    }
}
