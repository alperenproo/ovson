package net.ovson.api.model;

public final class PotionEffect {
    public final int id;
    public final String name;
    public final int amplifier;
    public final int duration;
    public final boolean isAmbient;

    public PotionEffect(int id, String name, int amplifier, int duration, boolean isAmbient) {
        this.id = id;
        this.name = name;
        this.amplifier = amplifier;
        this.duration = duration;
        this.isAmbient = isAmbient;
    }

    @Override
    public String toString() {
        return "PotionEffect{" +
                "name='" + name + '\'' +
                ", amplifier=" + amplifier +
                ", duration=" + duration +
                '}';
    }
}
