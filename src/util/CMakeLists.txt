file (GLOB LIB_SOURCES "*.cc")
add_library (util STATIC ${LIB_SOURCES})
target_include_directories (util PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
