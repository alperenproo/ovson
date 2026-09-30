package net.ovson.api.config;
import java.io.*;
import java.util.*;

public final class ConfigAPI {
    private ConfigAPI() {}
    
    private static File getConfigFile(Object plugin) {
        String name = plugin.getClass().getSimpleName();
        String path = System.getenv("LOCALAPPDATA") + "\\OVson\\plugin_config\\" + name + ".properties";
        return new File(path);
    }
    
    private static Properties loadProperties(Object plugin) {
        Properties props = new Properties();
        File file = getConfigFile(plugin);
        if (file.exists()) {
            try (FileInputStream in = new FileInputStream(file)) {
                props.load(in);
            } catch (Exception e) {}
        }
        return props;
    }
    
    public static synchronized void set(Object plugin, String key, String value) {
        Properties props = loadProperties(plugin);
        props.setProperty(key, value);
        saveProperties(plugin, props);
    }
    
    public static synchronized String get(Object plugin, String key) {
        return loadProperties(plugin).getProperty(key);
    }
    
    public static synchronized String get(Object plugin, String key, String defaultValue) {
        return loadProperties(plugin).getProperty(key, defaultValue);
    }
    
    public static synchronized boolean has(Object plugin, String key) {
        return loadProperties(plugin).containsKey(key);
    }
    
    public static synchronized void remove(Object plugin, String key) {
        Properties props = loadProperties(plugin);
        if(props.remove(key) != null) {
            saveProperties(plugin, props);
        }
    }
    
    public static synchronized Map<String, String> getAll(Object plugin) {
        Properties props = loadProperties(plugin);
        Map<String, String> map = new HashMap<>();
        for (String key : props.stringPropertyNames()) {
            map.put(key, props.getProperty(key));
        }
        return map;
    }
    
    public static synchronized void save(Object plugin) {
        // Saving is handled on set/remove
    }
    
    private static void saveProperties(Object plugin, Properties props) {
        File file = getConfigFile(plugin);
        file.getParentFile().mkdirs();
        try (FileOutputStream out = new FileOutputStream(file)) {
            props.store(out, null);
        } catch (Exception e) {}
    }
}
