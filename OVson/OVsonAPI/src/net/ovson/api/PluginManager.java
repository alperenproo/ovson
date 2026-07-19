package net.ovson.api;

import net.ovson.api.event.EventBus;
import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Logger;

public final class PluginManager {
    private static final Logger LOGGER = Logger.getLogger("OVsonPluginManager");
    private static final List<OVsonPlugin> PLUGINS = new ArrayList<>();

    public static void loadPlugins(String pluginsDirPath, ClassLoader parentLoader) {
        disablePlugins();
        File pluginsDir = new File(pluginsDirPath);
        File logFile = new File(pluginsDir, "plugin_debug.log");
        try (java.io.PrintWriter pw = new java.io.PrintWriter(new java.io.FileWriter(logFile, true))) {
            pw.println("=== Starting Plugin Load ===");
            if (!pluginsDir.exists()) {
                pw.println("Plugins dir does not exist, creating.");
                pluginsDir.mkdirs();
                return;
            }

            File[] files = pluginsDir.listFiles((dir, name) -> name.endsWith(".jar"));
            if (files == null) {
                pw.println("listFiles returned null.");
                return;
            }
            pw.println("Found " + files.length + " jar files.");

            for (File file : files) {
                try {
                    pw.println("Loading plugin jar: " + file.getName());
                    URL jarUrl = file.toURI().toURL();
                    URLClassLoader classLoader = new URLClassLoader(new URL[]{jarUrl}, parentLoader);

                    try (JarFile jar = new JarFile(file)) {
                        Enumeration<JarEntry> entries = jar.entries();
                        while (entries.hasMoreElements()) {
                            JarEntry entry = entries.nextElement();
                            String name = entry.getName();
                            if (name.endsWith(".class")) {
                                String className = name.replace('/', '.').substring(0, name.length() - 6);
                                pw.println("  Checking class: " + className);
                                try {
                                    Class<?> cls = classLoader.loadClass(className);
                                    pw.println("    -> Loaded class object.");
                                    if (cls.isAnnotationPresent(PluginInfo.class) && OVsonPlugin.class.isAssignableFrom(cls)) {
                                        pw.println("    -> Annotation present and assignable!");
                                        @SuppressWarnings("unchecked")
                                        Class<? extends OVsonPlugin> pluginClass = (Class<? extends OVsonPlugin>) cls;
                                        OVsonPlugin pluginInstance = pluginClass.getDeclaredConstructor().newInstance();
                                        pw.println("    -> Instantiated: " + pluginInstance.getName());
                                        pluginInstance.setEnabled(true);
                                        PLUGINS.add(pluginInstance);
                                    } else {
                                        pw.println("    -> Failed annotation or assignable check.");
                                        pw.println("       hasAnnotation: " + cls.isAnnotationPresent(PluginInfo.class));
                                        pw.println("       isAssignable: " + OVsonPlugin.class.isAssignableFrom(cls));
                                    }
                                } catch (Throwable t) {
                                    pw.println("    -> Exception while inspecting class: " + t.toString());
                                    t.printStackTrace(pw);
                                }
                            }
                        }
                    }
                } catch (Exception e) {
                    pw.println("Failed to load plugin from " + file.getName() + ": " + e.getMessage());
                    e.printStackTrace(pw);
                }
            }
            pw.println("=== Finished Plugin Load ===");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void disablePlugins() {
        for (OVsonPlugin plugin : PLUGINS) {
            try {
                plugin.setEnabled(false);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        PLUGINS.clear();
        net.ovson.api.clickgui.ClickGUI.clear();
    }

    public static List<OVsonPlugin> getPlugins() {
        return PLUGINS;
    }
}
