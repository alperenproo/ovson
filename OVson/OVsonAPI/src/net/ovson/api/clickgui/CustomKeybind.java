package net.ovson.api.clickgui;

public class CustomKeybind extends CustomSetting {
    private int keyCode;
    
    public CustomKeybind(String name) {
        super(name);
    }
    
    public String getKind() {
        return "keybind";
    }
    
    public native boolean isPressed();
    
    public int getValue() {
        return keyCode;
    }
    
    public void setValue(int keyCode) {
        this.keyCode = keyCode;
    }
}
