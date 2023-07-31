#include "match-finder.hh"
#include <array>
#include <iostream>

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

PianoKeyID PianoKeyID::from_key( unsigned short array_key )
{
  PianoKeyID ret;
  ret.key_id_ = array_key;
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

void MatchFinder::process_predict_events( const vector<MidiEvent>& events )
{
  for ( const auto& ev : events ) {
    process_predict_event( ev );
  }
}

void MatchFinder::process_predict_event( const MidiEvent& ev )
{
  if ( ev.type != KEYDOWN_TYPE ) { // only keydowns represent new notes
    return;
  }

  const auto this_keydown = PianoKeyID::from_raw_MIDI_code( ev.note );

  if ( previous_keydown_.has_value() ) {
    sequence_counts_.at( previous_keydown_.value() ).at( this_keydown )++;
    predict_event( ev );
  }
  previous_keydown_ = this_keydown;
}

void MatchFinder::predict_event( const MidiEvent& ev )
{
  const auto this_keydown = PianoKeyID::from_raw_MIDI_code( ev.note ); // current keydown

  if ( made_prediction ) { // if a prediction has been made, compare it to current keydown
    total_predictions++;
    if ( previous_prediction_ == this_keydown ) {
      correct_predictions++;
    }
    made_prediction = false;
    // cout << "Predicted " << previous_prediction_ << "; Actually " << this_keydown << "; Total correct " <<
    // correct_predictions << "\n";
  }

  const std::array<unsigned int, NUM_KEYS>& this_data = sequence_counts_[this_keydown];

  bool tie = false;     // if there is a tie, don't make a prediction
  unsigned int max = 0; // counter to find most frequent keydown following this_keydown
  PianoKeyID index;     // will eventually store PianoKeyID of prediction (if one is made)

  for ( int i = 0; i < NUM_KEYS; i++ ) {
    if ( this_data[i] > max ) {
      max = this_data[i];
      index = PianoKeyID::from_key( (unsigned short)i );
      tie = false;
    } else if ( this_data[i] == max ) {
      tie = true;
    }
  }

  if ( tie == false ) { // if no tie, make a prediction
    made_prediction = true;
    previous_prediction_ = index;
  }
};

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
  out << "Total predictions made: " << total_predictions << "\n";
  out << "Total correct predictions: " << correct_predictions << "\n";
  double accuracy = (double)correct_predictions / (double)total_predictions;
  out << "Prediction accuracy: " << accuracy << "\n";
  out << "\n";
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

void MatchFinder::find_next_note( unsigned int note, std::ostream& out )
{
  std::array<unsigned int, NUM_KEYS> lookup_array
    = sequence_counts_[note]; // find the column to look for (column contains all the notes that follows note x)

  unsigned int most_common_note_frequency = 0;
  unsigned int most_common_note = 0;

  for ( unsigned int x = 0; x < lookup_array.size(); x++ ) {
    if ( lookup_array[x] > most_common_note_frequency ) {
      most_common_note_frequency = lookup_array[x];
      most_common_note = x;
    }
  }

  out << "Most common note: " << most_common_note << endl;
}
