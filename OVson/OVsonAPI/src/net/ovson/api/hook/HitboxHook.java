package net.ovson.api.hook;

/**
 * Hook for RenderGlobal.drawOutlinedBoundingBox and RenderManager.renderDebugBoundingBox.
 * Colors player hitboxes with resolved team colors.
 * On Badlion Client, bypasses Badlion's Hitboxes.renderHitbox entirely for players with team colors.
 */
public final class HitboxHook {
    public static native boolean isTeamColoredHitboxesEnabled();
    public static native int resolveBoxColor(Object bb, int originalRed, int originalGreen, int originalBlue);
    public static native int resolveEntityColor(Object entity);
    public static native void debugLog(String msg);

    public static void log(String msg) {
        try {
            debugLog(msg);
        } catch (Throwable t) {
            System.out.println("[HitboxHook] " + msg);
        }
    }

    // Active entity being rendered by RenderManager (biu.b / renderDebugBoundingBox)
    private static volatile Object s_currentEntity = null;
    private static volatile int s_currentEntityColor = -1;

    private static volatile Object s_lastBb = null;
    private static volatile int s_lastColor = -1;
    private static volatile int s_lastOrigR = -1;
    private static volatile int s_lastOrigG = -1;
    private static volatile int s_lastOrigB = -1;
    private static long s_lastLogTime = 0;
    private static int s_callCount = 0;
    private static int s_resolvedCount = 0;

    private static ClassLoader s_mcClassLoader = null;
    private static java.lang.reflect.Method s_glColor4f = null;
    private static boolean s_glColor4fFailed = false;

    private static void setGLColor(float r, float g, float b, float a) {
        if (s_glColor4fFailed) return;
        try {
            if (s_glColor4f == null) {
                ClassLoader cl = s_mcClassLoader;
                if (cl == null) cl = Thread.currentThread().getContextClassLoader();
                if (cl != null) {
                    Class<?> gl11 = cl.loadClass("org.lwjgl.opengl.GL11");
                    s_glColor4f = gl11.getMethod("glColor4f", float.class, float.class, float.class, float.class);
                }
            }
            if (s_glColor4f != null) {
                s_glColor4f.invoke(null, r, g, b, a);
            }
        } catch (Throwable t) {
            s_glColor4fFailed = true;
        }
    }

    private static long s_lastEntityDbgTime = 0;

    public static boolean isPlayerEntity(Object entity) {
        if (entity == null) return false;
        String name = entity.getClass().getName();
        if (name.contains("Player") || name.contains("Abstract") ||
            name.equals("wn") || name.equals("bet") || name.equals("bew") || name.equals("bep") ||
            name.equals("pr")) return true;
        Class<?> sup = entity.getClass().getSuperclass();
        while (sup != null && sup != Object.class) {
            String sName = sup.getName();
            if (sName.contains("Player") || sName.contains("Abstract") ||
                sName.equals("wn") || sName.equals("bet") || sName.equals("bew") || sName.equals("bep") ||
                sName.equals("pr")) return true;
            sup = sup.getSuperclass();
        }
        return false;
    }

    private static java.lang.reflect.Method s_getBB = null;
    private static java.lang.reflect.Field s_posX = null, s_posY = null, s_posZ = null;
    private static java.lang.reflect.Field s_bbMinX = null, s_bbMinY = null, s_bbMinZ = null;
    private static java.lang.reflect.Field s_bbMaxX = null, s_bbMaxY = null, s_bbMaxZ = null;
    private static java.lang.reflect.Constructor<?> s_bbCtor = null;
    private static java.lang.reflect.Method s_drawBB = null;
    private static java.lang.reflect.Method s_getEyeHeight = null;
    private static java.lang.reflect.Method s_glDepthMask = null;
    private static java.lang.reflect.Method s_glDisable = null;
    private static java.lang.reflect.Method s_glEnable = null;
    private static boolean s_renderInitDone = false;
    private static boolean s_renderInitFailed = false;
    private static int s_bypassCount = 0;
    private static long s_lastBypassLogTime = 0;

    private static void initRenderReflection(Object entity) {
        if (s_renderInitDone) return;
        s_renderInitDone = true;
        try {
            ClassLoader cl = entity.getClass().getClassLoader();
            if (cl == null) { s_renderInitFailed = true; return; }

            Class<?> entityClass = entity.getClass();
            String[] bbMethodNames = {"D", "getEntityBoundingBox", "func_174813_aQ", "f_"};
            for (String mName : bbMethodNames) {
                try {
                    java.lang.reflect.Method m = null;
                    Class<?> c = entityClass;
                    while (c != null && c != Object.class) {
                        try { m = c.getDeclaredMethod(mName); break; } catch (NoSuchMethodException e) {}
                        c = c.getSuperclass();
                    }
                    if (m != null && m.getParameterTypes().length == 0) {
                        String retName = m.getReturnType().getName();
                        if (retName.equals("aug") || retName.contains("AxisAlignedBB")) {
                            m.setAccessible(true);
                            s_getBB = m;
                            break;
                        }
                    }
                } catch (Throwable ignored) {}
            }
            if (s_getBB == null) {
                Class<?> c = entityClass;
                while (c != null && c != Object.class && s_getBB == null) {
                    for (java.lang.reflect.Method m : c.getDeclaredMethods()) {
                        if (m.getParameterTypes().length == 0) {
                            String retName = m.getReturnType().getName();
                            if (retName.equals("aug") || retName.endsWith("AxisAlignedBB")) {
                                m.setAccessible(true);
                                s_getBB = m;
                                break;
                            }
                        }
                    }
                    c = c.getSuperclass();
                }
            }
            if (s_getBB == null) {
                log("[RENDER-INIT] FAIL: getEntityBoundingBox not found");
                s_renderInitFailed = true; return;
            }
            log("[RENDER-INIT] Found getBB: " + s_getBB.getDeclaringClass().getName() + "." + s_getBB.getName() + " -> " + s_getBB.getReturnType().getName());

            String[][] posFieldCandidates = {
                {"s", "t", "u"},          // notch
                {"posX", "posY", "posZ"}, // MCP
                {"field_70165_t", "field_70163_u", "field_70161_v"} // SRG
            };
            for (String[] trio : posFieldCandidates) {
                try {
                    Class<?> c = entityClass;
                    while (c != null && c != Object.class) {
                        try {
                            s_posX = c.getDeclaredField(trio[0]); s_posX.setAccessible(true);
                            s_posY = c.getDeclaredField(trio[1]); s_posY.setAccessible(true);
                            s_posZ = c.getDeclaredField(trio[2]); s_posZ.setAccessible(true);
                            break;
                        } catch (NoSuchFieldException e) { s_posX = null; }
                        c = c.getSuperclass();
                    }
                    if (s_posX != null) break;
                } catch (Throwable ignored) { s_posX = null; }
            }
            if (s_posX == null) {
                log("[RENDER-INIT] FAIL: posX/posY/posZ not found");
                s_renderInitFailed = true; return;
            }
            log("[RENDER-INIT] Found pos fields: " + s_posX.getDeclaringClass().getName() + "." + s_posX.getName());

            Class<?> bbClass = s_getBB.getReturnType();
            String[][] bbFieldCandidates = {
                {"a", "b", "c", "d", "e", "f"},                     // notch
                {"minX", "minY", "minZ", "maxX", "maxY", "maxZ"},   // MCP
                {"field_72340_a", "field_72338_b", "field_72339_c", "field_72336_d", "field_72337_e", "field_72334_f"} // SRG
            };
            for (String[] names : bbFieldCandidates) {
                try {
                    s_bbMinX = bbClass.getDeclaredField(names[0]); s_bbMinX.setAccessible(true);
                    s_bbMinY = bbClass.getDeclaredField(names[1]); s_bbMinY.setAccessible(true);
                    s_bbMinZ = bbClass.getDeclaredField(names[2]); s_bbMinZ.setAccessible(true);
                    s_bbMaxX = bbClass.getDeclaredField(names[3]); s_bbMaxX.setAccessible(true);
                    s_bbMaxY = bbClass.getDeclaredField(names[4]); s_bbMaxY.setAccessible(true);
                    s_bbMaxZ = bbClass.getDeclaredField(names[5]); s_bbMaxZ.setAccessible(true);
                    break;
                } catch (NoSuchFieldException e) { s_bbMinX = null; }
            }
            if (s_bbMinX == null) {
                log("[RENDER-INIT] FAIL: AxisAlignedBB fields not found in " + bbClass.getName());
                s_renderInitFailed = true; return;
            }
            s_bbCtor = bbClass.getConstructor(double.class, double.class, double.class,
                                              double.class, double.class, double.class);
            log("[RENDER-INIT] Found BB class: " + bbClass.getName() + " with fields and ctor");

            Class<?> renderGlobalClass = null;
            try { renderGlobalClass = cl.loadClass("bfr"); } catch (Throwable t) {}
            if (renderGlobalClass == null) {
                try { renderGlobalClass = cl.loadClass("net.minecraft.client.renderer.RenderGlobal"); } catch (Throwable t) {}
            }
            if (renderGlobalClass != null) {
                for (java.lang.reflect.Method m : renderGlobalClass.getDeclaredMethods()) {
                    Class<?>[] params = m.getParameterTypes();
                    if (params.length == 5 && params[0] == bbClass &&
                        params[1] == int.class && params[2] == int.class &&
                        params[3] == int.class && params[4] == int.class &&
                        m.getReturnType() == void.class &&
                        java.lang.reflect.Modifier.isStatic(m.getModifiers())) {
                        s_drawBB = m;
                        s_drawBB.setAccessible(true);
                        break;
                    }
                }
            }
            if (s_drawBB == null) {
                log("[RENDER-INIT] FAIL: drawOutlinedBoundingBox not found");
                s_renderInitFailed = true; return;
            }
            log("[RENDER-INIT] Found drawBB: " + s_drawBB.getDeclaringClass().getName() + "." + s_drawBB.getName());

            String[] eyeNames = {"getEyeHeight", "func_70047_e", "aP", "aQ", "aR"};
            for (String eName : eyeNames) {
                Class<?> c = entityClass;
                while (c != null && c != Object.class) {
                    try {
                        java.lang.reflect.Method m = c.getDeclaredMethod(eName);
                        if (m.getReturnType() == float.class) {
                            m.setAccessible(true);
                            s_getEyeHeight = m;
                            break;
                        }
                    } catch (NoSuchMethodException ignored) {}
                    c = c.getSuperclass();
                }
                if (s_getEyeHeight != null) break;
            }

            Class<?> gl11 = cl.loadClass("org.lwjgl.opengl.GL11");
            s_glDepthMask = gl11.getMethod("glDepthMask", boolean.class);
            s_glDisable = gl11.getMethod("glDisable", int.class);
            s_glEnable = gl11.getMethod("glEnable", int.class);

            log("[RENDER-INIT] SUCCESS! Ready to bypass Badlion rendering.");

        } catch (Throwable t) {
            s_renderInitFailed = true;
            log("[RENDER-INIT] FAIL: " + t.getClass().getName() + ": " + t.getMessage());
        }
    }

    public static boolean tryRenderEntityHitbox(Object entity, double x, double y, double z, float yaw, float partialTicks) {
        try {
            if (!isTeamColoredHitboxesEnabled() || entity == null) return false;
            if (s_mcClassLoader == null) s_mcClassLoader = entity.getClass().getClassLoader();

            int col = resolveEntityColor(entity);
            if (col == -1 || col == 0xFFFFFF) return false;

            initRenderReflection(entity);
            if (s_renderInitFailed) return false;

            int r = (col >> 16) & 0xFF;
            int g = (col >> 8) & 0xFF;
            int b = col & 0xFF;

            Object bb = s_getBB.invoke(entity);
            if (bb == null) return false;

            double posXd = s_posX.getDouble(entity);
            double posYd = s_posY.getDouble(entity);
            double posZd = s_posZ.getDouble(entity);

            double minX = s_bbMinX.getDouble(bb);
            double minY = s_bbMinY.getDouble(bb);
            double minZ = s_bbMinZ.getDouble(bb);
            double maxX = s_bbMaxX.getDouble(bb);
            double maxY = s_bbMaxY.getDouble(bb);
            double maxZ = s_bbMaxZ.getDouble(bb);

            Object renderBB = s_bbCtor.newInstance(
                minX - posXd + x, minY - posYd + y, minZ - posZd + z,
                maxX - posXd + x, maxY - posYd + y, maxZ - posZd + z
            );

            // GL state setup
            s_glDepthMask.invoke(null, false);
            s_glDisable.invoke(null, 3553);  // GL_TEXTURE_2D
            s_glDisable.invoke(null, 2896);  // GL_LIGHTING
            s_glDisable.invoke(null, 2884);  // GL_CULL_FACE
            s_glDisable.invoke(null, 3042);  // GL_BLEND

            s_drawBB.invoke(null, renderBB, r, g, b, 255);

            if (s_getEyeHeight != null) {
                try {
                    float eyeH = (Float) s_getEyeHeight.invoke(entity);
                    double halfW = (maxX - minX) / 2.0;
                    Object eyeBB = s_bbCtor.newInstance(
                        x - halfW, y + (double)eyeH - 0.01, z - halfW,
                        x + halfW, y + (double)eyeH + 0.01, z + halfW
                    );
                    s_drawBB.invoke(null, eyeBB, 255, 0, 0, 255);
                } catch (Throwable ignored) {}
            }

            s_glEnable.invoke(null, 3553);   // GL_TEXTURE_2D
            s_glEnable.invoke(null, 2896);   // GL_LIGHTING
            s_glEnable.invoke(null, 2884);   // GL_CULL_FACE
            s_glDepthMask.invoke(null, true);

            s_currentEntity = entity;
            s_currentEntityColor = col;

            s_bypassCount++;
            long now = System.currentTimeMillis();
            if (now - s_lastBypassLogTime > 3000) {
                s_lastBypassLogTime = now;
                log("[BYPASS] Rendered entity " + entity.getClass().getSimpleName() +
                    " with color 0x" + Integer.toHexString(col) +
                    " (total bypassed=" + s_bypassCount + ")");
            }

            return true;
        } catch (Throwable t) {
            // Debug: log first failure
            if (s_bypassCount == 0) {
                log("[BYPASS] Error: " + t.getClass().getName() + ": " + t.getMessage());
            }
            return false;
        }
    }
    public static void onBeginRenderEntity(Object entity, double x, double y, double z) {
        try {
            s_currentEntity = null;
            s_currentEntityColor = -1;
            if (entity == null) return;
            if (s_mcClassLoader == null) s_mcClassLoader = entity.getClass().getClassLoader();

            long now = System.currentTimeMillis();
            if (now - s_lastEntityDbgTime > 5000) {
                s_lastEntityDbgTime = now;
                StringBuilder sb = new StringBuilder();
                sb.append("[ENTITY-DBG] class=").append(entity.getClass().getName());
                sb.append(" supers=[");
                Class<?> c = entity.getClass().getSuperclass();
                boolean first = true;
                while (c != null && c != Object.class) {
                    if (!first) sb.append(",");
                    sb.append(c.getName());
                    c = c.getSuperclass();
                    first = false;
                }
                sb.append("]");
                log(sb.toString());
            }

            if (!isTeamColoredHitboxesEnabled()) return;

            int col = resolveEntityColor(entity);
            if (col != -1) {
                s_currentEntity = entity;
                s_currentEntityColor = col;
            }
        } catch (Throwable t) {
            s_currentEntity = null;
            s_currentEntityColor = -1;
        }
    }

    public static void onEndRenderEntity() {
        s_currentEntity = null;
        s_currentEntityColor = -1;
    }

    public static void updateBoxColor(Object bb, int originalRed, int originalGreen, int originalBlue) {
        try {
            s_callCount++;
            s_lastBb = bb;
            s_lastOrigR = originalRed;
            s_lastOrigG = originalGreen;
            s_lastOrigB = originalBlue;

            if (!isTeamColoredHitboxesEnabled() || bb == null) {
                s_lastColor = -1;
                return;
            }

            if (originalRed == 255 && originalGreen == 0 && originalBlue == 0) {
                s_lastColor = -1;
                return;
            }

            int col = -1;
            if (s_currentEntity != null && s_currentEntityColor != -1) {
                col = s_currentEntityColor;
            } else {
                col = resolveBoxColor(bb, originalRed, originalGreen, originalBlue);
            }

            s_lastColor = col;

            if (col != -1) {
                s_resolvedCount++;
                float cr = ((col >> 16) & 0xFF) / 255.0f;
                float cg = ((col >> 8) & 0xFF) / 255.0f;
                float cb = (col & 0xFF) / 255.0f;
                setGLColor(cr, cg, cb, 1.0f);
            }

            long now = System.currentTimeMillis();
            if (now - s_lastLogTime > 2000) {
                s_lastLogTime = now;
                log("[HITBOX-HOOK] calls=" + s_callCount + " resolved=" + s_resolvedCount +
                    " currentEntity=" + (s_currentEntity != null ? s_currentEntity.getClass().getSimpleName() : "null") +
                    " origRGB=(" + originalRed + "," + originalGreen + "," + originalBlue + ")" +
                    (col != -1 ? " -> TEAM_RGB=0x" + Integer.toHexString(col) : " -> DEFAULT/WHITE"));
            }
        } catch (Throwable t) {
            s_lastColor = -1;
        }
    }

    public static int getBoxRed(Object bb, int originalRed) {
        try {
            if (s_lastColor == -1 || bb != s_lastBb) {
                updateBoxColor(bb, originalRed, s_lastOrigG, s_lastOrigB);
            }
            if (s_lastColor != -1 && (bb == s_lastBb || bb == null)) {
                return (s_lastColor >> 16) & 0xFF;
            }
            return originalRed;
        } catch (Throwable t) {
            return originalRed;
        }
    }

    public static int getBoxGreen(Object bb, int originalGreen) {
        try {
            if (s_lastColor != -1 && (bb == s_lastBb || bb == null)) {
                return (s_lastColor >> 8) & 0xFF;
            }
            return originalGreen;
        } catch (Throwable t) {
            return originalGreen;
        }
    }

    public static int getBoxBlue(Object bb, int originalBlue) {
        try {
            if (s_lastColor != -1 && (bb == s_lastBb || bb == null)) {
                return s_lastColor & 0xFF;
            }
            return originalBlue;
        } catch (Throwable t) {
            return originalBlue;
        }
    }

    public static boolean shouldRenderHitbox(Object entity) {
        return true;
    }

    private static java.lang.reflect.Constructor<?> s_uConstructor = null;
    private static boolean s_uInitDone = false;

    public static Object resolveBadlionColor(Object defaultColor, Object entity) {
        try {
            if (!isTeamColoredHitboxesEnabled() || entity == null || defaultColor == null) return defaultColor;
            if (!isPlayerEntity(entity)) return defaultColor;

            int color = resolveEntityColor(entity);
            if (color == -1 || color == 0xFFFFFF) return defaultColor;

            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = color & 0xFF;
            int a = 255;

            if (!s_uInitDone) {
                s_uInitDone = true;
                Class<?> uCls = defaultColor.getClass();
                for (java.lang.reflect.Constructor<?> c : uCls.getConstructors()) {
                    Class<?>[] pTypes = c.getParameterTypes();
                    if (pTypes.length == 4 && pTypes[0] == int.class && pTypes[1] == int.class && pTypes[2] == int.class && pTypes[3] == int.class) {
                        s_uConstructor = c;
                        break;
                    }
                }
                if (s_uConstructor == null) {
                    for (java.lang.reflect.Constructor<?> c : uCls.getConstructors()) {
                        Class<?>[] pTypes = c.getParameterTypes();
                        if (pTypes.length == 3 && pTypes[0] == int.class && pTypes[1] == int.class && pTypes[2] == int.class) {
                            s_uConstructor = c;
                            break;
                        }
                    }
                }
            }

            if (s_uConstructor != null) {
                Class<?>[] pTypes = s_uConstructor.getParameterTypes();
                if (pTypes.length == 4) {
                    return s_uConstructor.newInstance(r, g, b, a);
                } else if (pTypes.length == 3) {
                    return s_uConstructor.newInstance(r, g, b);
                }
            }
        } catch (Throwable t) {}
        return defaultColor;
    }
    public static Object getBadlionCustomColor(Object entity) {
        return null;
    }
    public static void onBadlionGetHitboxColor(Object entity) {}
    public static void onBadlionRenderHitbox(Object entity) {}
}
