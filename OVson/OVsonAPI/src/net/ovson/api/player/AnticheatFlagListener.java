package net.ovson.api.player;

@FunctionalInterface
public interface AnticheatFlagListener {

    void onFlag(String playerName, String uuid, String checkId, String userFriendlyName);
}
