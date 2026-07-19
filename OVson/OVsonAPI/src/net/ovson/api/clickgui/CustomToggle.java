package net.ovson.api.clickgui;

public final class CustomToggle extends CustomSetting {
    private boolean value;
    private final SettingCallback<Boolean> callback;

    public CustomToggle(String name, boolean defaultValue, SettingCallback<Boolean> callback) {
        super(name);
        this.value = defaultValue;
        this.callback = callback;
    }

    public boolean getValue() {
        return value;
    }

    public void setValue(boolean value) {
        if (this.value != value) {
            this.value = value;
            if (callback != null) {
                callback.onChange(value);
            }
        }
    }

    @Override
    public String getKind() {
        return "toggle";
    }
}
