cmake_minimum_required(VERSION 3.15)
project(OVsonLoader LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions
add_definitions(-DWIN32 -D_WINDOWS -D_CRT_SECURE_NO_WARNINGS)

# Source files
set(LOADER_SOURCES
    main.cpp
    Utils/Utils.cpp
    Injection/Injection.cpp
    UI/UI.cpp
    OVsonLoader.rc
)

# Headers (for IDE visibility)
set(LOADER_HEADERS
    Utils/Utils.h
    Injection/Injection.h
    UI/UI.h
)

# Create the EXE target (WIN32 subsystem)
add_executable(OVsonLoader WIN32 ${LOADER_SOURCES} ${LOADER_HEADERS})

# Include directories
target_include_directories(OVsonLoader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/UI
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Injection
)

# Link libraries
target_link_libraries(OVsonLoader PRIVATE
    User32.lib
    Comctl32.lib
    Dwmapi.lib
    Wininet.lib
    Iphlpapi.lib
    Bcrypt.lib
    Ws2_32.lib
    Ole32.lib
    OleAut32.lib
    Wbemuuid.lib
)

# Output directory settings
set_target_properties(OVsonLoader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
