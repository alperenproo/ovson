package net.ovson.api.render;

public final class RenderAPI {
    private RenderAPI() {}

    /**
     * Draws a solid rectangle.
     */
    public static native void drawRect(float x, float y, float width, float height, int color);

    /**
     * Draws a rectangle with rounded corners.
     */
    public static native void drawRoundedRect(float x, float y, float width, float height, float radius, int color);

    /**
     * Draws a rectangle outline.
     */
    public static native void drawOutline(float x, float y, float width, float height, float thickness, int color);

    /**
     * Draws a rounded rectangle outline.
     */
    public static native void drawRoundedOutline(float x, float y, float width, float height, float radius, float thickness, int color);

    /**
     * Draws an additive glow effect behind a rectangle.
     */
    public static native void drawGlow(float x, float y, float width, float height, float radius, int color, float intensity);

    /**
     * Draws text on the screen using OVson's internal FontRenderer.
     */
    public static native void drawString(float x, float y, String text, int color, float scale);

    /**
     * Calculates the width of the given string using OVson's FontRenderer.
     */
    public static native float getStringWidth(String text);
    
    /**
     * Gets the screen width.
     */
    public static native int getDisplayWidth();

    /**
     * Gets the screen height.
     */
    public static native int getDisplayHeight();
}
