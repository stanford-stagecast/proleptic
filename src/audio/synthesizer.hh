#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <vector>

class Synthesizer
{
  MidiProcessor midi_processor {};
  NoteRepository note_repo {};
  std::vector<sound> active_sounds {};

public:
  void process_new_data( FileDescriptor& fd );

  wav_frame_t calculate_curr_sample();
};
