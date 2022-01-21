add_executable ("example1" "example1.cc")
add_executable ("example2" "example2.cc")

add_executable ("play-sine-wave" "play-sine-wave.cc")
target_link_libraries ("play-sine-wave" dbus)
target_link_libraries ("play-sine-wave" stats)
target_link_libraries ("play-sine-wave" audio)
target_link_libraries ("play-sine-wave" util)

target_link_libraries ("play-sine-wave" ${ALSA_LDFLAGS})
target_link_libraries ("play-sine-wave" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("play-sine-wave" ${DBus_LDFLAGS})
target_link_libraries ("play-sine-wave" ${DBus_LDFLAGS_OTHER})
