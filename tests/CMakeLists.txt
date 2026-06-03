# Phase 0 uses a tiny self-contained test framework (test_framework.h) so the suite
# builds with no network access. Catch2 via FetchContent comes with the CI pass.
add_executable(tonestack_tests
    test_main.cpp
    parameter_tests.cpp
    chain_tests.cpp
    rt_safety_tests.cpp)

target_link_libraries(tonestack_tests PRIVATE tonestack::engine tonestack::nodes)
tonestack_set_warnings(tonestack_tests)

add_test(NAME tonestack_tests COMMAND tonestack_tests)
