package net.ovson.api.event.input;

import net.ovson.api.event.Event;

public class KeyEvent extends Event {
    private final String keyName;
    private final int keyCode;
    private final boolean pressed;
    private final boolean inGui;

    public KeyEvent(String keyName, int keyCode, boolean pressed, boolean inGui) {
        this.keyName = keyName;
        this.keyCode = keyCode;
        this.pressed = pressed;
        this.inGui = inGui;
    }

    public String getKeyName() {
        return keyName;
    }

    public int getKeyCode() {
        return keyCode;
    }

    public boolean isPressed() {
        return pressed;
    }

    public boolean isInGui() {
        return inGui;
    }
}
