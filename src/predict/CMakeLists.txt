file (GLOB LIB_SOURCES "*.cc")
add_library (predict STATIC ${LIB_SOURCES})
target_include_directories (predict PUBLIC "${CMAKE_CURRENT_LIST_DIR}" )
target_link_libraries (predict PUBLIC util)
