# Optional offline codegen: run the Python `circuitc` compiler to (re)generate the engine
# coefficient headers from the circuit definitions. Gated on the compiler venv existing so a
# C++-only build/CI job (no Python) still configures — it just skips the generated-header test.
#
# Only the headers are generated here. The committed golden IR in contract/golden is a
# regression anchor and is rewritten only by a deliberate `circuitc compile --update-golden`,
# never as a build side effect.
#
# Set up the venv with:  python3 -m venv compiler/.venv &&
#   compiler/.venv/bin/pip install -e compiler[dev]

set(_circuitc_py "${CMAKE_SOURCE_DIR}/compiler/.venv/bin/python")
set(_generated_dir "${CMAKE_SOURCE_DIR}/nodes/generated")
set(_generated_headers
    "${_generated_dir}/ts9_tables.h"
    "${_generated_dir}/rangemaster_tables.h")

if(EXISTS "${_circuitc_py}")
    file(GLOB_RECURSE _circuitc_srcs "${CMAKE_SOURCE_DIR}/compiler/src/circuitc/*.py")
    add_custom_command(
        OUTPUT ${_generated_headers}
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${CMAKE_SOURCE_DIR}/compiler/src"
                "${_circuitc_py}" -m circuitc.cli compile
                --out-dir "${_generated_dir}"
        DEPENDS ${_circuitc_srcs}
        COMMENT "circuitc: generating coefficient headers"
        VERBATIM)
    add_custom_target(tonestack_codegen DEPENDS ${_generated_headers})
    set(TONESTACK_CODEGEN_AVAILABLE ON CACHE INTERNAL "circuitc venv present")
    message(STATUS "codegen: circuitc venv found, generated headers wired")
else()
    set(TONESTACK_CODEGEN_AVAILABLE OFF CACHE INTERNAL "circuitc venv present")
    message(STATUS "codegen: no circuitc venv (${_circuitc_py}); golden_coeffs test skipped")
endif()
