package net.ovson.api.event.player;

import net.ovson.api.event.Event;
import net.ovson.api.model.MovementInput;

public class PrePlayerInputEvent extends Event {
    private final MovementInput input;

    public PrePlayerInputEvent(MovementInput input) {
        this.input = input;
    }

    public MovementInput getInput() {
        return input;
    }
}
