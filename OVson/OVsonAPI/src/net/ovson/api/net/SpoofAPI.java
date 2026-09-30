package net.ovson.api.net;

public class SpoofAPI {
    public static native void setFakeLag(int durationMs);
    
    public static native int getChokedPacketsCount();
}
