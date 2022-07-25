cmake_minimum_required ( VERSION 3.22.1 )
cmake_policy ( SET CMP0048 NEW )
project ( forgrad )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig )

pkg_check_modules ( Eigen REQUIRED eigen3 )
include_directories ( SYSTEM ${Eigen_INCLUDE_DIRS} )
add_compile_options ( ${Eigen_CFLAGS} )

include_directories ("${PROJECT_SOURCE_DIR}/src/nn")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/nn")
