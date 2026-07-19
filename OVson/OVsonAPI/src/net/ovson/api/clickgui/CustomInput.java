package net.ovson.api.clickgui;

public final class CustomInput extends CustomSetting {
    private String value;
    private final SettingCallback<String> callback;

    public CustomInput(String name, String defaultValue, SettingCallback<String> callback) {
        super(name);
        this.value = defaultValue;
        this.callback = callback;
    }

    public String getValue() {
        return value;
    }

    public void setValue(String value) {
        if (value != null && !value.equals(this.value)) {
            this.value = value;
            if (callback != null) {
                callback.onChange(value);
            }
        }
    }

    @Override
    public String getKind() {
        return "input";
    }
}
