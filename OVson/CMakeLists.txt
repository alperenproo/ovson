cmake_minimum_required(VERSION 3.15)
project(OVsonSolution LANGUAGES C CXX RC)

# Global settings
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(OVson)
add_subdirectory(OVsonLoader)
