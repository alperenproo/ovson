package net.ovson.api.event.world;

import net.ovson.api.event.Event;
import net.ovson.api.model.Vec3;

public class ExplosionEvent extends Event {
    private final Vec3 position;
    private final float strength;

    public ExplosionEvent(Vec3 position, float strength) {
        this.position = position;
        this.strength = strength;
    }

    public Vec3 getPosition() {
        return position;
    }

    public float getStrength() {
        return strength;
    }
}
