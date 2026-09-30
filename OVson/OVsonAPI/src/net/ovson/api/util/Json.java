package net.ovson.api.util;
import java.util.*;

public class Json {
    private Object value;
    
    public Json(Object value) {
        this.value = value;
    }
    
    public static Json object() {
        return new Json(new HashMap<String, Object>());
    }
    
    public static Json array() {
        return new Json(new ArrayList<Object>());
    }
    
    public static Json parse(String s) {
        if (s == null || s.trim().isEmpty()) return new Json(null);
        try {
            return new Json(new Parser(s.trim()).parseValue());
        } catch (Exception e) {
            return new Json(null);
        }
    }

    public Object getValue() {
        return value;
    }

    public String getString(String key) {
        if (value instanceof Map) {
            Object v = ((Map<?,?>)value).get(key);
            return v != null ? String.valueOf(v) : null;
        }
        return null;
    }
    
    public int getInt(String key) {
        String s = getString(key);
        return s != null ? Integer.parseInt(s) : 0;
    }
    
    public double getDouble(String key) {
        String s = getString(key);
        return s != null ? Double.parseDouble(s) : 0.0;
    }
    
    public boolean getBoolean(String key) {
        String s = getString(key);
        return s != null ? Boolean.parseBoolean(s) : false;
    }
    
    public Json getObject(String key) {
        if (value instanceof Map) {
            Object v = ((Map<?,?>)value).get(key);
            return v != null ? new Json(v) : null;
        }
        return null;
    }
    
    public Json getArray(String key) {
        if (value instanceof Map) {
            Object v = ((Map<?,?>)value).get(key);
            return v != null ? new Json(v) : null;
        }
        return null;
    }
    
    @SuppressWarnings("unchecked")
    public Json put(String key, Object val) {
        if (value instanceof Map) {
            ((Map<String,Object>)value).put(key, val instanceof Json ? ((Json)val).value : val);
        }
        return this;
    }
    
    @SuppressWarnings("unchecked")
    public Json add(Object val) {
        if (value instanceof List) {
            ((List<Object>)value).add(val instanceof Json ? ((Json)val).value : val);
        }
        return this;
    }
    
    public int size() {
        if (value instanceof List) return ((List<?>)value).size();
        if (value instanceof Map) return ((Map<?,?>)value).size();
        return 0;
    }
    
    public Json get(int index) {
        if (value instanceof List) {
            return new Json(((List<?>)value).get(index));
        }
        return new Json(null);
    }
    
    public Json get(String key) {
        if (value instanceof Map) {
            return new Json(((Map<?,?>)value).get(key));
        }
        return new Json(null);
    }
    
    public String asString() {
        return value != null ? String.valueOf(value) : "";
    }

    public int asInt() {
        if (value instanceof Number) return ((Number) value).intValue();
        try { return Integer.parseInt(asString()); } catch (Exception e) { return 0; }
    }

    public double asDouble() {
        if (value instanceof Number) return ((Number) value).doubleValue();
        try { return Double.parseDouble(asString()); } catch (Exception e) { return 0.0; }
    }

    public boolean asBoolean() {
        if (value instanceof Boolean) return ((Boolean) value).booleanValue();
        return Boolean.parseBoolean(asString());
    }
    
    public boolean has(String key) {
        if (value instanceof Map) {
            return ((Map<?,?>)value).containsKey(key);
        }
        return false;
    }
    
    public String toJson() {
        return value != null ? value.toString() : "null";
    }
    
    public String toPrettyJson() {
        return toJson();
    }
    
    @SuppressWarnings("unchecked")
    public List<String> keys() {
        if (value instanceof Map) {
            return new ArrayList<>(((Map<String,?>)value).keySet());
        }
        return Collections.emptyList();
    }
    
    public Iterable<Json> forEach() {
        if (value instanceof List) {
            List<Json> list = new ArrayList<>();
            for (Object o : (List<?>)value) list.add(new Json(o));
            return list;
        }
        return Collections.emptyList();
    }

    private static class Parser {
        private final String src;
        private int idx = 0;

        Parser(String src) { this.src = src; }

        private void skipWhitespace() {
            while (idx < src.length() && Character.isWhitespace(src.charAt(idx))) {
                idx++;
            }
        }

        Object parseValue() {
            skipWhitespace();
            if (idx >= src.length()) return null;
            char c = src.charAt(idx);
            if (c == '{') return parseObject();
            if (c == '[') return parseArray();
            if (c == '"' || c == '\'') return parseString();
            if (c == 't' || c == 'f') return parseBoolean();
            if (c == 'n') { idx += 4; return null; }
            if (c == '-' || Character.isDigit(c)) return parseNumber();
            return null;
        }

        private Map<String, Object> parseObject() {
            Map<String, Object> map = new LinkedHashMap<>();
            idx++;
            skipWhitespace();
            if (idx < src.length() && src.charAt(idx) == '}') {
                idx++;
                return map;
            }
            while (idx < src.length()) {
                skipWhitespace();
                String key = parseString();
                skipWhitespace();
                if (idx < src.length() && src.charAt(idx) == ':') idx++;
                Object val = parseValue();
                map.put(key, val);
                skipWhitespace();
                if (idx < src.length() && src.charAt(idx) == ',') {
                    idx++;
                } else if (idx < src.length() && src.charAt(idx) == '}') {
                    idx++;
                    break;
                } else {
                    break;
                }
            }
            return map;
        }

        private List<Object> parseArray() {
            List<Object> list = new ArrayList<>();
            idx++;
            skipWhitespace();
            if (idx < src.length() && src.charAt(idx) == ']') {
                idx++;
                return list;
            }
            while (idx < src.length()) {
                Object val = parseValue();
                list.add(val);
                skipWhitespace();
                if (idx < src.length() && src.charAt(idx) == ',') {
                    idx++;
                } else if (idx < src.length() && src.charAt(idx) == ']') {
                    idx++;
                    break;
                } else {
                    break;
                }
            }
            return list;
        }

        private String parseString() {
            skipWhitespace();
            if (idx >= src.length()) return "";
            char quote = src.charAt(idx);
            if (quote != '"' && quote != '\'') return "";
            idx++;
            StringBuilder sb = new StringBuilder();
            while (idx < src.length()) {
                char c = src.charAt(idx++);
                if (c == quote) break;
                if (c == '\\' && idx < src.length()) {
                    char esc = src.charAt(idx++);
                    if (esc == 'n') sb.append('\n');
                    else if (esc == 'r') sb.append('\r');
                    else if (esc == 't') sb.append('\t');
                    else if (esc == 'b') sb.append('\b');
                    else if (esc == 'f') sb.append('\f');
                    else sb.append(esc);
                } else {
                    sb.append(c);
                }
            }
            return sb.toString();
        }

        private Boolean parseBoolean() {
            if (src.startsWith("true", idx)) { idx += 4; return Boolean.TRUE; }
            if (src.startsWith("false", idx)) { idx += 5; return Boolean.FALSE; }
            return Boolean.FALSE;
        }

        private Number parseNumber() {
            int start = idx;
            if (idx < src.length() && (src.charAt(idx) == '-' || src.charAt(idx) == '+')) idx++;
            boolean isFloat = false;
            while (idx < src.length()) {
                char c = src.charAt(idx);
                if (c == '.' || c == 'e' || c == 'E') { isFloat = true; idx++; }
                else if (Character.isDigit(c)) idx++;
                else break;
            }
            String numStr = src.substring(start, idx);
            try {
                if (isFloat) return Double.parseDouble(numStr);
                return Long.parseLong(numStr);
            } catch (Exception e) {
                return 0;
            }
        }
    }
}