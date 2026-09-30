package net.ovson.api.world;
import net.ovson.api.model.*;
import java.util.List;
import java.util.Map;
public final class WorldAPI {
    private WorldAPI() {}
    
    public static native boolean exists();
    public static native int getDimension();
    public static native long getWorldTime();
    public static native long getTotalWorldTime();
    public static native boolean isRaining();
    public static native boolean isThundering();
    public static native float getSunAngle();
    public static native int getDifficulty();
    
    public static native List<Entity> getEntities();
    public static native List<Entity> getPlayerEntities();
    public static native Entity getEntityById(int id);
    public static native boolean isValidEntity(Entity entity);
    public static native List<Entity> getEntitiesInRadius(Vec3 pos, double radius);
    public static native List<Entity> getEntitiesByType(String type);
    public static native Entity getClosestEntity(double maxDistance);
    public static native Entity getClosestPlayer(double maxDistance);
    
    public static native Block getBlockAt(int x, int y, int z);
    public static Block getBlockAt(Vec3 pos) {
        return getBlockAt((int)Math.floor(pos.x), (int)Math.floor(pos.y), (int)Math.floor(pos.z));
    }
    public static native int getBlockIdAt(int x, int y, int z);
    public static native int getLightLevelAt(int x, int y, int z);
    public static native boolean isBlockSolid(int x, int y, int z);
    public static native boolean canSeeBlock(Vec3 start, Vec3 end);
    
    public static native List<TileEntity> getTileEntities();
    
    public static native List<NetworkPlayer> getNetworkPlayers();
    public static native String getTabHeader();
    public static native String getTabFooter();
    public static native Map<String, List<String>> getTeams();
    public static native List<String> getScoreboardLines();
    
    public static native void playSound(String sound, float volume, float pitch, double x, double y, double z);
    
    public static native Entity spawnClientEntity(String type, double x, double y, double z);
    public static native void removeClientEntity(Entity entity);
    public static native void clearClientEntities();
    
    public static native String getTitleText();
    public static native void setTitleText(String title, String subtitle, int fadeIn, int stay, int fadeOut);
    public static native void clearTitleText();
    
    public static native String getBiomeAt(int x, int z);
}
