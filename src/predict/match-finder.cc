#include "match-finder.hh"
#include <array>
#include <iostream>

using namespace std;

static constexpr uint8_t KEYDOWN_TYPE = 0x90;
static constexpr uint8_t KEYUP_TYPE = 0x80;

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

unsigned int MatchFinder::find_next_note( unsigned int note )
{
  std::array<unsigned int, NUM_KEYS> lookup_array
    = sequence_counts_[note]; // find the column to look for (column contains all the notes that follows note x)

  unsigned int most_common_note_frequency = 0;
  unsigned int most_common_note = -1;

  for ( unsigned int x = 0; x < lookup_array.size(); x++ ) {
    if ( lookup_array[x] > most_common_note_frequency ) {
      most_common_note_frequency = lookup_array[x];
      most_common_note = x;
    }
  }

  return most_common_note;
}

std::array<char, 3> MatchFinder::next_note( unsigned int note )
{
  // create a note out of the prediction of the previous note

  if ( (find_next_note( note ) + 21) > 0 ) {
    return { char( KEYDOWN_TYPE ),
             char( find_next_note( note ) + 21 ),
             char( 70 ) }; // TODO: Fix so that if statement is true, return nothing instead of note with velocity 0.
  }

  else {
    return { char( KEYUP_TYPE ), char( find_next_note( note ) + 21 ), char( 70 ) };
  }
}
