# Registers a CTest that fails if the engine or node sources ever include a plugin/GUI
# SDK header. This keeps the open core plugin-SDK-agnostic and GPL-free.

add_test(
    NAME engine_sdk_isolation
    COMMAND ${CMAKE_COMMAND}
            "-DSCAN_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/engine;${CMAKE_CURRENT_SOURCE_DIR}/nodes"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EngineGuardScan.cmake")
