#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <deque>
#include <vector>

class Synthesizer
{
  struct sound
  {
    unsigned long offset;
    unsigned long velocity;
    float vol_ratio;
  };

  struct key
  {
    std::deque<sound> presses;
    std::deque<sound> releases;
  };

  MidiProcessor midi_processor {};
  NoteRepository note_repo {};
  std::vector<key> keys {};

public:
  Synthesizer();

  void process_new_data( FileDescriptor& fd );

  wav_frame_t calculate_curr_sample() const;

  void advance_sample();
};
