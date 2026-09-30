package net.ovson.api.util;
import java.util.concurrent.*;

public final class Async {
    private Async() {}
    
    private static final ExecutorService EXECUTOR = Executors.newCachedThreadPool(r -> {
        Thread t = new Thread(r);
        t.setDaemon(true);
        return t;
    });
    
    private static final ScheduledExecutorService SCHEDULED = Executors.newScheduledThreadPool(1, r -> {
        Thread t = new Thread(r);
        t.setDaemon(true);
        return t;
    });

    public static void run(Runnable r) {
        EXECUTOR.submit(r);
    }
    
    public static void runLater(Runnable r, long delayMs) {
        SCHEDULED.schedule(r, delayMs, TimeUnit.MILLISECONDS);
    }
    
    public static native void runOnMainThread(Runnable r);
    
    public static void repeat(Runnable r, long intervalMs, int times) {
        SCHEDULED.submit(() -> {
            for (int i = 0; i < times; i++) {
                try {
                    r.run();
                    Thread.sleep(intervalMs);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        });
    }
    
    public static void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
