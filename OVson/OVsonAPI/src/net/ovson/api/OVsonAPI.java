package net.ovson.api;

import net.ovson.api.player.PlayerStats;

public final class OVsonAPI {
    private OVsonAPI() {}

    /**
     * Retrieves the stats of a player from OVson's internal cache if taranmış.
     * Returns null if player is not cached.
     */
    public static native PlayerStats getPlayerStats(String name);

    /**
     * Checks if an OVson module is enabled by name (case-insensitive).
     */
    public static native boolean isModuleEnabled(String moduleName);

    /**
     * Toggles an OVson module state.
     */
    public static native void setModuleEnabled(String moduleName, boolean enabled);
}
