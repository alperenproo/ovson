package net.ovson.api.clickgui;

public final class CustomSlider extends CustomSetting {
    private float value;
    private final float min;
    private final float max;
    private final SettingCallback<Float> callback;

    public CustomSlider(String name, float defaultValue, float min, float max, SettingCallback<Float> callback) {
        super(name);
        this.value = defaultValue;
        this.min = min;
        this.max = max;
        this.callback = callback;
    }

    public float getValue() {
        return value;
    }

    public float getMin() {
        return min;
    }

    public float getMax() {
        return max;
    }

    public void setValue(float value) {
        if (this.value != value) {
            this.value = value;
            if (callback != null) {
                callback.onChange(value);
            }
        }
    }

    @Override
    public String getKind() {
        return "slider";
    }
}
