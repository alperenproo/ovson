package net.ovson.api.event;

public final class PlayerJoinEvent extends Event {
    private final String playerName;

    public PlayerJoinEvent(String playerName) {
        this.playerName = playerName;
    }

    public String getPlayerName() {
        return playerName;
    }
}
