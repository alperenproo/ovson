package net.ovson.api.script;

import net.ovson.api.OVsonPlugin;
import net.ovson.api.event.EventBus;

public abstract class OVsonScript extends OVsonPlugin {
    
    public String scriptName;
    
    public OVsonScript() {
        this.scriptName = this.getClass().getSimpleName();
    }
    
    public void onLoad() {}
    @Override
    public void onEnable() {}
    @Override
    public void onDisable() {}
    
    public void onPreUpdate() {}
    public void onPostUpdate() {}
    public boolean onChat(String message, int type) { return true; } // Return false to cancel
    public void onLocRaw(String locraw) {}
    
    public void print(String msg) {
        net.ovson.api.chat.ChatAPI.showClientMessage(msg);
    }
    
    public void playSound(String sound, float volume, float pitch) {
        net.ovson.api.world.WorldAPI.playSound(sound, volume, pitch, 0, 0, 0);
    }
}
