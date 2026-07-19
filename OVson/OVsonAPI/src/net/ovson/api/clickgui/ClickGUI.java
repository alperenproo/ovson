package net.ovson.api.clickgui;

import java.util.ArrayList;
import java.util.List;

public final class ClickGUI {
    private static final List<CustomModule> CUSTOM_MODULES = new ArrayList<>();

    private ClickGUI() {}

    /**
     * Registers a custom module that will automatically appear in the specified category in ClickGUI.
     * @param module The CustomModule instance to register.
     */
    public static void registerModule(CustomModule module) {
        if (module != null && !CUSTOM_MODULES.contains(module)) {
            CUSTOM_MODULES.add(module);
            registerModuleNative(module);
        }
    }

    /**
     * Retrieves all registered custom modules.
     */
    public static List<CustomModule> getCustomModules() {
        return CUSTOM_MODULES;
    }

    /**
     * Clears all registered custom modules. Called internally during plugin reload/shutdown.
     */
    public static void clear() {
        CUSTOM_MODULES.clear();
    }

    private static native void registerModuleNative(CustomModule module);
}
