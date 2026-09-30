package net.ovson.api.event.world;

import java.util.List;
import net.ovson.api.event.Event;

public class ScoreboardEvent extends Event {
    private final String objective;
    private final List<String> lines;

    public ScoreboardEvent(String objective, List<String> lines) {
        this.objective = objective;
        this.lines = lines;
    }

    public String getObjective() {
        return objective;
    }

    public List<String> getLines() {
        return lines;
    }
}
