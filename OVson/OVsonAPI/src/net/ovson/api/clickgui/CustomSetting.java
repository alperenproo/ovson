package net.ovson.api.clickgui;

public abstract class CustomSetting {
    private final String name;

    protected CustomSetting(String name) {
        this.name = name;
    }

    public final String getName() {
        return name;
    }

    public abstract String getKind();
}
