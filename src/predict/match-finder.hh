#pragma once

#include <array>
#include <vector>

#include "timing-stats.hh"

static constexpr uint8_t PIANO_OFFSET = 21;
static constexpr uint8_t NUM_KEYS = 88;

struct MidiEvent
{
  unsigned short type, note, velocity; // actual midi data
};

class MatchFinder
{
  std::array<std::vector<unsigned short>, NUM_KEYS> storage_;
  unsigned short prev_note;
  bool first_note = true;

  TimingStats timing_stats_;

public:
  void process_events( const std::vector<MidiEvent>& events );
  void print_stats() const;
};
