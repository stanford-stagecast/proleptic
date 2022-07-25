cmake_minimum_required ( VERSION 3.22.1 )
cmake_policy ( SET CMP0048 NEW )
project ( forgrad )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig REQUIRED )

pkg_check_modules ( Eigen REQUIRED eigen3 )
include_directories ( SYSTEM ${Eigen_INCLUDE_DIRS} )
add_compile_options ( ${Eigen_CFLAGS} )

pkg_check_modules( Cairo REQUIRED cairo )
include_directories( SYSTEM ${Cairo_INCLUDE_DIRS} )
add_compile_options ( ${Cairo_CFLAGS} )

pkg_check_modules( Pango REQUIRED pangocairo )
include_directories( SYSTEM ${Pango_INCLUDE_DIRS} )
add_compile_options ( ${Pango_CFLAGS} )

include_directories ("${PROJECT_SOURCE_DIR}/src/nn")
include_directories ("${PROJECT_SOURCE_DIR}/src/graph")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/nn")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/graph")
