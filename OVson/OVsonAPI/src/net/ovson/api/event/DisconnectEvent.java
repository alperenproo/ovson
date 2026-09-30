package net.ovson.api.event;

public class DisconnectEvent extends Event {
    private final String reason;

    public DisconnectEvent(String reason) {
        this.reason = reason;
    }

    public String getReason() {
        return reason;
    }
}
