package net.ovson.api.player;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import net.ovson.api.ui.NotificationAPI;

public class AnticheatAPI {

    public static class RegisteredCheck {
        public final String name;
        public final String userFriendlyName;
        public final String blacklistCode;
        public final AnticheatCheck check;

        public RegisteredCheck(String name, String userFriendlyName, String blacklistCode, AnticheatCheck check) {
            this.name = name;
            this.userFriendlyName = userFriendlyName != null ? userFriendlyName : name;
            this.blacklistCode = blacklistCode != null ? blacklistCode : "";
            this.check = check;
        }
    }

    private static final Map<String, RegisteredCheck> checks = new ConcurrentHashMap<String, RegisteredCheck>();
    private static final List<AnticheatFlagListener> flagListeners = new CopyOnWriteArrayList<AnticheatFlagListener>();

    public static native boolean isPlayerFlagged(String playerName);

    public static native boolean isPlayerSneaking(String playerName);

    public static void registerCheck(String name, AnticheatCheck check) {
        registerCheck(name, name, "", check);
    }

    public static void registerCheck(String name, String userFriendlyName, AnticheatCheck check) {
        registerCheck(name, userFriendlyName, "", check);
    }

    public static void registerCheck(String name, String userFriendlyName, String blacklistCode, AnticheatCheck check) {
        if (name == null || check == null) return;
        checks.put(name, new RegisteredCheck(name, userFriendlyName, blacklistCode, check));
    }

    public static void unregisterCheck(String name) {
        if (name != null) {
            checks.remove(name);
        }
    }

    public static void onFlag(AnticheatFlagListener listener) {
        if (listener != null) {
            flagListeners.add(listener);
        }
    }

    public static void removeFlagListener(AnticheatFlagListener listener) {
        flagListeners.remove(listener);
    }

    public static void flag(String playerName, String uuid, String checkId, String userFriendlyName) {
        if (playerName == null) return;
        String friendly = userFriendlyName != null ? userFriendlyName : checkId;
        
        NotificationAPI.warning("AntiCheat Flag", playerName + " -> " + friendly);

        for (AnticheatFlagListener listener : flagListeners) {
            try {
                listener.onFlag(playerName, uuid != null ? uuid : "", checkId, friendly);
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }

    public static void processPlayerData(AnticheatPlayerData data) {
        if (data == null || checks.isEmpty()) return;
        for (RegisteredCheck rc : checks.values()) {
            try {
                if (rc.check.check(data)) {
                    flag(data.name, data.uuid, rc.name, rc.userFriendlyName);
                }
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }

    public static int getRegisteredCheckCount() {
        return checks.size();
    }

    public static Map<String, RegisteredCheck> getRegisteredChecks() {
        return Collections.unmodifiableMap(checks);
    }
}
