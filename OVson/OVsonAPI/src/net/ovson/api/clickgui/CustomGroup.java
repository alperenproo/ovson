package net.ovson.api.clickgui;
import java.util.List;
import java.util.ArrayList;

public class CustomGroup extends CustomSetting {
    private List<CustomSetting> children = new ArrayList<>();
    
    public CustomGroup(String name) {
        super(name);
    }
    
    public void addSetting(CustomSetting setting) {
        children.add(setting);
    }
    
    public List<CustomSetting> getChildren() {
        return children;
    }
    
    public String getKind() {
        return "group";
    }
}
