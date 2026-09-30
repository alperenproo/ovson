package net.ovson.api.event;

public class ChatTabCompleteEvent extends Event {
    private final String[] matches;

    public ChatTabCompleteEvent(String[] matches) {
        this.matches = matches;
    }

    public String[] getMatches() {
        return matches;
    }
}
