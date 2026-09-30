package net.ovson.api.clickgui;

public class CustomColor extends CustomSetting {
    private int r, g, b, a;
    
    public CustomColor(String name) {
        super(name);
        this.r = 255;
        this.g = 255;
        this.b = 255;
        this.a = 255;
    }
    
    public String getKind() {
        return "color";
    }
    
    public int[] getValue() {
        return new int[]{r, g, b, a};
    }
    
    public void setValue(int r, int g, int b, int a) {
        this.r = r;
        this.g = g;
        this.b = b;
        this.a = a;
    }
}
