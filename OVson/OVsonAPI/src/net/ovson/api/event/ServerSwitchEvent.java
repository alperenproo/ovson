package net.ovson.api.event;

public class ServerSwitchEvent extends Event {
    private final String serverIP;

    public ServerSwitchEvent(String serverIP) {
        this.serverIP = serverIP;
    }

    public String getServerIP() {
        return serverIP;
    }
}
