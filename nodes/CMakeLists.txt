# Header-only for now; gains compiled translation units as nodes acquire vendored deps.
add_library(tonestack_nodes INTERFACE)

target_include_directories(tonestack_nodes INTERFACE include)
target_link_libraries(tonestack_nodes INTERFACE tonestack::engine)

add_library(tonestack::nodes ALIAS tonestack_nodes)
