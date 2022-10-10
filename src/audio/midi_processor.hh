#pragma once

#include <chrono>
#include <optional>
#include <queue>
#include <string>

#include "file_descriptor.hh"
#include "ring_buffer.hh"

/* wrap MIDI file input */
class MidiProcessor
{
  RingBuffer unprocessed_midi_bytes_ { 4096 };
  const static size_t batch_size = 1;
  const static size_t input_size = 16;

  std::chrono::steady_clock::time_point last_event_time_ { std::chrono::steady_clock::now() };

public:
  std::queue<float> nn_midi_input( const std::string& midi_filename );

  void read_from_fd( FileDescriptor& fd );

  float pop_event();

  void pop_active_sense_bytes();

  bool want_read() const { return unprocessed_midi_bytes_.writable_region().size() > 0; }

  bool has_event() { return unprocessed_midi_bytes_.readable_region().size() >= 3; }

  uint8_t get_event_type() const { return unprocessed_midi_bytes_.readable_region().at( 0 ); }
  uint8_t get_event_note() const { return unprocessed_midi_bytes_.readable_region().at( 1 ); }
  uint8_t get_event_velocity() const { return unprocessed_midi_bytes_.readable_region().at( 2 ); }

  // unsigned int pop_event();

  void reset_time() { last_event_time_ = std::chrono::steady_clock::now(); };
  std::chrono::steady_clock::time_point get_original_time() { return last_event_time_; };

  /* no event or active sense in more than 1 s */
  bool data_timeout() const;
};
