#pragma once

#include <jni.h>

namespace JavaHook {

    /**
     * Installs the JVMTI ClassFileLoadHook callback.
     * This allows plugins to transform class bytecode as classes are loaded.
     * Must be called after JVM is attached and PluginLoader is initialized.
     */
    void initialize();

    /**
     * Removes hooks and cleans up.
     */
    void shutdown();

    /**
     * Returns true if the hook system is active.
     */
    bool isActive();

    /**
     * Returns the number of classes that have been transformed.
     */
    int getTransformCount();
}
