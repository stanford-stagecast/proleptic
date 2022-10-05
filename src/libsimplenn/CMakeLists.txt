file (GLOB LIB_SOURCES "*.cc")
add_library (simplenn ${LIB_SOURCES})
target_link_libraries (simplenn nn util ${Eigen_LDFLAGS} ${Eigen_LDFLAGS_OTHER})
target_include_directories (simplenn INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
