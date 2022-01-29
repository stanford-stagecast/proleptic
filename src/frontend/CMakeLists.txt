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
target_link_libraries ("play-sine-wave" ${Sndfile_LDFLAGS})
target_link_libraries ("play-sine-wave" ${Sndfile_LDFLAGS_OTHER})


add_executable ("play-wav-file" "play-wav-file.cc")
target_link_libraries ("play-wav-file" dbus)
target_link_libraries ("play-wav-file" stats)
target_link_libraries ("play-wav-file" audio)
target_link_libraries ("play-wav-file" util)

target_link_libraries ("play-wav-file" ${ALSA_LDFLAGS})
target_link_libraries ("play-wav-file" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("play-wav-file" ${DBus_LDFLAGS})
target_link_libraries ("play-wav-file" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("play-wav-file" ${Sndfile_LDFLAGS})
target_link_libraries ("play-wav-file" ${Sndfile_LDFLAGS_OTHER})
