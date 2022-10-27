file (GLOB LIB_SOURCES "*.cc")
add_library(osc STATIC ${LIB_SOURCES})

add_executable("testing" "testing.cc")
