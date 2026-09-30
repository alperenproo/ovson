package net.ovson.api.render;
import net.ovson.api.model.*;
public final class RenderAPI {
    private RenderAPI() {}
    
    public static native void drawRect(float x, float y, float width, float height, int color);
    public static native void drawString(String text, float x, float y, int color);
    public static native int getStringWidth(String text);
    public static native int getFontHeight();
    
    public static native void drawGradientRect(float x, float y, float width, float height, int color1, int color2);
    public static native void drawLine2D(float x1, float y1, float x2, float y2, float width, int color);
    public static native void drawCircle(float x, float y, float radius, int color);
    public static native void drawImage(int textureId, float x, float y, float width, float height);
    public static native void drawItem(Object item, float x, float y, float scale);
    
    public static native void drawLine3D(Vec3 start, Vec3 end, float width, int color);
    public static native void drawBlock(Vec3 pos, int color, boolean filled, boolean outlined);
    public static native void drawBlock(int x, int y, int z, int color, boolean filled, boolean outlined);
    public static native void drawEntityBox(Entity entity, int color, float partialTicks, boolean filled, boolean outlined);
    public static native void drawTracer(Entity entity, float partialTicks, int color, float lineWidth);
    public static native void drawFilledBox(Vec3 min, Vec3 max, int color);
    public static native void drawPlane(Vec3 pos, float radius, int color);
    public static native void drawBeam(Vec3 pos, float height, int color);
    
    public static native void drawString3D(String text, Vec3 pos, float scale, boolean centered, boolean background, int color);
    
    public static native Vec3 worldToScreen(double x, double y, double z, float partialTicks, float scale);
    public static native Vec3 screenToWorld(int x, int y, float depth);
    
    public static native void beginBlur();
    public static native void applyBlur(int radius, float strength);
    public static native void beginBloom();
    public static native void applyBloom(int radius, float strength);
    
    public static native void enableScissor(int x, int y, int width, int height);
    public static native void disableScissor();
    
    public static int rgba(int r, int g, int b, int a) {
        return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
    }
    public static int rgb(int r, int g, int b) {
        return rgba(r, g, b, 255);
    }
    public static int rainbow(float offset, int speed) {
        float hue = ((System.currentTimeMillis() + (long)offset) % speed) / (float)speed;
        return java.awt.Color.HSBtoRGB(hue, 1.0f, 1.0f);
    }
    public static int lerpColor(int c1, int c2, float t) {
        int a1 = (c1 >> 24) & 0xFF; int r1 = (c1 >> 16) & 0xFF; int g1 = (c1 >> 8) & 0xFF; int b1 = c1 & 0xFF;
        int a2 = (c2 >> 24) & 0xFF; int r2 = (c2 >> 16) & 0xFF; int g2 = (c2 >> 8) & 0xFF; int b2 = c2 & 0xFF;
        int a = (int)(a1 + (a2 - a1) * t);
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);
        return rgba(r, g, b, a);
    }
}
