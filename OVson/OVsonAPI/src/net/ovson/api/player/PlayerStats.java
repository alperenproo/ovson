package net.ovson.api.player;

public final class PlayerStats {
    public int bedwarsStar;
    public int bedwarsFinalKills;
    public int bedwarsFinalDeaths;
    public int bedwarsWins;
    public int bedwarsLosses;
    public int winstreak;
    public String tagsDisplay;
    public boolean isNicked;

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
}
