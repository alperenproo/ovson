package net.ovson.api.ui;

public class NotificationAPI {
    public static native void sendNotification(String title, String message, int type, float duration);

    public static void info(String title, String message) {
        sendNotification(title, message, 0, 3.0f);
    }

    public static void warning(String title, String message) {
        sendNotification(title, message, 1, 3.5f);
    }

    public static void error(String title, String message) {
        sendNotification(title, message, 2, 4.0f);
    }
}
