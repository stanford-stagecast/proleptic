#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <vector>

class Synthesizer
{
  struct sound
  {
    uint8_t direction;
    uint8_t note;
    uint8_t velocity;
    unsigned long curr_offset;
  };

  MidiProcessor midi_processor {};
  NoteRepository note_repo {};
  std::vector<sound> active_sounds {};

public:
  void process_new_data( FileDescriptor& fd );

  wav_frame_t calculate_curr_sample() const;

  void advance_sample();
};
