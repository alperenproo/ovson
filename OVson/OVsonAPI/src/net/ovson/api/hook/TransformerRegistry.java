package net.ovson.api.hook;

import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Registry for class bytecode transformers.
 * Called from C++ JVMTI ClassFileLoadHook callback.
 * Plugins register their transformers here to intercept and modify class loading.
 */
public final class TransformerRegistry {
    private static final List<ClassTransformer> TRANSFORMERS = new CopyOnWriteArrayList<>();

    /**
     * Register a new class transformer.
     * @param transformer The transformer to register
     */
    public static void register(ClassTransformer transformer) {
        if (transformer != null && !TRANSFORMERS.contains(transformer)) {
            TRANSFORMERS.add(transformer);
        }
    }

    /**
     * Request the JVM to retransform (re-run transformers on) an already loaded class.
     * @param className Dot-separated fully qualified class name (e.g. "net.minecraft.client.Minecraft")
     */
    public static native void retransform(String className);

    /**
     * Unregister a class transformer.
     * @param transformer The transformer to remove
     */
    public static void unregister(ClassTransformer transformer) {
        TRANSFORMERS.remove(transformer);
    }

    /**
     * Clear all registered transformers.
     */
    public static void clear() {
        TRANSFORMERS.clear();
    }

    /**
     * Called from C++ JVMTI hook. Runs all registered transformers in order.
     * Returns null if no transformer modifies the class, otherwise returns
     * the final modified bytecode.
     *
     * @param className  Dot-separated class name
     * @param classBytes Original bytecode
     * @return Modified bytecode or null
     */
    public static byte[] transform(String className, byte[] classBytes) {
        if (TRANSFORMERS.isEmpty()) return null;

        byte[] current = classBytes;
        boolean modified = false;

        for (ClassTransformer transformer : TRANSFORMERS) {
            try {
                byte[] result = transformer.transform(className, current);
                if (result != null && result != current) {
                    current = result;
                    modified = true;
                }
            } catch (Throwable t) {
                System.err.println("[OVsonAPI] Transformer threw on class " + className + ": " + t.getMessage());
            }
        }

        return modified ? current : null;
    }

    /**
     * Returns the number of registered transformers.
     */
    public static int getTransformerCount() {
        return TRANSFORMERS.size();
    }
}
