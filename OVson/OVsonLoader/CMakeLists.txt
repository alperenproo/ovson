cmake_minimum_required(VERSION 3.21)
project(OVsonLoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the pre-installed Slint C++ release. Adjust SLINT_INSTALL_ROOT
# if you installed Slint to a different location. The installer drops
# a CMake config file under lib/cmake/Slint that exposes the
# Slint::Slint target + slint_target_sources() helper.
set(SLINT_INSTALL_ROOT "C:/Program Files/Slint-cpp 1.16.1"
    CACHE PATH "Slint C++ install root")
list(APPEND CMAKE_PREFIX_PATH "${SLINT_INSTALL_ROOT}")

find_package(Slint REQUIRED)

# Everything UI-related lives in slint_ui/. The old UI/, Injection/
# and Utils/ folders are left in place but no longer compiled — they
# carry the GDI+ loader history if you want to look back.
add_executable(OVsonLoader WIN32
    slint_ui/main.cpp
    slint_ui/injector.cpp
    slint_ui/process_scan.cpp
    slint_ui/updater.cpp
    OVsonLoader.rc
)

# slint_target_sources runs the .slint → C++ codegen step. The
# generated header is dropped into the target's include search path
# and is then #include'd from main.cpp as "ovson_ui.h".
slint_target_sources(OVsonLoader slint_ui/ovson_ui.slint)

target_include_directories(OVsonLoader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/slint_ui
)

# Block the Windows <minwindef.h> min/max macros — they collide with
# std::max/min used heavily inside Slint's templated headers and
# produce dozens of unreadable parse errors.
target_compile_definitions(OVsonLoader PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(OVsonLoader PRIVATE
    Slint::Slint
    User32.lib
    Shell32.lib
)

set_target_properties(OVsonLoader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy the Slint runtime DLLs next to the EXE after every build so the
# loader actually starts when launched outside of Visual Studio.
if(WIN32)
    add_custom_command(TARGET OVsonLoader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:OVsonLoader>
            $<TARGET_FILE_DIR:OVsonLoader>
        COMMAND_EXPAND_LISTS)
endif()
