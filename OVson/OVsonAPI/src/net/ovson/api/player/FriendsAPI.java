package net.ovson.api.player;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

public class FriendsAPI {
    
    private static final Set<String> friends = new HashSet<>();
    private static final Set<String> enemies = new HashSet<>();

    public static boolean addFriend(String name) {
        if (name == null || name.isEmpty()) return false;
        return friends.add(name.toLowerCase());
    }

    public static boolean removeFriend(String name) {
        if (name == null || name.isEmpty()) return false;
        return friends.remove(name.toLowerCase());
    }

    public static boolean isFriend(String name) {
        if (name == null || name.isEmpty()) return false;
        return friends.contains(name.toLowerCase());
    }

    public static Set<String> getFriends() {
        return Collections.unmodifiableSet(friends);
    }

    public static boolean addEnemy(String name) {
        if (name == null || name.isEmpty()) return false;
        return enemies.add(name.toLowerCase());
    }

    public static boolean removeEnemy(String name) {
        if (name == null || name.isEmpty()) return false;
        return enemies.remove(name.toLowerCase());
    }

    public static boolean isEnemy(String name) {
        if (name == null || name.isEmpty()) return false;
        return enemies.contains(name.toLowerCase());
    }

    public static Set<String> getEnemies() {
        return Collections.unmodifiableSet(enemies);
    }
    
    public static void clear() {
        friends.clear();
        enemies.clear();
    }
}
