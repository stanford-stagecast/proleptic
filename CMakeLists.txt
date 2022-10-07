cmake_minimum_required ( VERSION 2.8.5 )
cmake_policy ( SET CMP0048 NEW )
project ( pancake )

include ( etc/build_type.cmake )
include ( etc/cflags.cmake )
include ( etc/clang_format.cmake )
include ( etc/tests.cmake )

find_package ( PkgConfig )

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

add_subdirectory ("simplenn" EXCLUDE_FROM_ALL)
add_subdirectory ("src")
