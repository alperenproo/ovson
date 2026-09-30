package net.ovson.api.world;

import java.util.List;

public final class ScoreboardAPI {
    private ScoreboardAPI() {}

    public static native String getObjectiveTitle();
    public static native List<String> getLines();
}
