package net.ovson.api.event;

public class GuiCloseEvent extends Event {
    private final String screenName;

    public GuiCloseEvent(String screenName) {
        this.screenName = screenName;
    }

    public String getScreenName() {
        return screenName;
    }
}
