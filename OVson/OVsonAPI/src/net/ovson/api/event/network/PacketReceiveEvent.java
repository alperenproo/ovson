package net.ovson.api.event.network;

public final class PacketReceiveEvent extends PacketEvent {
    public PacketReceiveEvent(Object packet, String packetName) {
        super(packet, packetName);
    }
}
