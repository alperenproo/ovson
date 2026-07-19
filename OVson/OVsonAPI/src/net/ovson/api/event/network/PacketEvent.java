package net.ovson.api.event.network;

import net.ovson.api.event.Event;
import net.ovson.api.event.Cancellable;

public abstract class PacketEvent extends Event implements Cancellable {
    private final Object packet;
    private final String packetName;
    private boolean cancelled = false;

    public PacketEvent(Object packet, String packetName) {
        this.packet = packet;
        this.packetName = packetName;
    }

    /**
     * Gets the raw net.minecraft.network.Packet object.
     * Use reflection or cast it if you have the Minecraft classes in your build path.
     */
    public Object getPacket() {
        return packet;
    }

    /**
     * Gets the simple class name of the packet (e.g. "C03PacketPlayer")
     */
    public String getPacketName() {
        return packetName;
    }

    @Override
    public boolean isCancelled() {
        return cancelled;
    }

    @Override
    public void setCancelled(boolean cancel) {
        this.cancelled = cancel;
    }
}
