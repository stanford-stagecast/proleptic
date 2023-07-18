#include <iostream>

#include "match-finder.hh"

using namespace std;

static constexpr uint8_t KEYDOWN_TYPE = 0x90;

PianoKeyID PianoKeyID::from_raw_MIDI_code( unsigned short midi_key_id )
{
  static constexpr uint8_t PIANO_OFFSET = 21;

  if ( midi_key_id < PIANO_OFFSET ) {
    throw runtime_error( "invalid MIDI key id (too small!)" );
  }

  PianoKeyID ret;
  ret.key_id_ = midi_key_id - PIANO_OFFSET;

  if ( ret.key_id_ >= NUM_KEYS ) {
    throw runtime_error( "invalid MIDI key id (too big!)" );
  }

  return ret;
}

void MatchFinder::process_events( const vector<MidiEvent>& events )
{
  GlobalScopeTimer<Timer::Category::ProcessPianoEvent> timer;

  for ( const auto& ev : events ) {
    process_event( ev );
  }
}

void MatchFinder::process_event( const MidiEvent& ev )
{
  if ( ev.type != KEYDOWN_TYPE ) { // only keydowns represent new notes
    return;
  }

  const auto this_keydown = PianoKeyID::from_raw_MIDI_code( ev.note );

  if ( previous_keydown_.has_value() ) {
    sequence_counts_.at( previous_keydown_.value() ).at( this_keydown )++;
  }

  previous_keydown_ = this_keydown;
}
