#pragma once
#include <array>
#include <vector>

struct PianoRollEvent
{
  static constexpr float NoteDown = 1.0;
  static constexpr float NoteUp = 0.0;
  static constexpr float Unknown = 0.5;
};

template<std::size_t N>
using PianoRollColumn = std::array<float, N>;

template<std::size_t N>
using PianoRoll = std::vector<PianoRollColumn<N>>;
