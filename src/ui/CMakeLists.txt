file (GLOB LIB_SOURCES "*.cc")
add_library (ui STATIC ${LIB_SOURCES})
target_include_directories (ui PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (ui PRIVATE util)
