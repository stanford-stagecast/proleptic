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

target_link_libraries ("synthesizer-test" ${Samplerate_LDFLAGS})
target_link_libraries ("synthesizer-test" ${Samplerate_LDFLAGS_OTHER})


add_executable ("curis-demo" "curis-demo.cc")
target_link_libraries ("curis-demo" dbus)
target_link_libraries ("curis-demo" stats)
target_link_libraries ("curis-demo" audio)
target_link_libraries ("curis-demo" util)

target_link_libraries ("curis-demo" ${ALSA_LDFLAGS})
target_link_libraries ("curis-demo" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("curis-demo" ${DBus_LDFLAGS})
target_link_libraries ("curis-demo" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("curis-demo" ${Sndfile_LDFLAGS})
target_link_libraries ("curis-demo" ${Sndfile_LDFLAGS_OTHER})

add_executable ("split-ear-demo" "split-ear-demo.cc")
target_link_libraries ("split-ear-demo" dbus)
target_link_libraries ("split-ear-demo" stats)
target_link_libraries ("split-ear-demo" audio)
target_link_libraries ("split-ear-demo" simplenn)
target_link_libraries ("split-ear-demo" graph)
target_link_libraries ("split-ear-demo" util)

target_link_libraries ("split-ear-demo" ${ALSA_LDFLAGS})
target_link_libraries ("split-ear-demo" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("split-ear-demo" ${DBus_LDFLAGS})
target_link_libraries ("split-ear-demo" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("split-ear-demo" ${Sndfile_LDFLAGS})
target_link_libraries ("split-ear-demo" ${Sndfile_LDFLAGS_OTHER})

add_executable ("split-ear-piano-roll" "split-ear-piano-roll.cc")
target_link_libraries ("split-ear-piano-roll" dbus)
target_link_libraries ("split-ear-piano-roll" stats)
target_link_libraries ("split-ear-piano-roll" audio)
target_link_libraries ("split-ear-piano-roll" simplenn)
target_link_libraries ("split-ear-piano-roll" graph)
target_link_libraries ("split-ear-piano-roll" util)

target_link_libraries ("split-ear-piano-roll" ${ALSA_LDFLAGS})
target_link_libraries ("split-ear-piano-roll" ${ALSA_LDFLAGS_OTHER})

target_link_libraries ("split-ear-piano-roll" ${DBus_LDFLAGS})
target_link_libraries ("split-ear-piano-roll" ${DBus_LDFLAGS_OTHER})
target_link_libraries ("split-ear-piano-roll" ${Sndfile_LDFLAGS})
target_link_libraries ("split-ear-piano-roll" ${Sndfile_LDFLAGS_OTHER})
