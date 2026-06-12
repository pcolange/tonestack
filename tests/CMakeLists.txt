# Phase 0 uses a tiny self-contained test framework (test_framework.h) so the suite
# builds with no network access. Catch2 via FetchContent comes with the CI pass.
add_executable(tonestack_tests
    test_main.cpp
    parameter_tests.cpp
    chain_tests.cpp
    biquad_tests.cpp
    iir_tests.cpp
    oversample_waveshaper_tests.cpp
    rt_safety_tests.cpp)

target_link_libraries(tonestack_tests PRIVATE tonestack::engine tonestack::nodes)
tonestack_set_warnings(tonestack_tests)

add_test(NAME tonestack_tests COMMAND tonestack_tests)

# The golden-coefficient tests consume the circuitc-generated headers; build them only when
# the compiler venv produced them (see cmake/Codegen.cmake).
if(TONESTACK_CODEGEN_AVAILABLE)
    add_executable(tonestack_golden_coeffs golden_coeffs.cpp rangemaster_golden.cpp test_main.cpp)
    add_dependencies(tonestack_golden_coeffs tonestack_codegen)
    target_include_directories(tonestack_golden_coeffs PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/nodes/generated)
    target_link_libraries(tonestack_golden_coeffs PRIVATE tonestack::engine tonestack::nodes)
    tonestack_set_warnings(tonestack_golden_coeffs)
    add_test(NAME tonestack_golden_coeffs COMMAND tonestack_golden_coeffs)
endif()
