#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <vector>

class Synthesizer
{
  struct sound
  {
    bool active;
    bool direction;
    uint8_t velocity;
    unsigned long curr_offset;
    float ratio;
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
