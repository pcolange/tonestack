# Run in -P script mode by the engine_sdk_isolation test. Scans SCAN_DIRS for forbidden
# plugin/GUI SDK includes and fails if any are found.

set(_forbidden
    "include[ \t]*[<\"]+juce"
    "include[ \t]*[<\"]+JuceHeader"
    "include[ \t]*[<\"]+pluginterfaces"   # VST3 SDK
    "include[ \t]*[<\"]+public.sdk"        # VST3 SDK
    "include[ \t]*[<\"]+clap/"
    "include[ \t]*[<\"]+AudioUnit"
    "include[ \t]*[<\"]+AudioToolbox")

set(_violations "")

foreach(dir IN LISTS SCAN_DIRS)
    file(GLOB_RECURSE _files
         "${dir}/*.h" "${dir}/*.hpp" "${dir}/*.cpp" "${dir}/*.cc")
    foreach(file IN LISTS _files)
        file(READ "${file}" _contents)
        foreach(pat IN LISTS _forbidden)
            string(REGEX MATCH "${pat}" _hit "${_contents}")
            if(_hit)
                list(APPEND _violations "${file}: matched '${pat}'")
            endif()
        endforeach()
    endforeach()
endforeach()

if(_violations)
    message(FATAL_ERROR
        "Plugin/GUI SDK headers found in the open core:\n  ${_violations}")
endif()

message(STATUS "engine_sdk_isolation: clean")
