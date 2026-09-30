package net.ovson.api.event.render;

import net.ovson.api.event.Event;

public class ScreenRenderEvent extends Event {
    private final String guiName;
    private final int mouseX;
    private final int mouseY;
    private final float partialTicks;

    public ScreenRenderEvent(String guiName, int mouseX, int mouseY, float partialTicks) {
        this.guiName = guiName;
        this.mouseX = mouseX;
        this.mouseY = mouseY;
        this.partialTicks = partialTicks;
    }

    public String getGuiName() {
        return guiName;
    }

    public int getMouseX() {
        return mouseX;
    }

    public int getMouseY() {
        return mouseY;
    }

    public float getPartialTicks() {
        return partialTicks;
    }
}
