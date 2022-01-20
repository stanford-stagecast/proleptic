add_executable ("example1" "example1.cc")
add_executable ("example2" "example2.cc")

add_executable ("example3" "example3.cc")
target_link_libraries ("example3" dbus)
target_link_libraries ("example3" audio)
target_link_libraries ("example3" util)

target_link_libraries ("example3" ${ALSA_LDFLAGS})
target_link_libraries ("example3" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("example3" ${DBus_LDFLAGS})
target_link_libraries ("example3" ${DBus_LDFLAGS_OTHER})
