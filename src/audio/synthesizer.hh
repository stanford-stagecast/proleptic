#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
//#include <deque>
#include <vector>

class Synthesizer
{
  struct sound
  {
    unsigned long offset;
    unsigned long velocity;
    float vol_ratio;
    bool released = false;
  };

  struct key
  {
    std::vector<sound> presses;
    std::vector<sound> releases;
  };

  NoteRepository note_repo {};
  std::vector<key> keys {};
  bool sustain_down = false;
  size_t frames_processed = 0;

public:
  Synthesizer();

  void process_new_data( uint8_t event_type, uint8_t event_note, uint8_t event_velocity );

  wav_frame_t calculate_curr_sample() const;

  void advance_sample();
};
