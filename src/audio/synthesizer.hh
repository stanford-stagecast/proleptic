#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"

#include <deque>
#include <vector>

static constexpr unsigned int FUTURE_LENGTH
  = 2425 * 4096; // just longer than longest piano sample (25.862625 seconds)

class Synthesizer
{

  struct key
  {
    std::vector<std::pair<float, float>> future { FUTURE_LENGTH };
    bool released = false;
  };

  NoteRepository note_repo;
  std::vector<key> keys {};
  std::vector<std::pair<float, float>> total_future { FUTURE_LENGTH };
  bool sustain_down = false;
  size_t frames_processed = 0;

  size_t get_buff_idx( size_t offset ) const { return ( frames_processed + offset ) % FUTURE_LENGTH; };

public:
  Synthesizer( const std::string& sample_directory );

  void process_new_data( uint8_t event_type, uint8_t event_note, uint8_t event_velocity );

  void add_key_press( uint8_t adj_event_note, uint8_t event_vel );

  void add_key_release( uint8_t adj_event_note, uint8_t event_vel );

  wav_frame_t get_curr_sample() const;

  void advance_sample();
};