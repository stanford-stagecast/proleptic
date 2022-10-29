macro (add_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" dbus stats audio util)
    target_link_libraries ("${exec_name}" ${ALSA_LDFLAGS})
    target_link_libraries ("${exec_name}" ${ALSA_LDFLAGS_OTHER})

    target_link_libraries ("${exec_name}" ${DBus_LDFLAGS})
    target_link_libraries ("${exec_name}" ${DBus_LDFLAGS_OTHER})
    target_link_libraries ("${exec_name}" ${Sndfile_LDFLAGS})
    target_link_libraries ("${exec_name}" ${Sndfile_LDFLAGS_OTHER})
endmacro (add_exec)

macro (add_exec_with_samplerate exec_name)
  add_exec("${exec_name}")

  target_link_libraries ("${exec_name}" ${Samplerate_LDFLAGS})
  target_link_libraries ("${exec_name}" ${Samplerate_LDFLAGS_OTHER})
endmacro (add_exec_with_samplerate)

macro (add_exec_with_simplenn exec_name)
  add_exec("${exec_name}")

  target_link_libraries ("${exec_name}" simplenn)
endmacro (add_exec_with_simplenn)

add_executable ("example1" "example1.cc")
add_executable ("example2" "example2.cc")

add_exec(play-sine-wave)
add_exec(midi-input)
add_exec_with_samplerate(synthesizer-test)
add_exec(curis-demo)
add_exec_with_simplenn(split-ear-demo)
add_exec_with_simplenn(split-ear-piano-roll)
add_exec_with_simplenn(split-ear-octave)
add_exec_with_simplenn(metronome)