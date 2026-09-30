package net.ovson.api.event.world;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;
import net.ovson.api.model.Vec3;

public class SoundEvent extends Event implements Cancellable {
    private final String soundName;
    private final Vec3 position;
    private final float volume;
    private final float pitch;
    private boolean cancelled;

    public SoundEvent(String soundName, Vec3 position, float volume, float pitch) {
        this.soundName = soundName;
        this.position = position;
        this.volume = volume;
        this.pitch = pitch;
    }

    public String getSoundName() {
        return soundName;
    }

    public Vec3 getPosition() {
        return position;
    }

    public float getVolume() {
        return volume;
    }

    public float getPitch() {
        return pitch;
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
