#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include <deque>
#include <vector>

class Synthesizer
{

  struct key
  {
    std::deque<std::pair<float, float>> future {};
    bool released = false;
  };

  NoteRepository note_repo;
  std::vector<key> keys {};
  std::deque<std::pair<float, float>> total_future {};
  bool sustain_down = false;
  size_t frames_processed = 0;

public:
  Synthesizer( const std::string& sample_directory );

  void process_new_data( uint8_t event_type, uint8_t event_note, uint8_t event_velocity );

  void add_key_press( uint8_t adj_event_note, uint8_t event_vel );

  void add_key_release( uint8_t adj_event_note, uint8_t event_vel );

  wav_frame_t get_curr_sample() const;

  void advance_sample();
};
