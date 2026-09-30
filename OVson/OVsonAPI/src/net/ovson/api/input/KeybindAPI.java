package net.ovson.api.input;
public final class KeybindAPI {
    private KeybindAPI() {}
    public static native boolean isKeyDown(int key);
    public static native boolean isMouseDown(int button);
    public static native boolean isPressed(String bindName);
    public static native void setPressed(String bindName, boolean pressed);
    public static native int getKeyCode(String bindName);
    public static native void leftClick();
    public static native void rightClick();
    public static native int getScroll();
    public static native int[] getMousePosition();
}
