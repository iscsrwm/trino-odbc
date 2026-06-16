cmake_minimum_required(VERSION 3.16)
project(trino_odbc_driver VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Find dependencies
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# json-c for JSON parsing. Prefer the CMake package config (how vcpkg and newer
# distros ship it); fall back to pkg-config (typical on Linux distro packages).
find_package(json-c CONFIG QUIET)
if(json-c_FOUND)
    set(JSON_C_TARGET json-c::json-c)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSON_C REQUIRED json-c)
    include_directories(${JSON_C_INCLUDE_DIRS})
    link_directories(${JSON_C_LIBRARY_DIRS})
    set(JSON_C_TARGET ${JSON_C_LIBRARIES})
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Sanitizers
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Coverage
if(ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Add subdirectories
add_subdirectory(src)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation
install(DIRECTORY include/ DESTINATION include)
install(TARGETS trino_odbc DESTINATION lib)
