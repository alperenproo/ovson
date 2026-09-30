package net.ovson.api.hook;

import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;

public class RenderManagerTransformer implements ClassTransformer {
    private static void log(String msg) {
        try {
            String logDir = System.getenv("LOCALAPPDATA") + "\\OVson\\logs";
            java.io.File dir = new java.io.File(logDir);
            if (!dir.exists()) dir.mkdirs();
            java.io.FileWriter fw = new java.io.FileWriter(new java.io.File(dir, "hitbox.log"), true);
            fw.write("[TRANSFORMER] " + msg + "\n");
            fw.close();
        } catch (Throwable ignored) {}
        System.out.println("[OVson-Transformer] " + msg);
    }

    @Override
    public byte[] transform(String className, byte[] classBytes) {
        String normName = className.replace('/', '.');

        boolean isRenderGlobal = normName.equals("net.minecraft.client.renderer.RenderGlobal") || normName.equals("bfr");
        boolean isRenderManager = normName.equals("net.minecraft.client.renderer.entity.RenderManager") || normName.equals("biu");
        boolean isBadlionHitboxes = normName.equals("net.badlion.client.mods.render.Hitboxes");

        if (!isRenderGlobal && !isRenderManager && !isBadlionHitboxes) {
            return null;
        }

        log("Inspecting target class: " + normName + " (len=" + classBytes.length + ")");

        try {
            ClassReader cr = new ClassReader(classBytes);
            ClassNode cn = new ClassNode();
            cr.accept(cn, 0);

            boolean modified = false;

            if (isBadlionHitboxes) {
                for (MethodNode mn : cn.methods) {
                    StringBuilder sb = new StringBuilder();
                    sb.append("[HITBOXES-DUMP] Method: ").append(mn.name).append(mn.desc);
                    sb.append(" access=").append(mn.access);
                    log(sb.toString());
                    
                    for (AbstractInsnNode insn : mn.instructions.toArray()) {
                        if (insn instanceof MethodInsnNode) {
                            MethodInsnNode mi = (MethodInsnNode) insn;
                            String opName = "INVOKE?";
                            if (insn.getOpcode() == Opcodes.INVOKESTATIC) opName = "INVOKESTATIC";
                            else if (insn.getOpcode() == Opcodes.INVOKEVIRTUAL) opName = "INVOKEVIRTUAL";
                            else if (insn.getOpcode() == Opcodes.INVOKESPECIAL) opName = "INVOKESPECIAL";
                            else if (insn.getOpcode() == Opcodes.INVOKEINTERFACE) opName = "INVOKEINTERFACE";
                            log("[HITBOXES-DUMP]   " + opName + " " + mi.owner + "." + mi.name + mi.desc);
                        }
                    }
                }
                
                try {
                    String dumpDir = System.getenv("LOCALAPPDATA") + "\\OVson\\dumps";
                    java.io.File dir = new java.io.File(dumpDir);
                    if (!dir.exists()) dir.mkdirs();
                    java.io.FileOutputStream fos = new java.io.FileOutputStream(new java.io.File(dir, "Hitboxes.class"));
                    fos.write(classBytes);
                    fos.close();
                    log("[HITBOXES-DUMP] Saved class bytes to " + dumpDir + "\\Hitboxes.class");
                } catch (Throwable t) {
                    log("[HITBOXES-DUMP] Failed to save class bytes: " + t.getMessage());
                }

                for (MethodNode mn : cn.methods) {
                    if (mn.name.equals("renderHitbox") && mn.desc != null && mn.desc.contains("DDDFF)V")) {
                        log("Hooking renderHitbox in " + normName + "." + mn.name + mn.desc);
                        InsnList head = new InsnList();
                        head.add(new VarInsnNode(Opcodes.ALOAD, 0)); // pk entity
                        head.add(new VarInsnNode(Opcodes.DLOAD, 1)); // x
                        head.add(new VarInsnNode(Opcodes.DLOAD, 3)); // y
                        head.add(new VarInsnNode(Opcodes.DLOAD, 5)); // z
                        head.add(new MethodInsnNode(
                            Opcodes.INVOKESTATIC,
                            "net/ovson/api/hook/HitboxHook",
                            "onBeginRenderEntity",
                            "(Ljava/lang/Object;DDD)V",
                            false
                        ));
                        mn.instructions.insertBefore(mn.instructions.getFirst(), head);

                        for (AbstractInsnNode insn : mn.instructions.toArray()) {
                            if (insn.getOpcode() == Opcodes.RETURN) {
                                mn.instructions.insertBefore(insn, new MethodInsnNode(
                                    Opcodes.INVOKESTATIC,
                                    "net/ovson/api/hook/HitboxHook",
                                    "onEndRenderEntity",
                                    "()V",
                                    false
                                ));
                            }
                        }
                        modified = true;
                        log("Successfully hooked renderHitbox in " + normName);
                        break;
                    }
                }

                if (!modified) {
                    log("Target method not found in " + normName);
                    return null;
                }

                ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
                cn.accept(cw);
                byte[] out = cw.toByteArray();
                log("SUCCESS: Transformed " + normName + " (" + classBytes.length + " -> " + out.length + " bytes)");
                return out;
            }

            if (isRenderGlobal) {
                try {
                    String dumpDir = System.getenv("LOCALAPPDATA") + "\\OVson\\dumps";
                    java.io.File dir = new java.io.File(dumpDir);
                    if (!dir.exists()) dir.mkdirs();
                    java.io.FileOutputStream fos = new java.io.FileOutputStream(new java.io.File(dir, "bfr.class"));
                    fos.write(classBytes);
                    fos.close();
                    log("[BFR-DUMP] Saved bfr class bytes to dumps/bfr.class");
                } catch (Throwable t) {}

                for (MethodNode mn : cn.methods) {
                    boolean isDrawBox = (mn.desc != null && mn.desc.endsWith(";IIII)V")) &&
                                        (mn.name.equals("drawOutlinedBoundingBox") || mn.name.equals("a"));
                    if (isDrawBox) {
                        log("[BFR-DUMP] Method: " + mn.name + mn.desc + " maxStack=" + mn.maxStack + " maxLocals=" + mn.maxLocals);
                        int idx = 0;
                        for (AbstractInsnNode insn : mn.instructions.toArray()) {
                            if (insn instanceof VarInsnNode) {
                                VarInsnNode vi = (VarInsnNode) insn;
                                String op = "";
                                switch(vi.getOpcode()) {
                                    case Opcodes.ILOAD: op = "ILOAD"; break;
                                    case Opcodes.ALOAD: op = "ALOAD"; break;
                                    case Opcodes.DLOAD: op = "DLOAD"; break;
                                    case Opcodes.FLOAD: op = "FLOAD"; break;
                                    case Opcodes.ISTORE: op = "ISTORE"; break;
                                    case Opcodes.ASTORE: op = "ASTORE"; break;
                                    default: op = "VAR_" + vi.getOpcode(); break;
                                }
                                log("[BFR-DUMP]   " + idx + ": " + op + " " + vi.var);
                            } else if (insn instanceof MethodInsnNode) {
                                MethodInsnNode mi = (MethodInsnNode) insn;
                                log("[BFR-DUMP]   " + idx + ": INVOKE " + mi.owner + "." + mi.name + mi.desc);
                            } else if (insn instanceof IntInsnNode) {
                                IntInsnNode ii = (IntInsnNode) insn;
                                log("[BFR-DUMP]   " + idx + ": INTINSN op=" + ii.getOpcode() + " operand=" + ii.operand);
                            } else if (insn.getOpcode() >= 0) {
                                log("[BFR-DUMP]   " + idx + ": OPCODE " + insn.getOpcode());
                            }
                            idx++;
                        }
                        log("Hooking drawOutlinedBoundingBox in " + normName + "." + mn.name + mn.desc);

                        InsnList head = new InsnList();

                        head.add(new VarInsnNode(Opcodes.ALOAD, 0));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 1));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 2));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 3));
                        head.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/ovson/api/hook/HitboxHook", "updateBoxColor", "(Ljava/lang/Object;III)V", false));

                        head.add(new VarInsnNode(Opcodes.ALOAD, 0));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 1));
                        head.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/ovson/api/hook/HitboxHook", "getBoxRed", "(Ljava/lang/Object;I)I", false));
                        head.add(new VarInsnNode(Opcodes.ISTORE, 1));

                        head.add(new VarInsnNode(Opcodes.ALOAD, 0));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 2));
                        head.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/ovson/api/hook/HitboxHook", "getBoxGreen", "(Ljava/lang/Object;I)I", false));
                        head.add(new VarInsnNode(Opcodes.ISTORE, 2));

                        head.add(new VarInsnNode(Opcodes.ALOAD, 0));
                        head.add(new VarInsnNode(Opcodes.ILOAD, 3));
                        head.add(new MethodInsnNode(Opcodes.INVOKESTATIC, "net/ovson/api/hook/HitboxHook", "getBoxBlue", "(Ljava/lang/Object;I)I", false));
                        head.add(new VarInsnNode(Opcodes.ISTORE, 3));

                        mn.instructions.insertBefore(mn.instructions.getFirst(), head);
                        modified = true;
                        log("Successfully hooked drawOutlinedBoundingBox in " + mn.name);
                        break;
                    }
                }
            } else if (isRenderManager) {
                for (MethodNode mn : cn.methods) {
                    boolean isRenderDebugBoundingBox = 
                        (mn.desc != null && mn.desc.endsWith("DDDFF)V")) &&
                        (mn.name.equals("renderDebugBoundingBox") || mn.name.equals("b"));

                    if (isRenderDebugBoundingBox) {
                        log("Hooking renderDebugBoundingBox in " + normName + "." + mn.name + mn.desc);
                        InsnList head = new InsnList();
                        head.add(new VarInsnNode(Opcodes.ALOAD, 1));   // entity
                        head.add(new VarInsnNode(Opcodes.DLOAD, 2));   // x (double)
                        head.add(new VarInsnNode(Opcodes.DLOAD, 4));   // y (double)
                        head.add(new VarInsnNode(Opcodes.DLOAD, 6));   // z (double)
                        head.add(new MethodInsnNode(
                            Opcodes.INVOKESTATIC,
                            "net/ovson/api/hook/HitboxHook",
                            "onBeginRenderEntity",
                            "(Ljava/lang/Object;DDD)V",
                            false
                        ));
                        mn.instructions.insertBefore(mn.instructions.getFirst(), head);

                        for (AbstractInsnNode insn : mn.instructions.toArray()) {
                            if (insn.getOpcode() == Opcodes.RETURN) {
                                mn.instructions.insertBefore(insn, new MethodInsnNode(
                                    Opcodes.INVOKESTATIC,
                                    "net/ovson/api/hook/HitboxHook",
                                    "onEndRenderEntity",
                                    "()V",
                                    false
                                ));
                            }
                        }
                        modified = true;
                        log("Successfully hooked renderDebugBoundingBox in " + mn.name);
                        break;
                    }
                }
            }

            if (!modified) {
                log("Target method not found in " + normName);
                return null;
            }

            ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
            cn.accept(cw);
            byte[] out = cw.toByteArray();
            log("SUCCESS: Transformed " + normName + " (" + classBytes.length + " -> " + out.length + " bytes)");
            return out;
        } catch (Throwable t) {
            log("ERROR: " + t);
            t.printStackTrace();
            return null;
        }
    }
}
