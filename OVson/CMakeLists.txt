cmake_minimum_required(VERSION 3.15)
project(OVsonSolution LANGUAGES C CXX RC)

# Global settings
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OVSON_BUILD_LOADER "Build the Slint-based OVson loader" ON)
option(OVSON_BUILD_TESTS "Build OVson's deterministic tests" ON)

if(OVSON_BUILD_TESTS)
    include(CTest)
endif()

# Add subdirectories
add_subdirectory(OVson)
if(OVSON_BUILD_LOADER)
    add_subdirectory(OVsonLoader)
endif()
