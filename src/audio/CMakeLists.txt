file (GLOB LIB_SOURCES "*.cc")
add_library (audio STATIC ${LIB_SOURCES})
target_include_directories (audio PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (audio PUBLIC PkgConfig::ALSA PkgConfig::Sndfile util)
