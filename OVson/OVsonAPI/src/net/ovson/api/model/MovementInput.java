package net.ovson.api.model;

public class MovementInput {
    public float forward;
    public float strafe;
    public boolean jump;
    public boolean sneak;

    public MovementInput(float forward, float strafe, boolean jump, boolean sneak) {
        this.forward = forward;
        this.strafe = strafe;
        this.jump = jump;
        this.sneak = sneak;
    }
}
