package net.ovson.api;

import java.util.logging.Logger;

/**
 * All OVson plugins must extend this class.
 */
public abstract class OVsonPlugin {
    private Logger logger;
    private boolean enabled = false;

    public abstract void onEnable();
    public abstract void onDisable();

    public final Logger getLogger() {
        if (logger == null) {
            logger = Logger.getLogger(getName());
        }
        return logger;
    }

    public final String getName() {
        PluginInfo info = getClass().getAnnotation(PluginInfo.class);
        if (info != null) {
            return info.name();
        }
        return getClass().getSimpleName();
    }

    public final String getVersion() {
        PluginInfo info = getClass().getAnnotation(PluginInfo.class);
        if (info != null) {
            return info.version();
        }
        return "1.0";
    }

    public final String getAuthor() {
        PluginInfo info = getClass().getAnnotation(PluginInfo.class);
        if (info != null) {
            return info.author();
        }
        return "Unknown";
    }

    public final boolean isEnabled() {
        return enabled;
    }

    // Called by the C++ loader to toggle state
    public final void setEnabled(boolean state) {
        if (this.enabled == state) return;
        this.enabled = state;
        if (state) {
            onEnable();
        } else {
            onDisable();
        }
    }
}
