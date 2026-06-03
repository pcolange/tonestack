cmake_minimum_required(VERSION 3.21)

project(tonestack
    VERSION 0.0.1
    DESCRIPTION "Hybrid circuit-modeled + neural guitar amp/effects DSP framework"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(TONESTACK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(cmake/CompilerWarnings.cmake)

enable_testing()

# Offline circuit compiler codegen (optional; gated on the Python venv existing).
include(cmake/Codegen.cmake)

add_subdirectory(engine)
add_subdirectory(nodes)
add_subdirectory(harness)
add_subdirectory(tests)

# CI gate: the engine and node libraries must never depend on a plugin/GUI SDK.
include(cmake/EngineGuard.cmake)
