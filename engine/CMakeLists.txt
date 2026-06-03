add_library(tonestack_engine STATIC
    src/Parameter.cpp
    src/ParameterSet.cpp
    src/Chain.cpp)

target_include_directories(tonestack_engine PUBLIC include)
target_compile_features(tonestack_engine PUBLIC cxx_std_17)
tonestack_set_warnings(tonestack_engine)

add_library(tonestack::engine ALIAS tonestack_engine)
