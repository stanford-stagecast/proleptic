macro (add_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" dbus stats audio predict)
endmacro (add_exec)

macro (add_exec_with_samplerate exec_name)
  add_exec("${exec_name}")
  target_link_libraries ("${exec_name}" PkgConfig::Samplerate)
endmacro (add_exec_with_samplerate)

macro (add_exec_with_simplenn exec_name)
  add_exec("${exec_name}")
  target_link_libraries ("${exec_name}" simplenn)
endmacro (add_exec_with_simplenn)

add_executable ("example1" "example1.cc")
add_executable ("example2" "example2.cc")

add_exec(play-sine-wave)
add_exec(midi-input)
add_exec(midi-recorder)
add_exec(midi-to-piano)
add_exec_with_samplerate(synthesizer-test)
add_exec(curis-demo)
add_exec_with_simplenn(split-ear-demo)
add_exec_with_simplenn(split-ear-octave)
add_exec_with_simplenn(metronome)
add_exec_with_samplerate(synth-benchmark)
add_exec(match)
add_exec(match_v8)
add_exec(match_v9)
add_exec_with_simplenn(metronome-clock-recovery)

add_exec(matchfind-from-file)
add_exec(matchfind-from-piano)
