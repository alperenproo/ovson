package net.ovson.api.model;

public class NetworkPlayer {
    private final Object networkPlayerInfo;

    public NetworkPlayer(Object networkPlayerInfo) {
        this.networkPlayerInfo = networkPlayerInfo;
    }

    public native String getName();
    public native String getDisplayName();
    public native int getPing();
    public native String getUUID();
    public native String getSkinUrl();
}
