package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.model.Entity;

public class DamageEvent extends Event {
    private final Entity attacker;
    private final float damage;

    public DamageEvent(Entity attacker, float damage) {
        this.attacker = attacker;
        this.damage = damage;
    }

    public Entity getAttacker() {
        return attacker;
    }

    public float getDamage() {
        return damage;
    }
}
