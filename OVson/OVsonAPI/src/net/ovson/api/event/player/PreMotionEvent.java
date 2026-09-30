package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.model.PlayerState;

public class PreMotionEvent extends Event {
    private final PlayerState state;

    public PreMotionEvent(PlayerState state) {
        this.state = state;
    }

    public PlayerState getState() {
        return state;
    }
}
