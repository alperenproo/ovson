package net.ovson.api.event.world;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public class TitleEvent extends Event implements Cancellable {
    private final String title;
    private final String subtitle;
    private boolean cancelled;

    public TitleEvent(String title, String subtitle) {
        this.title = title;
        this.subtitle = subtitle;
    }

    public String getTitle() {
        return title;
    }

    public String getSubtitle() {
        return subtitle;
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
