cmake_minimum_required(VERSION 3.16)
# Enable the RC (resource compiler) language on Windows so the setup dialog
# resource (src/setup/setup.rc) is compiled into the driver DLL. RC is only
# meaningful on Windows; listing it is harmless elsewhere as it's auto-skipped.
if(WIN32)
    project(trino_odbc_driver VERSION 1.0.0 LANGUAGES C RC)
else()
    project(trino_odbc_driver VERSION 1.0.0 LANGUAGES C)
endif()

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

# Statically link the MSVC C runtime so the driver DLL has no external runtime
# dependency (no VC++ redistributable required on target machines). Must be set
# before any target is created. Pair this with the x64-windows-static vcpkg
# triplet so curl/json-c also use the static CRT.
option(TRINO_ODBC_STATIC_CRT "Statically link the MSVC runtime (Windows)" OFF)
if(TRINO_ODBC_STATIC_CRT AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# When building against static vcpkg dependencies (x64-windows-static[-md]),
# the curl/json-c CMake package configs carry the static archives plus their
# transitive system libraries (ws2_32, crypt32, ...). Prefer CONFIG mode so
# those usage requirements are picked up.
option(TRINO_ODBC_STATIC_DEPS "Link curl/json-c statically (vcpkg static triplet)" OFF)

# Find dependencies
if(TRINO_ODBC_STATIC_DEPS)
    find_package(CURL CONFIG REQUIRED)
else()
    find_package(CURL REQUIRED)
endif()
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
