package net.ovson.api.player;

@FunctionalInterface
public interface AnticheatCheck {

    boolean check(AnticheatPlayerData player);
}
