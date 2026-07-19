package net.ovson.api.event;

public interface Cancellable {
    boolean isCancelled();
    void setCancelled(boolean cancel);
}
