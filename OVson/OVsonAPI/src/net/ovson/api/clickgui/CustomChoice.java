package net.ovson.api.clickgui;

public final class CustomChoice extends CustomSetting {
    private String value;
    private final String[] choices;
    private final SettingCallback<String> callback;

    public CustomChoice(String name, String[] choices, String defaultValue, SettingCallback<String> callback) {
        super(name);
        this.choices = choices;
        this.value = defaultValue;
        this.callback = callback;
    }

    public String getValue() {
        return value;
    }

    public String[] getChoices() {
        return choices;
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
        return "choice";
    }
}
