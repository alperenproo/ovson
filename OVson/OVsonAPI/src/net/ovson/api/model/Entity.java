package net.ovson.api.model;

import java.util.List;

public class Entity {
    private final int entityId;

    public Entity(int entityId) {
        this.entityId = entityId;
    }

    public int getEntityId() {
        return this.entityId;
    }

    public native String getType();
    public native String getName();
    public native String getDisplayName();
    public native String getCustomNameTag();
    public native String getUUID();
    
    public native boolean isLiving();
    public native boolean isPlayer();
    public native boolean isUser();
    public native boolean isDead();
    public native boolean onGround();
    public native boolean isCollided();
    public native boolean isCollidedHorizontally();
    public native boolean isCollidedVertically();
    public native boolean inWater();
    public native boolean inLava();
    public native boolean isSprinting();
    public native boolean isSneaking();
    public native boolean isUsingItem();
    public native boolean isBurning();
    public native boolean isInvisible();
    public native boolean isSleeping();
    public native boolean isCreative();
    public native boolean isRiding();
    
    public boolean inLiquid() {
        return inWater() || inLava();
    }
    
    public native boolean isOnLadder();
    public native boolean isOnEdge();
    
    public native float getHealth();
    public native float getMaxHealth();
    public native float getAbsorption();
    public native int getHurtTime();
    public native int getMaxHurtTime();
    
    public native int getHunger();
    public native float getSaturation();
    public native int getAir();
    public native float getExperience();
    public native int getExperienceLevel();
    public native float getFallDistance();
    public native int getTicksExisted();
    
    public native Vec3 getPosition();
    public native Vec3 getLastPosition();
    public native Vec3 getServerPosition();
    public native Vec3 getBlockPosition();
    
    public native Vec3 getMotion();
    public native void setMotion(double x, double y, double z);
    public native void setPosition(double x, double y, double z);
    
    public native double getSpeed();
    public native double getBPS();
    
    public native float getYaw();
    public native float getPitch();
    public native void setYaw(float yaw);
    public native void setPitch(float pitch);
    
    public native void moveTo(double x, double y, double z);
    
    public native ItemStack getHeldItem();
    public native ItemStack getArmorInSlot(int slot);
    
    public native boolean isHoldingWeapon();
    public native boolean isHoldingBlock();
    
    public native List<PotionEffect> getPotionEffects();
    
    public native Entity getRidingEntity();
    public native Entity getRiddenByEntity();
    
    public native float getEyeHeight();
    public native Vec3 getEyePosition();
    
    public native double getDistanceTo(Entity entity);
    public native double getDistanceTo(Vec3 pos);
    
    public native boolean canSee(Entity entity);
    
    public native float[] getRotationsTo(Entity entity);
    public native float[] getRotationsTo(Vec3 pos);
}
