package net.ovson.api.hook;

import io.netty.channel.ChannelDuplexHandler;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Inbound Netty hook embedded into PacketFilterHook_bytes.h.
 *
 * Block-hit detection deliberately starts here so its hurt, swing, velocity,
 * health, explosion, respawn, and disconnect signals are server-originated
 * packets. The native detector still labels the result heuristic because
 * Minecraft 1.8.9 does not serialize whether the server reduced damage due to
 * sword blocking.
 */
@ChannelHandler.Sharable
public class PacketFilterHook extends ChannelDuplexHandler {
    private static final int SIGNAL_HURT = 1;
    private static final int SIGNAL_SWING = 2;
    private static final int SIGNAL_VELOCITY = 3;
    private static final int SIGNAL_HEALTH = 4;
    private static final int SIGNAL_RESPAWN = 5;
    private static final int SIGNAL_DISCONNECT = 6;
    private static final int SIGNAL_EXPLOSION = 7;

    private static Method getUnformattedMethod = null;
    private static Method componentToJsonMethod = null;
    private static Method jsonToComponentMethod = null;
    private static Field chatComponentField = null;
    private static Class<?> s02Class = null;
    private static Class<?> fyClass = null;

    public static native String processIncomingChat(String unformatted, String rawJson);
    public static native void logDebug(String message);
    private static native void onServerPacket(int kind, int entityId,
                                               int data1, int data2, int data3,
                                               float value1, float value2,
                                               float value3);

    private static void dbg(String message) {
        try {
            logDebug(message);
        } catch (Throwable ignored) {
        }
    }

    private static boolean isPacket(String name, String deobfuscatedName,
                                    String obfuscatedName) {
        return name.endsWith(deobfuscatedName) || name.equals(obfuscatedName);
    }

    private static Number readNumberField(Object packet, Class<?> primitiveType,
                                          int primitiveOrdinal,
                                          String... preferredNames)
            throws IllegalAccessException {
        Class<?> type = packet.getClass();
        for (String name : preferredNames) {
            try {
                Field field = type.getDeclaredField(name);
                if (field.getType() == primitiveType) {
                    field.setAccessible(true);
                    return (Number) field.get(packet);
                }
            } catch (NoSuchFieldException ignored) {
            }
        }

        int ordinal = 0;
        for (Field field : type.getDeclaredFields()) {
            if (field.getType() != primitiveType) {
                continue;
            }
            if (ordinal++ == primitiveOrdinal) {
                field.setAccessible(true);
                return (Number) field.get(packet);
            }
        }
        return null;
    }

    private static void inspectServerPacket(Object packet) {
        try {
            Class<?> type = packet.getClass();
            String name = type.getName();

            if (isPacket(name, "S0BPacketAnimation", "fq")) {
                Number entity = readNumberField(packet, Integer.TYPE, 0,
                                                "entityID", "entityId",
                                                "field_148981_a", "a");
                Number animation = readNumberField(packet, Integer.TYPE, 1,
                                                   "type", "animationType",
                                                   "field_148980_b", "b");
                if (entity != null && animation != null && animation.intValue() == 0) {
                    onServerPacket(SIGNAL_SWING, entity.intValue(), 0, 0, 0,
                                   0.0f, 0.0f, 0.0f);
                }
                return;
            }

            if (isPacket(name, "S19PacketEntityStatus", "gi")) {
                Number entity = readNumberField(packet, Integer.TYPE, 0,
                                                "entityId", "field_149164_a", "a");
                Number status = readNumberField(packet, Byte.TYPE, 0,
                                                "logicOpcode", "field_149163_b", "b");
                // Status 2 is Entity#handleStatusUpdate's generic hurt event.
                if (entity != null && status != null && status.byteValue() == 2) {
                    onServerPacket(SIGNAL_HURT, entity.intValue(), 0, 0, 0,
                                   0.0f, 0.0f, 0.0f);
                }
                return;
            }

            if (isPacket(name, "S12PacketEntityVelocity", "hm")) {
                Number entity = readNumberField(packet, Integer.TYPE, 0,
                                                "entityID", "entityId",
                                                "field_149417_a", "a");
                Number motionX = readNumberField(packet, Integer.TYPE, 1,
                                                 "motionX", "field_149415_b", "b");
                Number motionY = readNumberField(packet, Integer.TYPE, 2,
                                                 "motionY", "field_149416_c", "c");
                Number motionZ = readNumberField(packet, Integer.TYPE, 3,
                                                 "motionZ", "field_149414_d", "d");
                if (entity != null && motionX != null && motionY != null && motionZ != null) {
                    onServerPacket(SIGNAL_VELOCITY, entity.intValue(),
                                   motionX.intValue(), motionY.intValue(), motionZ.intValue(),
                                   0.0f, 0.0f, 0.0f);
                }
                return;
            }

            if (isPacket(name, "S06PacketUpdateHealth", "hp")) {
                Number health = readNumberField(packet, Float.TYPE, 0,
                                                "health", "field_149336_a", "a");
                if (health != null) {
                    onServerPacket(SIGNAL_HEALTH, -1, 0, 0, 0,
                                   health.floatValue(), 0.0f, 0.0f);
                }
                return;
            }

            if (isPacket(name, "S07PacketRespawn", "he")) {
                onServerPacket(SIGNAL_RESPAWN, -1, 0, 0, 0,
                               0.0f, 0.0f, 0.0f);
                return;
            }

            if (isPacket(name, "S40PacketDisconnect", "gh")) {
                onServerPacket(SIGNAL_DISCONNECT, -1, 0, 0, 0,
                               0.0f, 0.0f, 0.0f);
                return;
            }

            if (isPacket(name, "S27PacketExplosion", "gk")) {
                onServerPacket(SIGNAL_EXPLOSION, -1, 0, 0, 0,
                               0.0f, 0.0f, 0.0f);
            }
        } catch (Throwable ignored) {
            // Packet inspection must never interfere with vanilla handling.
        }
    }

    @Override
    public void channelRead(ChannelHandlerContext context, Object packet) throws Exception {
        inspectServerPacket(packet);

        try {
            Class<?> type = packet.getClass();
            boolean isChat = false;
            String className = null;
            if (s02Class != null && type == s02Class) {
                isChat = true;
            } else if (fyClass != null && type == fyClass) {
                isChat = true;
            } else {
                className = type.getName();
                if (className.endsWith("S02PacketChat")) {
                    s02Class = type;
                    isChat = true;
                } else if (className.equals("fy")) {
                    fyClass = type;
                    isChat = true;
                }
            }

            if (isChat) {
                if (chatComponentField == null) {
                    try {
                        chatComponentField = type.getDeclaredField("chatComponent");
                    } catch (Exception first) {
                        try {
                            chatComponentField = type.getDeclaredField("a");
                        } catch (Exception second) {
                            for (Field field : type.getDeclaredFields()) {
                                String fieldType = field.getType().getName();
                                if (fieldType.contains("IChatComponent") || fieldType.equals("eu")) {
                                    chatComponentField = field;
                                    break;
                                }
                            }
                        }
                    }
                    if (chatComponentField != null) {
                        chatComponentField.setAccessible(true);
                        dbg("[Java-PacketFilter] Found chatComponentField: "
                            + chatComponentField.getName());
                    } else {
                        dbg("[Java-PacketFilter] ERROR: Could not find chatComponentField in "
                            + className);
                    }
                }

                Object component = chatComponentField != null
                        ? chatComponentField.get(packet) : null;
                if (component != null) {
                    if (getUnformattedMethod == null) {
                        try {
                            getUnformattedMethod = component.getClass().getMethod(
                                    "getUnformattedText");
                        } catch (Exception ignored) {
                            for (Method method : component.getClass().getMethods()) {
                                String name = method.getName();
                                if (method.getReturnType() == String.class
                                        && method.getParameterTypes().length == 0
                                        && (name.equals("c")
                                            || name.equals("func_150260_c")
                                            || name.equals("getUnformattedText"))) {
                                    getUnformattedMethod = method;
                                    break;
                                }
                            }
                            if (getUnformattedMethod == null) {
                                for (Method method : component.getClass().getMethods()) {
                                    if (method.getReturnType() == String.class
                                            && method.getParameterTypes().length == 0) {
                                        getUnformattedMethod = method;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    Class<?> serializerClass = null;
                    try {
                        serializerClass = Class.forName(
                                "net.minecraft.util.IChatComponent$Serializer");
                    } catch (Exception first) {
                        try {
                            serializerClass = Class.forName("eu$a");
                        } catch (Exception second) {
                            try {
                                serializerClass = Class.forName("ev");
                            } catch (Exception ignored) {
                            }
                        }
                    }

                    if (serializerClass != null) {
                        if (componentToJsonMethod == null) {
                            for (Method method : serializerClass.getDeclaredMethods()) {
                                if (method.getReturnType() == String.class
                                        && method.getParameterTypes().length == 1) {
                                    componentToJsonMethod = method;
                                    break;
                                }
                            }
                        }
                        if (jsonToComponentMethod == null) {
                            for (Method method : serializerClass.getDeclaredMethods()) {
                                Class<?>[] parameters = method.getParameterTypes();
                                if (parameters.length == 1
                                        && parameters[0] == String.class
                                        && method.getReturnType() != String.class) {
                                    jsonToComponentMethod = method;
                                    break;
                                }
                            }
                        }
                    }

                    String unformatted = getUnformattedMethod != null
                            ? (String) getUnformattedMethod.invoke(component) : "";
                    String rawJson = componentToJsonMethod != null
                            ? (String) componentToJsonMethod.invoke(null, component) : "";
                    if (rawJson != null && !rawJson.isEmpty()) {
                        String processed = processIncomingChat(unformatted, rawJson);
                        if ("[CANCEL]".equals(processed)) {
                            return;
                        }
                        if (processed != null && !processed.isEmpty()
                                && !processed.equals(rawJson)
                                && jsonToComponentMethod != null) {
                            Object replacement = jsonToComponentMethod.invoke(null, processed);
                            if (replacement != null) {
                                chatComponentField.set(packet, replacement);
                            }
                        }
                    }
                }
            }
        } catch (Throwable throwable) {
            dbg("[Java-PacketFilter] EXCEPTION: " + throwable.toString());
        }

        super.channelRead(context, packet);
    }
}
