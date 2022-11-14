#include "synthesizer.hh"
#include <cmath>
#include <iostream>

constexpr unsigned int NUM_KEYS = 88;
constexpr unsigned int KEY_OFFSET = 21;

constexpr unsigned int KEY_DOWN = 144;
constexpr unsigned int KEY_UP = 128;
constexpr unsigned int SUSTAIN = 176;

using namespace std;

Synthesizer::Synthesizer( const string& sample_directory )
  : note_repo( sample_directory )
{
  total_future.resize( 48000 * 30 ); // Samples per second * 30 seconds (maximum length of key press sound)
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    keys.push_back( {} );
  }
}

void Synthesizer::process_new_data( uint8_t event_type, uint8_t event_note, uint8_t event_velocity )
{
  if ( event_type == SUSTAIN ) {
    if ( event_velocity == 127 )
      sustain_down = true;
    else
      sustain_down = false;
  } else if ( event_type == KEY_DOWN || event_type == KEY_UP ) {
    bool direction = event_type == KEY_DOWN ? true : false;

    if ( !direction ) {
      add_key_release( event_note - KEY_OFFSET, event_velocity );
    } else {
      add_key_press( event_note - KEY_OFFSET, event_velocity );
    }
  }
}

void Synthesizer::add_key_press( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = false;
  k.future.resize( 48000 * 30 ); // Samples per second * 30 seconds (maximum length of key press sound)

  size_t offset = 0;

  // Update key future and total future
  while ( !note_repo.note_finished( true, adj_event_note, event_vel, offset ) ) {
    float amplitude_multiplier = 0.2; /* to avoid clipping */

    const std::pair<float, float> curr_sample = note_repo.get_sample( true, adj_event_note, event_vel, offset );

    // Update key future
    k.future.at( offset ).first += curr_sample.first * amplitude_multiplier;
    k.future.at( offset ).second += curr_sample.second * amplitude_multiplier;

    // Update total future
    total_future.at( offset ).first += curr_sample.first * amplitude_multiplier;
    total_future.at( offset ).second += curr_sample.second * amplitude_multiplier;

    offset++;
  }
}

void Synthesizer::add_key_release( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = true;
  k.future.resize( 48000 * 30 ); // Samples per second * 30 seconds (maximum length of key press sound)

  float vol_ratio = 1.0;

  // Update key future and total future
  for ( size_t offset = 0; offset < 48000 * 30; offset++ ) {
    if ( vol_ratio > 0 )
      vol_ratio -= 0.0001;

    const std::pair<float, float> new_sample
      = { k.future.at( offset ).first * vol_ratio, k.future.at( offset ).second * vol_ratio };

    // Update total future by getting delta of new key future and old key future
    total_future.at( offset ).first += new_sample.first - k.future.at( offset ).first;
    total_future.at( offset ).second += new_sample.second - k.future.at( offset ).second;

    // Update key future
    k.future.at( offset ).first = new_sample.first;
    k.future.at( offset ).second = new_sample.second;

    // Add release sound
    if ( !note_repo.note_finished( false, adj_event_note, event_vel, offset ) ) {
      float amplitude_multiplier = exp10( -37 / 20.0 ) * 0.2;

      const std::pair<float, float> rel_sample = note_repo.get_sample( false, adj_event_note, event_vel, offset );

      // Update key future
      k.future.at( offset ).first += rel_sample.first * amplitude_multiplier;
      k.future.at( offset ).second += rel_sample.second * amplitude_multiplier;

      // Update total future
      total_future.at( offset ).first += rel_sample.first * amplitude_multiplier;
      total_future.at( offset ).second += rel_sample.second * amplitude_multiplier;
    }
  }
}

wav_frame_t Synthesizer::get_curr_sample() const
{
  return total_future.front();
}

void Synthesizer::advance_sample()
{
  frames_processed++;
  total_future.pop_front();
  total_future.push_back( { 0, 0 } );
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& k = keys.at( i );
    if ( k.future.size() > 0 )
      k.future.pop_front();
  }
}
