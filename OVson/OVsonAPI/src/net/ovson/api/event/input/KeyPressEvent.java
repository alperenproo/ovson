package net.ovson.api.event.input;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public class KeyPressEvent extends Event implements Cancellable {
    private final char character;
    private final int keyCode;
    private boolean cancelled;

    public KeyPressEvent(char character, int keyCode) {
        this.character = character;
        this.keyCode = keyCode;
    }

    public char getCharacter() {
        return character;
    }

    public int getKeyCode() {
        return keyCode;
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
