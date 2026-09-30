package net.ovson.api.render.gl;
public final class GL {
    private GL() {}
    public static final int POINTS = 0;
    public static final int LINES = 1;
    public static final int LINE_STRIP = 3;
    public static final int TRIANGLES = 4;
    public static final int TRIANGLE_FAN = 6;
    public static final int QUADS = 7;
    public static final int POLYGON = 9;

    public static native void push();
    public static native void pop();
    public static native void translate(double x, double y, double z);
    public static native void rotate(float angle, float x, float y, float z);
    public static native void scale(double x, double y, double z);
    
    public static native void blend(boolean enabled);
    public static native void alpha(boolean enabled);
    public static native void depth(boolean enabled);
    public static native void depthMask(boolean enabled);
    public static native void depthFunc(int func);
    public static native void cull(boolean enabled);
    public static native void lighting(boolean enabled);
    public static native void texture2d(boolean enabled);
    public static native void lineSmooth(boolean enabled);
    public static native void lineWidth(float width);
    public static native void polygonSmooth(boolean enabled);
    
    public static native void begin(int mode);
    public static native void end();
    public static native void vertex2(float x, float y);
    public static native void vertex3(double x, double y, double z);
    public static native void color4f(float r, float g, float b, float a);
    public static native void color(int hexColor);
    public static native void resetColor();
    public static native void colorMask(boolean r, boolean g, boolean b, boolean a);
    public static native void texCoord2(float u, float v);
    public static native void normal(float x, float y, float z);
    
    public static native void bindTexture(int id);
    public static native int getBoundTexture();
    public static native int loadTexture(String path);
}
