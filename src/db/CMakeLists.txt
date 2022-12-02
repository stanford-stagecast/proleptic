file (GLOB LIB_SOURCES "*.cc")
add_library (db INTERFACE)
target_include_directories (db INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (db INTERFACE PkgConfig::Eigen util)
