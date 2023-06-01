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

  size_t offset = 0;

  const std::vector<std::pair<float, float>> samples = note_repo.get_wav( true, adj_event_note, event_vel );

  // Update key future and total future
  while ( !note_repo.note_finished( true, adj_event_note, event_vel, offset ) ) {
    float amplitude_multiplier = 0.2; /* to avoid clipping */

    const std::pair<float, float> curr_sample = samples.at( offset );

    // Update key future
    k.future.at( get_buff_idx( offset ) ).first += curr_sample.first * amplitude_multiplier;
    k.future.at( get_buff_idx( offset ) ).second += curr_sample.second * amplitude_multiplier;

    // Update total future
    total_future.at( get_buff_idx( offset ) ).first += curr_sample.first * amplitude_multiplier;
    total_future.at( get_buff_idx( offset ) ).second += curr_sample.second * amplitude_multiplier;

    offset++;
  }
}

void Synthesizer::add_shallow_key_press( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = false;

  NoteRepository::WavCombination combo = note_repo.get_keydown_combination( adj_event_note, event_vel );

  const size_t max_size = combo.a->size();
  if ( max_size > k.future.size() ) {
    throw runtime_error( "note too big!" );
  }
}

void Synthesizer::add_key_release( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = true;

  float vol_ratio = 1.0;

  const std::vector<std::pair<float, float>> rel_samples = note_repo.get_wav( true, adj_event_note, event_vel );

  // Update key future and total future
  for ( size_t offset = 0; offset < 48000 * 30; offset++ ) {
    if ( vol_ratio > 0 )
      vol_ratio -= 0.0001;

    if ( !sustain_down ) {
      const std::pair<float, float> new_sample = { k.future.at( get_buff_idx( offset ) ).first * vol_ratio,
                                                   k.future.at( get_buff_idx( offset ) ).second * vol_ratio };

      // Update total future by getting delta of new key future and old key future
      total_future.at( get_buff_idx( offset ) ).first
        += new_sample.first - k.future.at( get_buff_idx( offset ) ).first;
      total_future.at( get_buff_idx( offset ) ).second
        += new_sample.second - k.future.at( get_buff_idx( offset ) ).second;

      // Update key future
      k.future.at( get_buff_idx( offset ) ).first = new_sample.first;
      k.future.at( get_buff_idx( offset ) ).second = new_sample.second;
    }

    // Add release sound
    if ( !note_repo.note_finished( false, adj_event_note, event_vel, offset ) ) {
      float amplitude_multiplier = exp10( -37 / 20.0 ) * 0.2;

      const std::pair<float, float> rel_sample = rel_samples[offset];

      // Update key future
      k.future.at( get_buff_idx( offset ) ).first += rel_sample.first * amplitude_multiplier;
      k.future.at( get_buff_idx( offset ) ).second += rel_sample.second * amplitude_multiplier;

      // Update total future
      total_future.at( get_buff_idx( offset ) ).first += rel_sample.first * amplitude_multiplier;
      total_future.at( get_buff_idx( offset ) ).second += rel_sample.second * amplitude_multiplier;
    }
  }
}

wav_frame_t Synthesizer::get_curr_sample() const
{
  return total_future.at( get_buff_idx( 0 ) );
}

void Synthesizer::advance_sample()
{
  frames_processed++;
}
