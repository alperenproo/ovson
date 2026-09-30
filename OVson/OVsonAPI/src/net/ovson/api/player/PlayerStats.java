package net.ovson.api.player;

public final class PlayerStats {
    public String name;
    public String uuid;
    public String rank;
    public int networkLevel;
    public String teamColor;

    public int bedwarsStar;
    public int bedwarsFinalKills;
    public int bedwarsFinalDeaths;
    public int bedwarsWins;
    public int bedwarsLosses;
    public int winstreak;
    public String tagsDisplay;
    public boolean isNicked;

    public PlayerStats() {}

    public PlayerStats(int bedwarsStar, int bedwarsFinalKills, int bedwarsFinalDeaths,
                       int bedwarsWins, int bedwarsLosses, int winstreak,
                       String tagsDisplay, boolean isNicked) {
        this.bedwarsStar = bedwarsStar;
        this.bedwarsFinalKills = bedwarsFinalKills;
        this.bedwarsFinalDeaths = bedwarsFinalDeaths;
        this.bedwarsWins = bedwarsWins;
        this.bedwarsLosses = bedwarsLosses;
        this.winstreak = winstreak;
        this.tagsDisplay = tagsDisplay;
        this.isNicked = isNicked;
    }

    public PlayerStats(String name, String uuid, String rank, int networkLevel, String teamColor,
                       int bedwarsStar, int bedwarsFinalKills, int bedwarsFinalDeaths,
                       int bedwarsWins, int bedwarsLosses, int winstreak,
                       String tagsDisplay, boolean isNicked) {
        this.name = name;
        this.uuid = uuid;
        this.rank = rank != null ? rank : "NONE";
        this.networkLevel = networkLevel;
        this.teamColor = teamColor != null ? teamColor : "";
        this.bedwarsStar = bedwarsStar;
        this.bedwarsFinalKills = bedwarsFinalKills;
        this.bedwarsFinalDeaths = bedwarsFinalDeaths;
        this.bedwarsWins = bedwarsWins;
        this.bedwarsLosses = bedwarsLosses;
        this.winstreak = winstreak;
        this.tagsDisplay = tagsDisplay;
        this.isNicked = isNicked;
    }

    public double getFkdr() {
        if (bedwarsFinalDeaths == 0) return bedwarsFinalKills;
        return (double) bedwarsFinalKills / (double) bedwarsFinalDeaths;
    }

    public double getWlr() {
        if (bedwarsLosses == 0) return bedwarsWins;
        return (double) bedwarsWins / (double) bedwarsLosses;
    }

    public boolean hasCheatTag() {
        if (tagsDisplay == null) return false;
        return tagsDisplay.contains("[C]") || tagsDisplay.contains("[CC]") || tagsDisplay.contains("[BC]");
    }

    public String getCheatTagString() {
        if (tagsDisplay == null) return "None";
        if (tagsDisplay.contains("[BC]")) return "Blatant";
        if (tagsDisplay.contains("[CC]")) return "Closet";
        if (tagsDisplay.contains("[C]")) return "Confirmed";
        return "None";
    }

    @Override
    public String toString() {
        return "PlayerStats{" +
                "name='" + name + '\'' +
                ", rank='" + rank + '\'' +
                ", level=" + networkLevel +
                ", star=" + bedwarsStar +
                ", fkdr=" + String.format("%.2f", getFkdr()) +
                ", wlr=" + String.format("%.2f", getWlr()) +
                ", winstreak=" + winstreak +
                '}';
    }
}
