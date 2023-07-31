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

  void process_event( const MidiEvent& ev );

  void process_predict_event( const MidiEvent& ev ); // from-file
  void predict_event( const MidiEvent& ev );         // from-file

  PianoKeyID previous_prediction_ {}; // from-file
  bool made_prediction = false;       // from-file

public:
  void process_events( const std::vector<MidiEvent>& events );
  void process_predict_events( const std::vector<MidiEvent>& events ); // from-file
  void summary( std::ostream& out ) const;                             // from-file
  void print_data_structure( std::ostream& out ) const;
  void find_next_note( unsigned int note, std::ostream& out );
  std::array<char, 3> next_note( unsigned int note );
  void predict_chunk( const std::vector<MidiEvent>& events );

  int total_predictions = 0;
  int correct_predictions = 0;
};
