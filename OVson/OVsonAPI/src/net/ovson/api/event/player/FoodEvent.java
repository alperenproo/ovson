package net.ovson.api.event.player;

import net.ovson.api.event.Event;

public class FoodEvent extends Event {
    private final float hunger;
    private final float saturation;

    public FoodEvent(float hunger, float saturation) {
        this.hunger = hunger;
        this.saturation = saturation;
    }

    public float getHunger() {
        return hunger;
    }

    public float getSaturation() {
        return saturation;
    }
}
