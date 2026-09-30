package net.ovson.api.chat;

import java.util.ArrayList;
import java.util.List;

public class ChatBuilder {

    private final List<Part> parts = new ArrayList<Part>();
    private Part currentPart;

    public ChatBuilder() {
        this.currentPart = new Part("");
        this.parts.add(currentPart);
    }

    public ChatBuilder(String initialText) {
        this.currentPart = new Part(initialText != null ? initialText : "");
        this.parts.add(currentPart);
    }

    public static ChatBuilder create() {
        return new ChatBuilder();
    }

    public static ChatBuilder of(String initialText) {
        return new ChatBuilder(initialText);
    }

    public ChatBuilder append(String text) {
        this.currentPart = new Part(text != null ? text : "");
        this.parts.add(currentPart);
        return this;
    }

    public ChatBuilder append(ChatBuilder other) {
        if (other != null && !other.parts.isEmpty()) {
            this.parts.addAll(other.parts);
            this.currentPart = this.parts.get(this.parts.size() - 1);
        }
        return this;
    }

    public ChatBuilder text(String text) {
        if (currentPart.text.isEmpty()) {
            currentPart.text = (text != null ? text : "");
        } else {
            append(text);
        }
        return this;
    }

    public ChatBuilder color(String colorName) {
        if (colorName == null) return this;
        currentPart.color = parseColorName(colorName);
        return this;
    }

    public ChatBuilder color(char colorChar) {
        currentPart.color = parseColorChar(colorChar);
        return this;
    }

    public ChatBuilder bold() {
        currentPart.bold = true;
        return this;
    }

    public ChatBuilder italic() {
        currentPart.italic = true;
        return this;
    }

    public ChatBuilder underline() {
        currentPart.underlined = true;
        return this;
    }

    public ChatBuilder strikethrough() {
        currentPart.strikethrough = true;
        return this;
    }

    public ChatBuilder obfuscated() {
        currentPart.obfuscated = true;
        return this;
    }

    public ChatBuilder hoverText(String tooltip) {
        currentPart.hoverAction = "show_text";
        currentPart.hoverValue = tooltip != null ? tooltip : "";
        return this;
    }

    public ChatBuilder clickCommand(String command) {
        currentPart.clickAction = "run_command";
        currentPart.clickValue = command != null ? command : "";
        return this;
    }

    public ChatBuilder clickUrl(String url) {
        currentPart.clickAction = "open_url";
        currentPart.clickValue = url != null ? url : "";
        return this;
    }

    public ChatBuilder clickSuggest(String suggestedText) {
        currentPart.clickAction = "suggest_command";
        currentPart.clickValue = suggestedText != null ? suggestedText : "";
        return this;
    }

    public String toJson() {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"text\":\"\",\"extra\":[");
        for (int i = 0; i < parts.size(); i++) {
            if (i > 0) sb.append(",");
            parts.get(i).writeJson(sb);
        }
        sb.append("]}");
        return sb.toString();
    }

    public String toLegacyText() {
        StringBuilder sb = new StringBuilder();
        for (Part part : parts) {
            if (part.color != null) sb.append(getColorLegacy(part.color));
            if (part.bold) sb.append("§l");
            if (part.italic) sb.append("§o");
            if (part.underlined) sb.append("§n");
            if (part.strikethrough) sb.append("§m");
            if (part.obfuscated) sb.append("§k");
            sb.append(part.text);
        }
        return sb.toString();
    }

    public void send() {
        ChatAPI.showJsonMessage(toJson(), toLegacyText());
    }

    private static String parseColorName(String name) {
        name = name.toLowerCase().trim();
        if (name.length() == 1) return parseColorChar(name.charAt(0));
        switch (name) {
            case "black": return "black";
            case "dark_blue": return "dark_blue";
            case "dark_green": return "dark_green";
            case "dark_aqua": return "dark_aqua";
            case "dark_red": return "dark_red";
            case "dark_purple": return "dark_purple";
            case "gold": case "orange": return "gold";
            case "gray": case "grey": return "gray";
            case "dark_gray": case "dark_grey": return "dark_gray";
            case "blue": return "blue";
            case "green": return "green";
            case "aqua": case "cyan": return "aqua";
            case "red": return "red";
            case "light_purple": case "pink": case "purple": return "light_purple";
            case "yellow": return "yellow";
            case "white": return "white";
            default: return name;
        }
    }

    private static String parseColorChar(char c) {
        switch (c) {
            case '0': return "black";
            case '1': return "dark_blue";
            case '2': return "dark_green";
            case '3': return "dark_aqua";
            case '4': return "dark_red";
            case '5': return "dark_purple";
            case '6': return "gold";
            case '7': return "gray";
            case '8': return "dark_gray";
            case '9': return "blue";
            case 'a': return "green";
            case 'b': return "aqua";
            case 'c': return "red";
            case 'd': return "light_purple";
            case 'e': return "yellow";
            case 'f': return "white";
            default: return "white";
        }
    }

    private static String getColorLegacy(String colorName) {
        switch (colorName) {
            case "black": return "§0";
            case "dark_blue": return "§1";
            case "dark_green": return "§2";
            case "dark_aqua": return "§3";
            case "dark_red": return "§4";
            case "dark_purple": return "§5";
            case "gold": return "§6";
            case "gray": return "§7";
            case "dark_gray": return "§8";
            case "blue": return "§9";
            case "green": return "§a";
            case "aqua": return "§b";
            case "red": return "§c";
            case "light_purple": return "§d";
            case "yellow": return "§e";
            case "white": return "§f";
            default: return "§f";
        }
    }

    private static class Part {
        String text;
        String color;
        boolean bold;
        boolean italic;
        boolean underlined;
        boolean strikethrough;
        boolean obfuscated;
        String clickAction;
        String clickValue;
        String hoverAction;
        String hoverValue;

        Part(String text) {
            this.text = text;
        }

        void writeJson(StringBuilder sb) {
            sb.append("{\"text\":\"").append(escapeJson(text)).append("\"");
            if (color != null) sb.append(",\"color\":\"").append(color).append("\"");
            if (bold) sb.append(",\"bold\":true");
            if (italic) sb.append(",\"italic\":true");
            if (underlined) sb.append(",\"underlined\":true");
            if (strikethrough) sb.append(",\"strikethrough\":true");
            if (obfuscated) sb.append(",\"obfuscated\":true");

            if (clickAction != null && clickValue != null) {
                sb.append(",\"clickEvent\":{\"action\":\"")
                  .append(clickAction).append("\",\"value\":\"")
                  .append(escapeJson(clickValue)).append("\"}");
            }

            if (hoverAction != null && hoverValue != null) {
                sb.append(",\"hoverEvent\":{\"action\":\"")
                  .append(hoverAction).append("\",\"value\":{\"text\":\"")
                  .append(escapeJson(hoverValue)).append("\"}}");
            }
            sb.append("}");
        }

        private static String escapeJson(String s) {
            if (s == null) return "";
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < s.length(); i++) {
                char c = s.charAt(i);
                switch (c) {
                    case '"': sb.append("\\\""); break;
                    case '\\': sb.append("\\\\"); break;
                    case '\b': sb.append("\\b"); break;
                    case '\f': sb.append("\\f"); break;
                    case '\n': sb.append("\\n"); break;
                    case '\r': sb.append("\\r"); break;
                    case '\t': sb.append("\\t"); break;
                    default:
                        if (c < ' ') {
                            String hex = "000" + Integer.toHexString(c);
                            sb.append("\\u").append(hex.substring(hex.length() - 4));
                        } else {
                            sb.append(c);
                        }
                }
            }
            return sb.toString();
        }
    }
}
