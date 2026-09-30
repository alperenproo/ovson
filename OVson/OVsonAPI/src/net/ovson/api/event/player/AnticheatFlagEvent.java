package net.ovson.api.event.player;

import net.ovson.api.event.Event;

public class AnticheatFlagEvent extends Event {
    
    private final String playerName;

    public AnticheatFlagEvent(String playerName) {
        this.playerName = playerName;
    }

    public String getPlayerName() {
        return playerName;
    }
}
