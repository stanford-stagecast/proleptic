#include "match-finder.hh"
#include <algorithm>
#include <array>
#include <iostream>

using namespace std;
using namespace std::ranges;

static constexpr uint8_t KEYDOWN_TYPE = 0x90;
static constexpr uint8_t KEYUP_TYPE = 0x80;

PianoKeyID PianoKeyID::from_raw_MIDI_code( unsigned short midi_key_id )
{
  if ( midi_key_id < PIANO_OFFSET ) {
    throw runtime_error( "invalid MIDI key id (too small!)" );
  }

  PianoKeyID ret { 0 };
  ret.key_id_ = midi_key_id - PIANO_OFFSET;

  if ( ret.key_id_ >= NUM_KEYS ) {
    throw runtime_error( "invalid MIDI key id (too big!)" );
  }

  return ret;
}

uint8_t PianoKeyID::to_raw_MIDI_code() const
{
  if ( key_id_ >= NUM_KEYS ) {
    throw runtime_error( "invalid key ID (too big)" );
  }

  return key_id_ + PIANO_OFFSET;
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

void MatchFinder::summary( ostream& out ) const
{
  out << "MatchFinder statistics summary\n";
  out << "------------------------------\n\n";

  /* example statistic: total number of KeyDown events (following another KeyDown event) recorded */
  unsigned int total_count = 0;
  for ( const auto& first_key_array : sequence_counts_ ) {
    for ( const auto& second_key_count : first_key_array ) {
      total_count += second_key_count;
    }
  }

  out << "Total number of events recorded in the MatchFinder: " << total_count << "\n";
}

void MatchFinder::print_data_structure( ostream& out ) const
{
  for ( int i = 0; i < NUM_KEYS; i++ ) {
    for ( int j = 0; j < NUM_KEYS; j++ ) {
      out << sequence_counts_[i][j] << " ";
    }
    out << std::endl;
  }
}

optional<MidiEvent> MatchFinder::predict_next_event() const
{
  if ( not previous_keydown_.has_value() ) {
    return {};
  }

  auto& lookup_array = sequence_counts_.at( previous_keydown_.value() );

  unsigned int max_count = 0;

#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  optional<PianoKeyID> max_element;

  for ( PianoKeyID key_id = 0; key_id < lookup_array.size(); key_id = key_id + 1 ) {
    unsigned int count = lookup_array.at( key_id );
    if ( count > max_count ) {
      max_count = count;
      max_element.emplace( key_id );
    } else if ( count == max_count ) {
      max_element.reset();
    }
  }

  if ( max_element.has_value() ) {
    return MidiEvent { KEYDOWN_TYPE, max_element.value().to_raw_MIDI_code(), 70 };
  }

  return {};
}
