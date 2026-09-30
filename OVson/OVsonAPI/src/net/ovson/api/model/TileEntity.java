package net.ovson.api.model;

import java.util.Map;

public final class TileEntity {
    public final String type;
    public final String name;

    public TileEntity(String type, String name) {
        this.type = type;
        this.name = name;
    }

    public native Vec3 getPosition();
    public native String getSkullData();
    public native Map<String, Object> getNbtData();
}
