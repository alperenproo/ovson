package net.ovson.api.media;

public class AudioAPI {
    public static native void playSoundFile(String filePath, float volume);
    
    public static native String getClipboard();
    
    public static native void setClipboard(String text);
}
