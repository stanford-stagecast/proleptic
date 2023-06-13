#pragma once

#include "midi_processor.hh"
#include "note_repository.hh"
#include "typed_ring_buffer.hh"

#include <deque>
#include <vector>

static constexpr unsigned int FUTURE_LENGTH
  = 2425 * 4096; // just longer than longest piano sample (25.862625 seconds)

class Synthesizer
{
  using ChannelPair = TypedRingStorage<std::pair<float, float>>;

  struct key
  {
    ChannelPair future { FUTURE_LENGTH };
    bool released = false;
  };

  NoteRepository note_repo;
  std::vector<key> keys;
  ChannelPair total_future { FUTURE_LENGTH };
  bool sustain_down = false;
  size_t frames_processed = 0;

public:
  Synthesizer( const std::string& sample_directory );

  void process_new_data( uint8_t event_type, uint8_t event_note, uint8_t event_velocity );

  void add_key_press( uint8_t adj_event_note, uint8_t event_vel );

  void add_key_release( uint8_t adj_event_note, uint8_t event_vel );

  void add_shallow_key_press( uint8_t adj_event_note, uint8_t event_vel );

  wav_frame_t get_curr_sample() const;

  void advance_sample();
};
