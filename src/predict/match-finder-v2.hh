#pragma once

#include <array>
#include <vector>

#include "timer.hh"

static constexpr uint8_t NUM_KEYS = 88;

struct MidiEvent
{
  unsigned short type, note, velocity; // actual midi data
};

class PianoKeyID
{
  uint8_t key_id_;

public:
  static PianoKeyID from_raw_MIDI_code( unsigned short midi_key_id );
  static PianoKeyID from_key( unsigned short array_key );
  operator uint8_t() const { return key_id_; }
};

class MatchFinder
{
  /* Given KeyDown of key x, how many times was the next KeyDown of key y?
     We store this count in sequence_counts_[x][y] */
  std::array<std::array<unsigned int, NUM_KEYS>, NUM_KEYS> sequence_counts_ {};

  std::optional<PianoKeyID> previous_keydown_ {}; /* previous key pressed */

  void process_event_v2( const MidiEvent& ev );
  void predict_event( const MidiEvent& ev );

public:
  void process_events_v2( const std::vector<MidiEvent>& events );
  void summary_v2( std::ostream& out ) const;

  PianoKeyID previous_prediction_ {};
  bool made_prediction = false;
  float total_predictions = 0;
  float correct_predictions = 0;
};
