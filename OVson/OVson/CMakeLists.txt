cmake_minimum_required(VERSION 3.15)
project(OVson LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions
add_definitions(-DWIN32 -D_WINDOWS -D_USRDLL -D_WINDLL -DOVSON_EXPORTS -D_CRT_SECURE_NO_WARNINGS)

# Source files
set(OVSON_SOURCES
    dllmain.cpp
    Config/Config.cpp
    Net/Http.cpp
    Services/Hypixel.cpp
    Chat/ChatInterceptor.cpp
    Chat/ChatHook.cpp
    Chat/ChatSDK.cpp
    Chat/Commands.cpp
    Utils/Logger.cpp
    SDK/Minecraft.cpp
    SDK/Player.cpp
    Render/FontRenderer.cpp
    Render/StatsOverlay.cpp
    Render/RenderHook.cpp
    Render/DefenseRenderer.cpp
    Render/TextureLoader.cpp
    Render/NotificationManager.cpp
    Render/ClickGUI.cpp
    Render/RenderUtils.cpp
    Render/TechOverlay.cpp
    Logic/BedDefense/BedDefenseManager.cpp
    Logic/BedDefense/BlockHook.cpp
    Logic/AutoGG.cpp
    Utils/Timer.cpp
    Utils/ChatBypasser.cpp
    Utils/ThreadTracker.cpp
    Services/UrchinService.cpp
    Services/SeraphService.cpp
    Services/DiscordManager.cpp
    Utils/ReplaySpammer.cpp
    Resource.rc
)

# Headers (for IDE visibility)
set(OVSON_HEADERS
    Java.h
    resource.h
    Config/Config.h
    Net/Http.h
    Services/Hypixel.h
    Chat/ChatInterceptor.h
    Chat/ChatHook.h
    Chat/ChatSDK.h
    Chat/Commands.h
    Utils/Logger.h
    Utils/SensitivityFix.h
    Utils/Timer.h
    Utils/ThreadTracker.h
    Utils/stb_image.h
    Utils/BedwarsPrestiges.h
    JNI/classfile_constants.h
    JNI/jawt.h
    JNI/jawt_md.h
    JNI/jdwpTransport.h
    JNI/jni.h
    JNI/jni_md.h
    JNI/jvmti.h
    JNI/jvmticmlr.h
    SDK/Minecraft.h
    SDK/Player.h
    Render/FontRenderer.h
    Render/StatsOverlay.h
    Render/RenderHook.h
    Render/DefenseRenderer.h
    Render/NotificationManager.h
    Render/ClickGUI.h
    Render/RenderUtils.h
    Render/TextureLoader.h
    Render/TechOverlay.h
    Logic/BedDefense/BedDefenseManager.h
    Logic/BedDefense/BlockHook.h
    Logic/AutoGG.h
    Utils/ChatBypasser.h
    Services/UrchinService.h
    Services/SeraphService.h
    Services/DiscordManager.h
    Utils/ReplaySpammer.h
)

# Create the DLL target
add_library(OVson SHARED ${OVSON_SOURCES} ${OVSON_HEADERS})

# Include directories
target_include_directories(OVson PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/JNI
    ${CMAKE_CURRENT_SOURCE_DIR}/MinHook
)

# Link libraries
target_link_libraries(OVson PRIVATE
    opengl32.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/JNI/jvm.lib
    Bcrypt.lib
    Iphlpapi.lib
    Winhttp.lib
    wbemuuid.lib
    Ws2_32.lib
    Advapi32.lib
    Shell32.lib
)

# Output directory settings (standardize across VS versions)
set_target_properties(OVson PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
