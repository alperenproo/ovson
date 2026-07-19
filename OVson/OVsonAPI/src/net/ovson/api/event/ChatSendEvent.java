package net.ovson.api.event;

public final class ChatSendEvent extends Event implements Cancellable {
    private final String message;
    private boolean cancelled = false;

    public ChatSendEvent(String message) {
        this.message = message;
    }

    public String getMessage() {
        return message;
    }

    @Override
    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public void setCancelled(boolean cancel) {
        this.cancelled = cancel;
    }
}
