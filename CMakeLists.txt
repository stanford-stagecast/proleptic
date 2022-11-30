cmake_minimum_required ( VERSION 3.22.1 )
cmake_policy ( SET CMP0048 NEW )
project ( tasseoleptic )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig REQUIRED )

find_package ( OpenMP )

pkg_check_modules ( Eigen REQUIRED IMPORTED_TARGET GLOBAL eigen3 )
if(OpenMP_CXX_FOUND)
  target_link_libraries(PkgConfig::Eigen INTERFACE OpenMP::OpenMP_CXX)
endif()
target_compile_options(PkgConfig::Eigen INTERFACE -fno-aggressive-loop-optimizations)

pkg_check_modules ( ALSA REQUIRED IMPORTED_TARGET GLOBAL alsa )
pkg_check_modules ( DBus REQUIRED IMPORTED_TARGET GLOBAL dbus-1 )
pkg_check_modules ( Sndfile REQUIRED IMPORTED_TARGET GLOBAL sndfile )
pkg_check_modules ( Samplerate REQUIRED IMPORTED_TARGET GLOBAL samplerate )

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/graph")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/nn")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/ui")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/audio")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/stats")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/dbus")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/visualizer")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/tests")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/frontend")
