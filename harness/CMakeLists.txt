add_executable(tonestack-harness src/main.cpp)

target_link_libraries(tonestack-harness PRIVATE tonestack::engine tonestack::nodes)
tonestack_set_warnings(tonestack-harness)
