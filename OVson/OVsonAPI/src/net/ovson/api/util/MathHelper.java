package net.ovson.api.util;
import java.util.Random;

public final class MathHelper {
    private MathHelper() {}
    private static final Random RANDOM = new Random();

    public static double round(double value, int places) {
        double scale = Math.pow(10, places);
        return Math.round(value * scale) / scale;
    }
    
    public static int randomInt(int min, int max) {
        return RANDOM.nextInt((max - min) + 1) + min;
    }
    
    public static double randomDouble(double min, double max) {
        return min + (max - min) * RANDOM.nextDouble();
    }
    
    public static float wrapDegrees(float value) {
        value %= 360.0F;
        if (value >= 180.0F) value -= 360.0F;
        if (value < -180.0F) value += 360.0F;
        return value;
    }
    
    public static double clamp(double value, double min, double max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }
    
    public static double lerp(double a, double b, double f) {
        return a + f * (b - a);
    }
    
    public static float lerpAngle(float start, float end, float amount) {
        float diff = wrapDegrees(end - start);
        return start + diff * amount;
    }
    
    public static double easeInOut(double t) {
        return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }
    
    public static double easeIn(double t) {
        return t * t;
    }
    
    public static double easeOut(double t) {
        return t * (2 - t);
    }
}
