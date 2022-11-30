file (GLOB LIB_SOURCES "*.cc")
add_library (dbus STATIC ${LIB_SOURCES})
target_include_directories (dbus PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries (dbus PUBLIC PkgConfig::DBus PRIVATE util )
