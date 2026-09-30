package net.ovson.api.event.input;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public class MouseEvent extends Event implements Cancellable {
    private final int button;
    private final boolean pressed;
    private final int x;
    private final int y;
    private final int scroll;
    private boolean cancelled;

    public MouseEvent(int button, boolean pressed, int x, int y, int scroll) {
        this.button = button;
        this.pressed = pressed;
        this.x = x;
        this.y = y;
        this.scroll = scroll;
    }

    public int getButton() {
        return button;
    }

    public boolean isPressed() {
        return pressed;
    }

    public int getX() {
        return x;
    }

    public int getY() {
        return y;
    }

    public int getScroll() {
        return scroll;
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
