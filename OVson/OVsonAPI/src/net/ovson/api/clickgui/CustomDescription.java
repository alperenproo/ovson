package net.ovson.api.clickgui;

public class CustomDescription extends CustomSetting {
    private String text;
    
    public CustomDescription(String text) {
        super(text);
        this.text = text;
    }

    public CustomDescription(String name, String text) {
        super(name);
        this.text = text;
    }
    
    public String getText() {
        return text;
    }
    
    public String getKind() {
        return "description";
    }
}
