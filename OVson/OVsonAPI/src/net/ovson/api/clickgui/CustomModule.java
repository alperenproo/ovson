package net.ovson.api.clickgui;

import java.util.ArrayList;
import java.util.List;

public final class CustomModule {
    private final String name;
    private final String category;
    private final String description;
    private boolean enabled;
    private final List<CustomSetting> settings = new ArrayList<>();
    private final ModuleCallback callback;

    public CustomModule(String name, String category, String description, boolean defaultEnabled, ModuleCallback callback) {
        this.name = name;
        this.category = category.toUpperCase(); // Ensure uppercase category (e.g. COMBAT, VISUALS, UTILS, SETTINGS)
        this.description = description;
        this.enabled = defaultEnabled;
        this.callback = callback;
    }

    public String getName() {
        return name;
    }

    public String getCategory() {
        return category;
    }

    public String getDescription() {
        return description;
    }

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        if (this.enabled != enabled) {
            this.enabled = enabled;
            if (callback != null) {
                callback.onToggle(enabled);
            }
        }
    }

    public void addSetting(CustomSetting setting) {
        if (setting != null && !settings.contains(setting)) {
            settings.add(setting);
        }
    }

    public List<CustomSetting> getSettings() {
        return settings;
    }
}
