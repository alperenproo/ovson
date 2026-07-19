package net.ovson.api.event.network;

public final class PacketSendEvent extends PacketEvent {
    public PacketSendEvent(Object packet, String packetName) {
        super(packet, packetName);
    }
}
