#pragma once

#include <array>
#include <optional>
#include <vector>

#include "timer.hh"

static constexpr uint8_t NUM_KEYS = 88;

struct MidiEvent
{
  uint8_t type, note, velocity; // actual midi data

  std::string_view to_string_view() const { return { reinterpret_cast<const char*>( this ), sizeof( MidiEvent ) }; }
};

class PianoKeyID
{
  uint8_t key_id_ {};

  static constexpr uint8_t PIANO_OFFSET = 21;

public:
  static PianoKeyID from_raw_MIDI_code( unsigned short midi_key_id );
  uint8_t to_raw_MIDI_code() const;
  operator uint8_t() const { return key_id_; }
  PianoKeyID( uint8_t id )
    : key_id_( id )
  {}
};

class MatchFinder
{
public:
  struct Stats
  {
    unsigned int total_keydown_events;
    unsigned int total_predictions_made;
    unsigned int predictions_that_were_correct;
    unsigned int predictions_that_were_incorrect;
    unsigned int predictions_that_were_ignored;
  };

private:
  /* Given KeyDown of key x, how many times was the next KeyDown of key y?
     We store this count in sequence_counts_[x][y] */
  std::array<std::array<unsigned int, NUM_KEYS>, NUM_KEYS> sequence_counts_ {};

  std::optional<PianoKeyID> previous_keydown_ {}; /* previous key pressed */

  Stats stats_ {};

  std::optional<PianoKeyID> pending_prediction_ {};

public:
  void process_event( const MidiEvent& ev );
  void summary( std::ostream& out ) const;
  void print_data_structure( std::ostream& out ) const;

  std::optional<MidiEvent> predict_next_event(); /* non-const because updates stats */
};
