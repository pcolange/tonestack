add_executable(tonestack-harness src/main.cpp)

target_link_libraries(tonestack-harness PRIVATE tonestack::engine tonestack::nodes)
tonestack_set_warnings(tonestack-harness)

# The Rangemaster demo consumes circuitc-generated tables; enabled when the venv built them.
if(TONESTACK_CODEGEN_AVAILABLE)
    add_dependencies(tonestack-harness tonestack_codegen)
    target_include_directories(tonestack-harness PRIVATE ${CMAKE_SOURCE_DIR}/nodes/generated)
    target_compile_definitions(tonestack-harness PRIVATE TONESTACK_HAVE_GENERATED=1)
endif()
