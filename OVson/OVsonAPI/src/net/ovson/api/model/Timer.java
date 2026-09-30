package net.ovson.api.model;

public class Timer {
    private long lastReset;

    public Timer() {
        reset();
    }

    public void reset() {
        this.lastReset = System.nanoTime();
    }

    public long elapsed() {
        return (System.nanoTime() - lastReset) / 1_000_000L;
    }

    public boolean hasElapsed(long ms) {
        return elapsed() >= ms;
    }

    public boolean hasElapsedAndReset(long ms) {
        if (hasElapsed(ms)) {
            reset();
            return true;
        }
        return false;
    }
}
