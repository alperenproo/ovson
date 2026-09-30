package net.ovson.api.util;

public final class ChatFormat {
    private ChatFormat() {}
    
    public static String color(String text) {
        return text.replace("&", "\u00A7");
    }
    
    public static String strip(String text) {
        return text.replaceAll("\u00A7[0-9a-fk-or]", "");
    }
    
    public static String bold(String text) {
        return "\u00A7l" + text;
    }
    
    public static String italic(String text) {
        return "\u00A7o" + text;
    }
    
    public static String underline(String text) {
        return "\u00A7n" + text;
    }
    
    public static String strikethrough(String text) {
        return "\u00A7m" + text;
    }
    
    public static String rainbow(String text) {
        String colors = "4c6e2ab319d5";
        StringBuilder sb = new StringBuilder();
        int colorIndex = 0;
        for (char c : text.toCharArray()) {
            if (c == ' ') {
                sb.append(c);
            } else {
                sb.append("\u00A7").append(colors.charAt(colorIndex % colors.length())).append(c);
                colorIndex++;
            }
        }
        return sb.toString();
    }
}
