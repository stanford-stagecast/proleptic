[![Compile](https://github.com/stanford-stagecast/proleptic/actions/workflows/compile.yml/badge.svg)](https://github.com/stanford-stagecast/proleptic/actions/workflows/compile.yml)

# Overview
* `src/`: standalone binaries and demos
* `src/graph/`: svg generation for the web interface
* `src/libsimplenn/`: the public interface of simplenn
* `src/nn/`: the core neural network code, including inference and training
* `src/tests/`: unit tests
* `src/util/`: helpful classes/structures used elsewhere
* `src/audio/`: classes used for audio synthesis and talking to ALSA devices
* `src/dbus/`: code to claim exclusive use of an audio device
* `src/stats/`: a task that periodically prints runtime info and timing
* `src/frontend/`: user-invoked programs (originally part of the synthesizer)

# Setup
1. Clone the repo and enter it: `git clone
git@github.com:stanford-stagecast/proleptic.git && cd proleptic`
2. Initialize CMake: `cmake -S . -B build`

# Common Tasks
* Building: `cmake --build build`
* Building in parallel: `cmake --build build --parallel`
* Formatting: `cmake --build build --target format`
  * or use `git clang-format` when you have staged changes
* Testing: `cmake --build build --target check`
  * or run `ctest --test-dir build`
* Running the audio synthesizer: `./build/src/frontend/synthesizer-test [device_prefix] [midi_device] [sample_directory]`
  * On the snr-piano machine:
    * device_prefix: Scarlett
    * midi_device: /dev/snd/midi*
    * sample_directory: /usr/local/share/slender/samples/

If you want to use a compiler other than the default, delete your `build`
directory, then redo the initialization step with `CC` and `CXX` set
appropriately:
- `CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake -B build`

CMake has the following configs set up:
* `Debug`
* `Release`
* `DebugASan`

# Repository Checks

Current checks:
* tests pass in `DebugASan` and `Release` modes
* code is correctly formatted, according to `clang-format`
* linear history
* only branches with all checks passing can be pushed to main
