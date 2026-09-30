package net.ovson.api.player;

import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import net.ovson.api.net.HttpClient;
import net.ovson.api.net.HttpResponse;
import net.ovson.api.util.Json;

public class StatsAPI {

    private static String hypixelApiKey = "";
    private static final Map<String, String> uuidCache = new ConcurrentHashMap<String, String>();
    private static final Map<String, PlayerStats> statsCache = new ConcurrentHashMap<String, PlayerStats>();

    private StatsAPI() {}

    public static native double getFkdr(String playerName);

    public static native double getWlr(String playerName);

    public static native int getFinalKills(String playerName);

    public static native int getWinstreak(String playerName);

    public static native int getNetworkLevel(String playerName);

    public static native int getBedwarsStar(String playerName);

    public static native String getTeamColor(String playerName);

    public static native boolean isNicked(String playerName);

    public static native String getRank(String playerName);

    public static native boolean isInHypixelGame();

    public static native int getGameMode();

    public static native String getMapName();

    public static void setHypixelApiKey(String apiKey) {
        hypixelApiKey = (apiKey != null ? apiKey.trim() : "");
    }

    public static String getHypixelApiKey() {
        return hypixelApiKey;
    }

    public static PlayerStats getPlayerStats(String playerName) {
        if (playerName == null || playerName.isEmpty()) return null;

        PlayerStats cached = statsCache.get(playerName.toLowerCase());
        if (cached != null) return cached;

        PlayerStats stats = new PlayerStats(
            playerName,
            uuidCache.getOrDefault(playerName.toLowerCase(), ""),
            getRank(playerName),
            getNetworkLevel(playerName),
            getTeamColor(playerName),
            getBedwarsStar(playerName),
            getFinalKills(playerName),
            0,
            0,
            0,
            getWinstreak(playerName),
            "",
            isNicked(playerName)
        );

        statsCache.put(playerName.toLowerCase(), stats);
        return stats;
    }

    public static CompletableFuture<String> getUuidAsync(String playerName) {
        if (playerName == null || playerName.isEmpty()) {
            return CompletableFuture.completedFuture(null);
        }

        String cached = uuidCache.get(playerName.toLowerCase());
        if (cached != null) {
            return CompletableFuture.completedFuture(cached);
        }

        String url = "https://api.mojang.com/users/profiles/minecraft/" + playerName;
        return HttpClient.getAsync(url).thenApply(new java.util.function.Function<HttpResponse, String>() {
            @Override
            public String apply(HttpResponse resp) {
                if (resp != null && resp.isSuccess()) {
                    try {
                        Json json = Json.parse(resp.body);
                        String id = json.getString("id");
                        if (id != null && !id.isEmpty()) {
                            uuidCache.put(playerName.toLowerCase(), id);
                            return id;
                        }
                    } catch (Exception ignored) {}
                }
                return null;
            }
        });
    }

    public static CompletableFuture<Json> getPlayerProfileAsync(String nameOrUuid) {
        if (nameOrUuid == null || nameOrUuid.isEmpty()) {
            return CompletableFuture.completedFuture(null);
        }

        if (hypixelApiKey.isEmpty()) {
            CompletableFuture<Json> failed = new CompletableFuture<Json>();
            failed.completeExceptionally(new IllegalStateException("Hypixel API Key tanimli degil! Lutfen StatsAPI.setHypixelApiKey(...) cagirin."));
            return failed;
        }

        boolean isUuid = (nameOrUuid.length() == 32 || nameOrUuid.length() == 36) && !nameOrUuid.contains(" ");

        if (isUuid) {
            String cleanUuid = nameOrUuid.replace("-", "");
            return fetchHypixelByUuid(cleanUuid);
        } else {
            return getUuidAsync(nameOrUuid).thenCompose(new java.util.function.Function<String, CompletableFuture<Json>>() {
                @Override
                public CompletableFuture<Json> apply(String uuid) {
                    if (uuid == null) {
                        return CompletableFuture.completedFuture(null);
                    }
                    return fetchHypixelByUuid(uuid);
                }
            });
        }
    }

    private static CompletableFuture<Json> fetchHypixelByUuid(String uuid) {
        String url = "https://api.hypixel.net/player?uuid=" + uuid;
        Map<String, String> headers = new java.util.HashMap<String, String>();
        headers.put("API-Key", hypixelApiKey);

        return HttpClient.getAsync(url, headers).thenApply(new java.util.function.Function<HttpResponse, Json>() {
            @Override
            public Json apply(HttpResponse resp) {
                if (resp != null && resp.isSuccess()) {
                    try {
                        return Json.parse(resp.body);
                    } catch (Exception ignored) {}
                }
                return null;
            }
        });
    }

    public static void clearCache() {
        uuidCache.clear();
        statsCache.clear();
    }
}
