package net.ovson.api.bridge;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public final class Bridge {
    private Bridge() {}
    private static final ConcurrentHashMap<String, Object> map = new ConcurrentHashMap<>();
    
    public static void set(String key, Object value) {
        map.put(key, value);
    }
    
    public static Object get(String key) {
        return map.get(key);
    }
    
    public static boolean has(String key) {
        return map.containsKey(key);
    }
    
    public static void remove(String key) {
        map.remove(key);
    }
    
    public static void clear() {
        map.clear();
    }
    
    public static Set<String> keys() {
        return map.keySet();
    }
}
