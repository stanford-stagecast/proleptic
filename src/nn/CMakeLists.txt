file (GLOB LIB_SOURCES "*.cc")
add_library (nn STATIC ${LIB_SOURCES})
target_include_directories (nn PUBLIC "${CMAKE_CURRENT_LIST_DIR}" )
target_link_libraries (nn PUBLIC PkgConfig::Eigen PRIVATE graph util)
