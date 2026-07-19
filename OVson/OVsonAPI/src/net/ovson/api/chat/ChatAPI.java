package net.ovson.api.chat;

public final class ChatAPI {
    private ChatAPI() {}

    /**
     * Sends a chat message or command (/cmd) on behalf of the player to the server.
     */
    public static native void sendChatMessage(String message);

    /**
     * Prints a local client-side text message to the player's chat.
     */
    public static native void showClientMessage(String message);

    /**
     * Prints a local client-side message with the default [OVson] prefix.
     */
    public static native void showPrefixedMessage(String message);

    /**
     * Prints a local client-side raw JSON component message to the player's chat.
     * @param json The JSON string representation of an IChatComponent.
     * @param fallback The fallback text message to show if JSON parsing fails.
     */
    public static native void showJsonMessage(String json, String fallback);

    /**
     * Shows a message on the action bar (above the hotbar).
     */
    public static native void showActionBar(String message);

    /**
     * Shows a large title and subtitle on the screen.
     */
    public static native void showTitle(String title, String subtitle, int fadeIn, int stay, int fadeOut);

    /**
     * Clears all chat messages from the chat GUI.
     */
    public static native void clearChat();

    /**
     * Retrieves the history of received chat messages.
     * @param maxCount The maximum number of messages to retrieve (0 for all).
     * @return A list of chat messages.
     */
    public static native java.util.List<String> getChatHistory(int maxCount);

    /**
     * Retrieves the history of sent messages.
     * @param maxCount The maximum number of messages to retrieve (0 for all).
     * @return A list of sent messages.
     */
    public static native java.util.List<String> getSentHistory(int maxCount);
}
