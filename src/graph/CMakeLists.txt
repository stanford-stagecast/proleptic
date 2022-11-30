file (GLOB LIB_SOURCES "*.cc")
add_library (graph STATIC ${LIB_SOURCES})
target_include_directories (graph PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
