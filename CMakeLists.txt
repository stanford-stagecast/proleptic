cmake_minimum_required ( VERSION 3.22.1 )
cmake_policy ( SET CMP0048 NEW )
project ( tasseoleptic )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig REQUIRED )

find_package ( OpenMP )
if(OpenMP_CXX_FOUND)
    link_libraries(OpenMP::OpenMP_CXX)
endif()

pkg_check_modules ( Eigen REQUIRED eigen3 )
include_directories ( SYSTEM ${Eigen_INCLUDE_DIRS} )
add_compile_options ( ${Eigen_CFLAGS} )

pkg_check_modules ( ALSA REQUIRED alsa )
include_directories ( ${ALSA_INCLUDE_DIRS} )
add_compile_options ( ${ALSA_CFLAGS} )

pkg_check_modules ( DBus REQUIRED dbus-1 )
include_directories ( ${DBus_INCLUDE_DIRS} )
add_compile_options ( ${DBus_CFLAGS} )

pkg_check_modules ( Sndfile REQUIRED sndfile )
include_directories ( ${Sndfile_INCLUDE_DIRS} )
add_compile_options ( ${Sndfile_CFLAGS} )

pkg_check_modules ( Samplerate REQUIRED samplerate )
include_directories ( ${Samplerate_INCLUDE_DIRS} )
add_compile_options ( ${Samplerate_CFLAGS} )

include_directories ("${PROJECT_SOURCE_DIR}/src/nn")
include_directories ("${PROJECT_SOURCE_DIR}/src/graph")
include_directories ("${PROJECT_SOURCE_DIR}/src/util")
include_directories ("${PROJECT_SOURCE_DIR}/src/ui")
include_directories ("${PROJECT_SOURCE_DIR}/src/audio")
include_directories ("${PROJECT_SOURCE_DIR}/src/stats")
include_directories ("${PROJECT_SOURCE_DIR}/src/dbus")
include_directories ("${PROJECT_SOURCE_DIR}/src/visualizer")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/nn")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/graph")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/util")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/ui")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/audio")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/stats")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/dbus")

add_subdirectory ("${PROJECT_SOURCE_DIR}/src/tests")
add_subdirectory ("${PROJECT_SOURCE_DIR}/src/frontend")
