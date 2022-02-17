#pragma once

#include <chrono>
#include <optional>

#include "file_descriptor.hh"
#include "ring_buffer.hh"

/* wrap MIDI file input */
class MidiProcessor
{
  RingBuffer unprocessed_midi_bytes_ { 4096 };

  std::chrono::steady_clock::time_point last_event_time_ { std::chrono::steady_clock::now() };

  void pop_active_sense_bytes();

public:
  void read_from_fd( FileDescriptor& fd );

  bool want_read() const { return unprocessed_midi_bytes_.writable_region().size() > 0; }

  bool has_event() { return unprocessed_midi_bytes_.readable_region().size() >= 3; }

  uint8_t get_event_type() const { return unprocessed_midi_bytes_.readable_region().at( 0 ); }
  uint8_t get_event_note() const { return unprocessed_midi_bytes_.readable_region().at( 1 ); }
  uint8_t get_event_velocity() const { return unprocessed_midi_bytes_.readable_region().at( 2 ); }

  void pop_event();

  /* no event or active sense in more than 1 s */
  bool data_timeout() const;
};
