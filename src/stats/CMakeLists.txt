file (GLOB LIB_SOURCES "*.cc")
add_library (stats STATIC ${LIB_SOURCES})
target_include_directories (stats PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (stats PRIVATE util)
