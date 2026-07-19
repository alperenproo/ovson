package net.ovson.api.hook;

/**
 * Interface for bytecode transformers.
 * Plugins implement this to modify class bytecode at load time.
 */
public interface ClassTransformer {

    /**
     * Called when a class is being loaded. Return null to skip transformation,
     * or return a modified byte array to replace the class bytecode.
     *
     * @param className  The fully qualified class name (e.g. "net.minecraft.client.Minecraft")
     * @param classBytes The original bytecode of the class
     * @return Modified bytecode, or null to leave unchanged
     */
    byte[] transform(String className, byte[] classBytes);
}
