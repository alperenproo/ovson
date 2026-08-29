cmake_minimum_required(VERSION 3.15)
project(OVson LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PROJECT_IS_TOP_LEVEL)
    option(OVSON_BUILD_TESTS "Build OVson's deterministic tests" ON)
    if(OVSON_BUILD_TESTS)
        include(CTest)
    endif()
endif()

# Definitions
add_definitions(-DWIN32 -D_WINDOWS -D_USRDLL -D_WINDLL -DOVSON_EXPORTS -D_CRT_SECURE_NO_WARNINGS)

if(MSVC)
    add_compile_options(/EHa /wd4535)
endif()

set(OVSON_SOURCES
    dllmain.cpp
    Config/Config.cpp
    Config/StatColors.cpp
    Net/Http.cpp
    Services/Hypixel.cpp
    Services/AbyssService.cpp
    Services/AuroraService.cpp
    Services/KhadowService.cpp
    Services/PrismService.cpp
    Logic/StatsTracker.cpp
    Logic/McBindings.cpp
    Logic/ScoreboardTeams.cpp
    Logic/TabListScan.cpp
    Logic/HypixelGameState.cpp
    Logic/StatsCache.cpp
    Logic/PlayerStatsFetch.cpp
    Logic/StatsPoll.cpp
    Logic/McLogTail.cpp
    Logic/PacketHook.cpp
    Logic/Bedwars/BedwarsConfig.cpp
    Logic/Bedwars/BedwarsCore.cpp
    Logic/Bedwars/BedwarsMaps.cpp
    Logic/Bedwars/BedwarsRuntime.cpp
    Logic/NickRoll/NickRollCore.cpp
    Logic/NickRoll/NickRollRuntime.cpp
    Logic/NickRoll/NickScore.cpp
    Logic/MediaSession.cpp
    Render/MediaOverlay.cpp
    Logic/BlockHitAudio.cpp
    Logic/BlockHitAudioBackend.cpp
    Logic/BlockHitHeuristic.cpp
    Logic/BlockHitSound.cpp
    Logic/BedDefense/BedDefenseManager.cpp
    Logic/BedDefense/BlockHook.cpp
    Chat/ChatAPI_Bridge.cpp
    Chat/ChatHook.cpp
    Chat/ChatSDK.cpp
    Chat/Commands.cpp
    ClickGUI/ClickGUI_Bridge.cpp
    ClickGUI/Helpers.cpp
    ClickGUI/Input.cpp
    ClickGUI/Lifecycle.cpp
    ClickGUI/LiquidGlass.cpp
    ClickGUI/Render.cpp
    ClickGUI/State.cpp
    ClickGUI/Tabs/Bedwars.cpp
    ClickGUI/Tabs/Colors.cpp
    ClickGUI/Tabs/Debug.cpp
    ClickGUI/Tabs/Players.cpp
    ClickGUI/Tabs/Plugins.cpp
    ClickGUI/Tabs/Settings.cpp
    ClickGUI/Tabs/Tags.cpp
    ClickGUI/Tabs/Utils.cpp
    ClickGUI/Tabs/Visuals.cpp
    JavaHook/JavaHook.cpp
    Plugins/PluginLoader.cpp
    Utils/Logger.cpp
    Utils/CrashDump.cpp
    Utils/NumberDenicker.cpp
    Utils/Watchdog.cpp
    SDK/Minecraft.cpp
    SDK/Player.cpp
    Render/BetterTab.cpp
    Render/BedwarsOverlay.cpp
    Render/Framebuffer.cpp
    Render/FontRenderer.cpp
    Render/GL.cpp
    Render/StatsOverlay.cpp
    Render/RenderHook.cpp
    Render/DefenseRenderer.cpp
    Render/NameTagRenderer.cpp
    Render/NotificationManager.cpp
    Render/RenderUtils.cpp
    Render/Shader.cpp
    Render/TextureLoader.cpp
    Render/TechOverlay.cpp
    Logic/AutoGG.cpp
    Utils/Timer.cpp
    Utils/ChatBypasser.cpp
    Utils/ThreadTracker.cpp
    Utils/Anticheat/AcCore.cpp
    Utils/Anticheat/AcChecks.cpp
    Services/UrchinService.cpp
    Services/SeraphService.cpp
    Services/DiscordManager.cpp
    Utils/ReplaySpammer.cpp
    Resource.rc
)

set(OVSON_HEADERS
    Java.h
    resource.h
    Config/Config.h
    Config/StatColors.h
    Net/Http.h
    Services/Hypixel.h
    Logic/StatsTracker.h
    Logic/StatsTracker.internal.h
    Logic/PacketHook.h
    Logic/Bedwars/BedwarsConfig.h
    Logic/Bedwars/BedwarsCore.h
    Logic/Bedwars/BedwarsRuntime.h
    Logic/NickRoll/NickRollCore.h
    Logic/NickRoll/NickRollRuntime.h
    Logic/NickRoll/NickScore.h
    Logic/MediaSession.h
    Render/MediaOverlay.h
    Logic/BlockHitHeuristic.h
    Logic/BlockHitAudio.h
    Logic/BlockHitAudioBackend.h
    Chat/ChatHook.h
    Chat/ChatSDK.h
    Chat/Commands.h
    ClickGUI/ClickGUI.h
    ClickGUI/ClickGUI_Bridge.h
    ClickGUI/Helpers.h
    ClickGUI/LiquidGlass.h
    ClickGUI/State.h
    ClickGUI/Tabs/Tabs.h
    ClickGUI/Theme.h
    JavaHook/JavaHook.h
    Plugins/PluginLoader.h
    Utils/Logger.h
    Utils/SensitivityFix.h
    Utils/Timer.h
    Utils/ThreadTracker.h
    Utils/stb_image.h
    Utils/BedwarsPrestiges.h
    SDK/Minecraft.h
    SDK/Player.h
    Render/BetterTab.h
    Render/BedwarsOverlay.h
    Render/Framebuffer.h
    Render/FontRenderer.h
    Render/GL.h
    Render/StatsOverlay.h
    Render/RenderHook.h
    Render/DefenseRenderer.h
    Render/NameTagRenderer.h
    Render/NotificationManager.h
    Render/RenderUtils.h
    Render/Shader.h
    Render/TextureLoader.h
    Render/TechOverlay.h
    Logic/BedDefense/BedDefenseManager.h
    Logic/BedDefense/BlockHook.h
    Logic/AutoGG.h
    Logic/PacketHook.h
    Logic/BlockHitSound.h
    Logic/PacketFilterHook_bytes.h
    Logic/PacketFilterHook.java
    Utils/ChatBypasser.h
    Utils/CrashDump.h
    Utils/NumberDenicker.h
    Utils/SafeGuard.h
    Utils/Watchdog.h
    Utils/Anticheat/Anticheat.h
    Utils/Anticheat/AcInternal.h
    Services/UrchinService.h
    Services/SeraphService.h
    Services/AbyssService.h
    Services/AuroraService.h
    Services/KhadowService.h
    Services/PrismService.h
    Services/DiscordManager.h
    Utils/ReplaySpammer.h
)

if(MSVC)
    set_source_files_properties(
        dllmain.cpp
        Config/Config.cpp
        Chat/ChatHook.cpp
        ClickGUI/Render.cpp
        ClickGUI/Tabs/Utils.cpp
        Logic/HypixelGameState.cpp
        Logic/PacketHook.cpp
        Logic/StatsPoll.cpp
        Logic/Bedwars/BedwarsConfig.cpp
        Logic/Bedwars/BedwarsCore.cpp
        Logic/Bedwars/BedwarsMaps.cpp
        Logic/Bedwars/BedwarsRuntime.cpp
        Logic/BlockHitAudio.cpp
        Logic/BlockHitAudioBackend.cpp
        Logic/BlockHitSound.cpp
        Logic/BedDefense/BedDefenseManager.cpp
        Logic/BedDefense/BlockHook.cpp
        Render/DefenseRenderer.cpp
        Render/NotificationManager.cpp
        Render/RenderHook.cpp
        Render/BedwarsOverlay.cpp
        ClickGUI/Tabs/Bedwars.cpp
        PROPERTIES COMPILE_OPTIONS "/W4"
    )
endif()

# The DLL embeds the plugin API. Build it in the binary tree so a clean
# checkout does not require a pre-generated JAR in the source directory.
find_package(Java COMPONENTS Development QUIET)
if(NOT Java_FOUND)
    message(FATAL_ERROR
        "Building OVson requires a JDK with javac and jar. Install a JDK that "
        "supports 'javac --release 8', or set Java_JAVAC_EXECUTABLE and "
        "Java_JAR_EXECUTABLE explicitly.")
endif()

set(OVSON_API_SOURCES
    ../OVsonAPI/src/net/ovson/api/OVsonAPI.java
    ../OVsonAPI/src/net/ovson/api/OVsonPlugin.java
    ../OVsonAPI/src/net/ovson/api/PluginInfo.java
    ../OVsonAPI/src/net/ovson/api/PluginManager.java
    ../OVsonAPI/src/net/ovson/api/chat/ChatAPI.java
    ../OVsonAPI/src/net/ovson/api/clickgui/ClickGUI.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomChoice.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomInput.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomModule.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomSetting.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomSlider.java
    ../OVsonAPI/src/net/ovson/api/clickgui/CustomToggle.java
    ../OVsonAPI/src/net/ovson/api/clickgui/ModuleCallback.java
    ../OVsonAPI/src/net/ovson/api/clickgui/SettingCallback.java
    ../OVsonAPI/src/net/ovson/api/event/Cancellable.java
    ../OVsonAPI/src/net/ovson/api/event/ChatReceivedEvent.java
    ../OVsonAPI/src/net/ovson/api/event/ChatSendEvent.java
    ../OVsonAPI/src/net/ovson/api/event/Event.java
    ../OVsonAPI/src/net/ovson/api/event/EventBus.java
    ../OVsonAPI/src/net/ovson/api/event/EventHandler.java
    ../OVsonAPI/src/net/ovson/api/event/PlayerJoinEvent.java
    ../OVsonAPI/src/net/ovson/api/event/Render2DEvent.java
    ../OVsonAPI/src/net/ovson/api/event/TickEvent.java
    ../OVsonAPI/src/net/ovson/api/event/network/PacketEvent.java
    ../OVsonAPI/src/net/ovson/api/event/network/PacketReceiveEvent.java
    ../OVsonAPI/src/net/ovson/api/event/network/PacketSendEvent.java
    ../OVsonAPI/src/net/ovson/api/event/player/AttackEvent.java
    ../OVsonAPI/src/net/ovson/api/event/player/UpdateEvent.java
    ../OVsonAPI/src/net/ovson/api/hook/ClassTransformer.java
    ../OVsonAPI/src/net/ovson/api/hook/TransformerRegistry.java
    ../OVsonAPI/src/net/ovson/api/player/PlayerAPI.java
    ../OVsonAPI/src/net/ovson/api/player/PlayerStats.java
    ../OVsonAPI/src/net/ovson/api/render/RenderAPI.java
)
list(TRANSFORM OVSON_API_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(OVSON_API_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/OVsonAPI-classes")
set(OVSON_API_JAR "${CMAKE_CURRENT_BINARY_DIR}/OVsonAPI.jar")
add_custom_command(
    OUTPUT "${OVSON_API_JAR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${OVSON_API_CLASSES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OVSON_API_CLASSES_DIR}"
    COMMAND "${Java_JAVAC_EXECUTABLE}" --release 8
            -d "${OVSON_API_CLASSES_DIR}" ${OVSON_API_SOURCES}
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${OVSON_API_JAR}"
            -C "${OVSON_API_CLASSES_DIR}" .
    DEPENDS ${OVSON_API_SOURCES}
    COMMENT "Building Java 8-compatible OVsonAPI.jar"
    VERBATIM
)
add_custom_target(OVsonAPIJar DEPENDS "${OVSON_API_JAR}")

# Create the DLL target
add_library(OVson SHARED ${OVSON_SOURCES} ${OVSON_HEADERS})
add_dependencies(OVson OVsonAPIJar)
set_property(SOURCE Resource.rc APPEND PROPERTY COMPILE_DEFINITIONS
    "OVSON_API_JAR_PATH=\"${OVSON_API_JAR}\"")

target_include_directories(OVson PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/JNI
    ${CMAKE_CURRENT_SOURCE_DIR}/MinHook
)

target_link_libraries(OVson PRIVATE
    opengl32.lib
    # C++/WinRT (Windows.Media.Control) for the now-playing overlay.
    WindowsApp.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/JNI/jvm.lib
    Bcrypt.lib
    Iphlpapi.lib
    Winhttp.lib
    wbemuuid.lib
    Ws2_32.lib
    Advapi32.lib
    Shell32.lib
    Dbghelp.lib
    Dsound.lib
    Dxguid.lib
)

set_target_properties(OVson PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(OVSON_BUILD_TESTS AND BUILD_TESTING
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_executable(OVsonBedwarsTests
        Logic/Bedwars/BedwarsConfig.cpp
        Logic/Bedwars/BedwarsConfig.h
        Logic/Bedwars/BedwarsCore.cpp
        Logic/Bedwars/BedwarsMaps.cpp
        Logic/Bedwars/BedwarsCore.h
        ../OVsonLoader/slint_ui/shutdown_coordinator.cpp
        ../OVsonLoader/slint_ui/shutdown_coordinator.h
        tests/BedwarsTests.cpp
    )
    target_include_directories(OVsonBedwarsTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(OVsonBedwarsTests PRIVATE /W4)
    endif()
    add_test(NAME OVson.Bedwars COMMAND OVsonBedwarsTests)

    add_executable(OVsonBlockHitHeuristicTests
        Logic/BlockHitHeuristic.cpp
        Logic/BlockHitHeuristic.h
        tests/BlockHitHeuristicTests.cpp
    )
    target_include_directories(OVsonBlockHitHeuristicTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(OVsonBlockHitHeuristicTests PRIVATE /W4)
    endif()
    add_test(NAME OVson.BlockHitHeuristic
             COMMAND OVsonBlockHitHeuristicTests)

    add_executable(OVsonBlockHitAudioTests
        Logic/BlockHitAudio.cpp
        Logic/BlockHitAudio.h
        tests/BlockHitAudioTests.cpp
    )
    target_include_directories(OVsonBlockHitAudioTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(OVsonBlockHitAudioTests PRIVATE /W4)
    endif()
    add_test(NAME OVson.BlockHitAudio
             COMMAND OVsonBlockHitAudioTests)

    # The /nick command is limited to six uses a day, so everything that can
    # be settled without the game is settled here instead.
    add_executable(OVsonNickRollTests
        Logic/NickRoll/NickRollCore.cpp
        Logic/NickRoll/NickRollCore.h
        tests/NickRollTests.cpp
    )
    target_include_directories(OVsonNickRollTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(OVsonNickRollTests PRIVATE /W4)
    endif()
    add_test(NAME OVson.NickRoll COMMAND OVsonNickRollTests)

    add_executable(OVsonNickScoreTests
        Logic/NickRoll/NickScore.cpp
        Logic/NickRoll/NickScore.h
        tests/NickScoreTests.cpp
    )
    target_include_directories(OVsonNickScoreTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        target_compile_options(OVsonNickScoreTests PRIVATE /W4)
    endif()
    add_test(NAME OVson.NickScore COMMAND OVsonNickScoreTests)

    add_executable(OVsonNickScoreParity
        Logic/NickRoll/NickScore.cpp
        Logic/NickRoll/NickScore.h
        tests/NickScoreParity.cpp
    )
    target_include_directories(OVsonNickScoreParity PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME OVson.NickScoreParity
             COMMAND OVsonNickScoreParity
                     ${CMAKE_CURRENT_SOURCE_DIR}/../../nickscore-parity.tsv)

    add_test(
        NAME OVson.BedDefenseRestoration
        COMMAND ${CMAKE_COMMAND}
                "-DOVSON_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/BedDefenseRestoration.cmake"
    )
endif()
