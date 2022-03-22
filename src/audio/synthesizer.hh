#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <deque>
#include <vector>

class Synthesizer
{
  struct sound
  {
    std::deque<unsigned long> presses;
    std::deque<unsigned long> releases;
    std::deque<unsigned long> velocities;
    std::deque<unsigned long> volumes;
  };

  MidiProcessor midi_processor {};
  NoteRepository note_repo {};
  std::vector<sound> sounds {};

public:
  Synthesizer();

  void process_new_data( FileDescriptor& fd );

  wav_frame_t calculate_curr_sample() const;

  void advance_sample();
};
