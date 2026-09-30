package net.ovson.api.clickgui;

public class CustomButton extends CustomSetting {
    private final Runnable action;

    public CustomButton(String name, Runnable action) {
        super(name);
        this.action = action;
    }

    @Override
    public String getKind() {
        return "button";
    }

    public void click() {
        if (this.action != null) {
            try {
                this.action.run();
            } catch (Throwable t) {
                t.printStackTrace();
            }
        }
    }
}
