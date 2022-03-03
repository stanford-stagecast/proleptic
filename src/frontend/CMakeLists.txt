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


add_executable ("midi-input" "midi-input.cc")
target_link_libraries ("midi-input" dbus)
target_link_libraries ("midi-input" stats)
target_link_libraries ("midi-input" audio)
target_link_libraries ("midi-input" util)

target_link_libraries ("midi-input" ${ALSA_LDFLAGS})
target_link_libraries ("midi-input" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("midi-input" ${DBus_LDFLAGS})
target_link_libraries ("midi-input" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("midi-input" ${Sndfile_LDFLAGS})
target_link_libraries ("midi-input" ${Sndfile_LDFLAGS_OTHER})


add_executable ("synthesizer-test" "synthesizer-test.cc")
target_link_libraries ("synthesizer-test" dbus)
target_link_libraries ("synthesizer-test" stats)
target_link_libraries ("synthesizer-test" audio)
target_link_libraries ("synthesizer-test" util)

target_link_libraries ("synthesizer-test" ${ALSA_LDFLAGS})
target_link_libraries ("synthesizer-test" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("synthesizer-test" ${DBus_LDFLAGS})
target_link_libraries ("synthesizer-test" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("synthesizer-test" ${Sndfile_LDFLAGS})
target_link_libraries ("synthesizer-test" ${Sndfile_LDFLAGS_OTHER})
