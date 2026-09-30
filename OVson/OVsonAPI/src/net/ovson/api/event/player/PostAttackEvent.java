package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.model.Entity;

public class PostAttackEvent extends Event {
    private final Entity target;

    public PostAttackEvent(Entity target) {
        this.target = target;
    }

    public Entity getTarget() {
        return target;
    }
}
