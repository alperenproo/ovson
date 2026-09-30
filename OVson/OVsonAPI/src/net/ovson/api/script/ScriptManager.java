package net.ovson.api.script;

import net.ovson.api.chat.ChatAPI;
import net.ovson.api.event.EventBus;
import net.ovson.api.event.EventHandler;
import net.ovson.api.event.ChatReceivedEvent;
import net.ovson.api.event.TickEvent;
import net.ovson.api.world.WorldAPI;
import net.ovson.api.player.PlayerAPI;

import javax.tools.JavaCompiler;
import javax.tools.ToolProvider;
import java.io.*;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class ScriptManager {
    
    private static final List<OVsonScript> activeScripts = new ArrayList<>();
    private static File scriptDir;
    private static Thread tickerThread = null;
    private static volatile boolean running = false;
    private static Object eventListener = null;
    
    public static void init(File rootDir) {
        scriptDir = new File(rootDir, "scripts");
        if (!scriptDir.exists()) {
            scriptDir.mkdirs();
        }
        
        if (eventListener != null) {
            try {
                EventBus.getInstance().unregister(eventListener);
            } catch (Throwable t) {}
            eventListener = null;
        }
        
        eventListener = new Object() {
            @EventHandler
            public void onChat(ChatReceivedEvent e) {
                List<OVsonScript> copy = new ArrayList<>(activeScripts);
                for (OVsonScript script : copy) {
                    if (script.isEnabled()) {
                        try {
                            if (!script.onChat(e.getMessage(), 0)) {
                                e.setCancelled(true);
                            }
                        } catch (Throwable t) {
                            t.printStackTrace();
                        }
                    }
                }
            }
            
            @EventHandler
            public void onTick(TickEvent e) {
                tickAllScripts();
            }
        };
        
        EventBus.getInstance().register(eventListener);
        
        running = true;
        if (tickerThread == null || !tickerThread.isAlive()) {
            tickerThread = new Thread(() -> {
                while (running) {
                    try {
                        tickAllScripts();
                        Thread.sleep(50);
                    } catch (InterruptedException ie) {
                        break;
                    } catch (Throwable t) {
                        t.printStackTrace();
                    }
                }
            }, "OVson-ScriptManager-Ticker");
            tickerThread.setDaemon(true);
            tickerThread.start();
        }
        
        loadScripts();
    }

    private static void tickAllScripts() {
        List<OVsonScript> copy = new ArrayList<>(activeScripts);
        for (OVsonScript script : copy) {
            if (script.isEnabled()) {
                try {
                    script.onPreUpdate();
                    script.onPostUpdate();
                } catch (Throwable t) {
                    t.printStackTrace();
                }
            }
        }
    }
    
    public static void unloadScripts() {
        running = false;
        if (tickerThread != null) {
            tickerThread.interrupt();
            tickerThread = null;
        }

        List<OVsonScript> copy = new ArrayList<>(activeScripts);
        activeScripts.clear();
        for (OVsonScript script : copy) {
            try {
                script.setEnabled(false);
            } catch (Throwable ex) {
                ex.printStackTrace();
            }
        }
        
        if (eventListener != null) {
            try {
                EventBus.getInstance().unregister(eventListener);
            } catch (Throwable t) {}
            eventListener = null;
        }
    }
    
    public static void loadScripts() {
        unloadScripts();
        
        File[] files = scriptDir.listFiles((dir, name) -> name.endsWith(".java"));
        if (files == null) return;
        
        for (File file : files) {
            compileAndLoad(file);
        }
    }
    
    private static void compileAndLoad(File sourceFile) {
        String scriptName = sourceFile.getName().replace(".java", "");
        File compiledDir = new File(scriptDir, "compiled");
        compiledDir.mkdirs();
        
        File logFile = new File(scriptDir.getParentFile(), "plugins/plugin_debug.log");
        try (PrintWriter logPw = new PrintWriter(new FileWriter(logFile, true))) {
            logPw.println("[ScriptManager] Loading script: " + scriptName);
            
            String rawCode = new String(Files.readAllBytes(sourceFile.toPath()));
            File rawJavaFile = new File(compiledDir, scriptName + ".java");
            Files.write(rawJavaFile.toPath(), rawCode.getBytes("UTF-8"));
            
            File apiJar = new File(scriptDir.getParentFile(), "OVsonAPI.jar");
            String cp = apiJar.exists() ? apiJar.getAbsolutePath() : System.getProperty("java.class.path");
            
            String className = scriptName;
            try {
                Matcher m = Pattern.compile("package\\s+([a-zA-Z0-9_.]+)\\s*;").matcher(rawCode);
                if (m.find()) {
                    className = m.group(1) + "." + scriptName;
                }
            } catch (Exception ex) {}

            JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
            int compileResult = -1;
            StringBuilder compilerOutput = new StringBuilder();
            
            if (compiler != null) {
                ByteArrayOutputStream errStream = new ByteArrayOutputStream();
                compileResult = compiler.run(null, null, errStream, "-d", compiledDir.getPath(), "-source", "1.8", "-target", "1.8", "-cp", cp, "-encoding", "UTF-8", rawJavaFile.getPath());
                compilerOutput.append(errStream.toString("UTF-8"));
            } else {
                try {
                    ProcessBuilder pb = new ProcessBuilder("javac", "-d", compiledDir.getPath(), "-source", "1.8", "-target", "1.8", "-encoding", "UTF-8", "-cp", cp, rawJavaFile.getPath());
                    pb.redirectErrorStream(true);
                    Process p = pb.start();
                    BufferedReader reader = new BufferedReader(new InputStreamReader(p.getInputStream(), "UTF-8"));
                    String line;
                    while ((line = reader.readLine()) != null) {
                        compilerOutput.append(line).append("\n");
                    }
                    compileResult = p.waitFor();
                } catch (Throwable t) {
                    compilerOutput.append("javac process failed: ").append(t.getMessage()).append("\n");
                }
            }
            
            File pkgClass = new File(compiledDir, className.replace('.', '/') + ".class");
            File flatClass = new File(compiledDir, scriptName + ".class");
            
            if (compileResult != 0 && (pkgClass.exists() || flatClass.exists())) {
                logPw.println("  [ScriptManager] Compilation failed/skipped, but found precompiled class: " + (pkgClass.exists() ? pkgClass.getPath() : flatClass.getPath()));
                compileResult = 0;
            }
            
            if (compileResult == 0) {
                URLClassLoader classLoader = new URLClassLoader(new URL[]{ compiledDir.toURI().toURL() }, ScriptManager.class.getClassLoader());
                logPw.println("  [ScriptManager] Loading class: " + className);
                Class<?> clazz = classLoader.loadClass(className);
                OVsonScript instance = (OVsonScript) clazz.getDeclaredConstructor().newInstance();
                
                instance.onLoad();
                instance.setEnabled(true);
                activeScripts.add(instance);
                net.ovson.api.PluginManager.registerPlugin(instance);
                
                logPw.println("  [ScriptManager] Successfully loaded and enabled script: " + scriptName);
                net.ovson.api.chat.ChatAPI.showClientMessage("\u00A7a[OVson] Loaded Script: " + scriptName);
            } else {
                logPw.println("  [ScriptManager] ERROR: Compilation failed for " + scriptName + ": " + compilerOutput.toString());
                net.ovson.api.chat.ChatAPI.showClientMessage("\u00A7c[OVson] Compilation error: " + scriptName);
                String err = compilerOutput.toString().trim();
                if (err.length() > 0) {
                    for (String errLine : err.split("\n")) {
                        net.ovson.api.chat.ChatAPI.showClientMessage("\u00A7c" + errLine);
                    }
                }
            }
        } catch (Throwable e) {
            e.printStackTrace();
            try {
                File exLogFile = new File(scriptDir.getParentFile(), "plugins/plugin_debug.log");
                try (PrintWriter logPw = new PrintWriter(new FileWriter(exLogFile, true))) {
                    logPw.println("  [ScriptManager] EXCEPTION loading " + scriptName + ": " + e.toString());
                    e.printStackTrace(logPw);
                }
                net.ovson.api.chat.ChatAPI.showClientMessage("\u00A7c[OVson] Error loading " + scriptName + ": " + e.getMessage());
            } catch (Throwable ignored) {}
        }
    }
}