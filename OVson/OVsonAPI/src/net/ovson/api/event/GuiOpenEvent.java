package net.ovson.api.event;

public class GuiOpenEvent extends Event implements Cancellable {
    private final String screenName;
    private final boolean opening;
    private boolean cancelled;

    public GuiOpenEvent(String screenName, boolean opening) {
        this.screenName = screenName;
        this.opening = opening;
    }

    public String getScreenName() {
        return screenName;
    }

    public boolean isOpening() {
        return opening;
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
