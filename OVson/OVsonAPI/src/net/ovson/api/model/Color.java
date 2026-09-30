package net.ovson.api.model;

public final class Color {
    public final int r;
    public final int g;
    public final int b;
    public final int a;

    public Color(int r, int g, int b) {
        this(r, g, b, 255);
    }

    public Color(int r, int g, int b, int a) {
        this.r = r;
        this.g = g;
        this.b = b;
        this.a = a;
    }

    public Color(int argb) {
        this.a = (argb >> 24) & 0xFF;
        this.r = (argb >> 16) & 0xFF;
        this.g = (argb >> 8) & 0xFF;
        this.b = argb & 0xFF;
    }

    public int toARGB() {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    public Color withAlpha(int alpha) {
        return new Color(this.r, this.g, this.b, alpha);
    }

    public static Color fromHSB(float hue, float saturation, float brightness) {
        int argb = java.awt.Color.HSBtoRGB(hue, saturation, brightness);
        return new Color(argb);
    }

    public static Color lerp(Color c1, Color c2, float t) {
        t = Math.max(0.0f, Math.min(1.0f, t));
        int r = (int) (c1.r + (c2.r - c1.r) * t);
        int g = (int) (c1.g + (c2.g - c1.g) * t);
        int b = (int) (c1.b + (c2.b - c1.b) * t);
        int a = (int) (c1.a + (c2.a - c1.a) * t);
        return new Color(r, g, b, a);
    }

    public static Color rainbow(float speed, int offset) {
        float hue = ((System.currentTimeMillis() + offset) % (int)(1000 / speed)) / (1000f / speed);
        return fromHSB(hue, 1.0f, 1.0f);
    }
}
