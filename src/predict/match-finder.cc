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
    if ( pending_prediction_.has_value() ) {
      stats_.predictions_that_were_ignored++;
    }
    pending_prediction_.reset();
    return;
  }

  const auto this_keydown = PianoKeyID::from_raw_MIDI_code( ev.note );

  if ( previous_keydown_.has_value() && second_previous_keydown_.has_value() ) {
    cout << "hi \n";
    sequence_counts_.at( second_previous_keydown_.value() ).at( previous_keydown_.value() ).at( this_keydown )++;
  }

  // update both previous keydowns

  second_previous_keydown_ = previous_keydown_;
  previous_keydown_ = this_keydown;

  /* update stats */
  stats_.total_keydown_events++;
  if ( pending_prediction_.has_value() ) {
    if ( pending_prediction_.value() == this_keydown ) {
      stats_.predictions_that_were_correct++;
    } else {
      stats_.predictions_that_were_incorrect++;
    }
  }
  pending_prediction_.reset();
}

void MatchFinder::summary( ostream& out ) const
{
  out << "MatchFinder statistics summary\n";
  out << "------------------------------\n\n";

  /* example statistic: total number of KeyDown events (following another KeyDown event) recorded */
  unsigned int total_count = 0;
  for ( const auto& first_key_array : sequence_counts_ ) {
    for ( const auto& second_key_count : first_key_array ) {
      for ( const auto& third_key_count : second_key_count ) {
        total_count += third_key_count;
      }
    }
  }

  out << "Total number of events recorded in the MatchFinder: " << total_count << "\n";
  out << "Total number of keydown events:  " << stats_.total_keydown_events << "\n";
  out << "Number of predictions made:      " << stats_.total_predictions_made << "\n";
  out << "Predictions that were ignored:   " << stats_.predictions_that_were_ignored << "\n";
  out << "\n";
  if ( stats_.predictions_that_were_ignored > stats_.total_predictions_made ) {
    throw runtime_error( "invalid statistics" );
  }
  const auto net_predictions_made = stats_.total_predictions_made - stats_.predictions_that_were_ignored;
  out << "Net predictions made:            " << stats_.total_predictions_made - stats_.predictions_that_were_ignored
      << "\n";
  out << "Predictions that were correct:   " << stats_.predictions_that_were_correct << "\n";
  out << "Predictions that were incorrect: " << stats_.predictions_that_were_incorrect << "\n";

  const auto total_correct_incorrect
    = stats_.predictions_that_were_correct + stats_.predictions_that_were_incorrect;
  if ( total_correct_incorrect > net_predictions_made ) {
    throw runtime_error( "invalid statistics II" );
  }

  out << "Net preds w/ unknown truth:      " << ( net_predictions_made - total_correct_incorrect ) << "\n";
}

void MatchFinder::print_data_structure( ostream& out ) const
{
  for ( int i = 0; i < NUM_KEYS; i++ ) {
    for ( int j = 0; j < NUM_KEYS; j++ ) {
      for ( int k = 0; k < NUM_KEYS; k++ ) {
        out << sequence_counts_[i][j][k] << " ";
      }
    }
    out << std::endl;
  }
}

optional<MidiEvent> MatchFinder::predict_next_event()
{
  if ( pending_prediction_.has_value() ) {
    throw runtime_error( "attempted to predict next event without a new event" );
  }

  if ( not previous_keydown_.has_value() || not second_previous_keydown_.has_value() ) {
    return {};
  }

  auto& lookup_array = sequence_counts_.at( second_previous_keydown_.value() ).at( previous_keydown_.value() );

  unsigned int max_count = 0;

  for ( PianoKeyID key_id = 0; key_id < lookup_array.size(); key_id = key_id + 1 ) {
    unsigned int count = lookup_array.at( key_id );
    if ( count > max_count ) {
      max_count = count;
      pending_prediction_.emplace( key_id );
    } else if ( count == max_count ) {
      pending_prediction_.reset();
    }
  }

  if ( pending_prediction_.has_value() ) {
    stats_.total_predictions_made++;
    return MidiEvent { KEYDOWN_TYPE, pending_prediction_.value().to_raw_MIDI_code(), 70 };
  }

  return {};
}
