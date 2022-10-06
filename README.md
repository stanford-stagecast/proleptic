# Overview
* `src/`: standalone binaries and demos
* `src/graph/`: svg generation for the web demos
* `src/libsimplenn/`: the public interface of simplenn
* `src/nn/`: the core neural network code, including inference and training
* `src/tests/`: unit tests
* `src/util/`: helpful classes/structures used elsewhere

# Setup
1. Clone the repo and enter it: `git clone
git@github.com:stanford-stagecast/simplenn.git && cd simplenn`
2. Create a build directory: `mkdir build`
3. Initialize CMake: `cmake -B build`

# Common Tasks
* Building: `cmake --build build`
* Building in parallel: `cmake --build build --parallel`
* Formatting: `cmake --build build --target format`
  * or use `git clang-format` when you have staged changes
* Testing: `cmake --build build --target check`
  * or run `ctest --test-dir build`

If you want to use a compiler other than the default, delete your `build`
directory, then redo the initialization step with `CC` and `CXX` set
appropriately:
- `CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake -B build`

CMake has the following configs set up:
* `Debug`
* `Release`
* `DebugASan`

# Repository Checks

Status:
[![Compile](https://github.com/stanford-stagecast/simplenn/actions/workflows/compile.yml/badge.svg)](https://github.com/stanford-stagecast/simplenn/actions/workflows/compile.yml)

Current checks:
* tests pass in `DebugASan` and `Release` modes
* code is correctly formatted, according to `clang-format`
* only branches with all checks passing can be merged into main
