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
  , keys( NUM_KEYS )
{}

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

  const std::vector<std::pair<float, float>> samples = note_repo.get_wav( true, adj_event_note, event_vel );
  auto key_region = k.future.mutable_storage( frames_processed % FUTURE_LENGTH );
  auto total_region = total_future.mutable_storage( frames_processed % FUTURE_LENGTH );

  size_t offset = 0;

  // Update key future and total future
  while ( !note_repo.note_finished( true, adj_event_note, event_vel, offset ) ) {
    float amplitude_multiplier = 0.2; /* to avoid clipping */

    const std::pair<float, float> curr_sample = samples.at( offset );

    // Update key future
    auto& sample = key_region[offset];
    sample.first += curr_sample.first * amplitude_multiplier;
    sample.second += curr_sample.second * amplitude_multiplier;

    // Update total future
    auto& total_sample = total_region[offset];
    total_sample.first += curr_sample.first * amplitude_multiplier;
    total_sample.second += curr_sample.second * amplitude_multiplier;

    offset++;
  }
}

void Synthesizer::add_shallow_key_press( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = false;

  NoteRepository::WavCombination combo = note_repo.get_keydown_combination( adj_event_note, event_vel );

#if 0
  const size_t max_size = combo.a->size();
  if ( max_size > k.future.size() ) {
    throw runtime_error( "note too big!" );
  }

  size_t offset = 0;

  // Update key future and total future
  for ( int i = 0 ; i < max_size; i++ ) {
    float amplitude_multiplier = 0.2; /* to avoid clipping */

    const std::pair<float, float> curr_sample
      = { combo.a->at( i ).first * combo.a_weight + combo.b->at( i ).first * combo.b_weight,
          combo.a->at( i ).second * combo.a_weight + combo.b->at( i ).second * combo.b_weight };

    // Update key future
    k.future.at( get_buff_idx( i ) ).first += curr_sample.first;
    k.future.at( get_buff_idx( i ) ).second += curr_sample.second;

    // Update total future
    total_future.at( get_buff_idx( i ) ).first += curr_sample.first;
    total_future.at( get_buff_idx( i ) ).second += curr_sample.second;
  }
#endif
}

void Synthesizer::add_key_release( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = true;

  float vol_ratio = 1.0;

  const std::vector<std::pair<float, float>> rel_samples = note_repo.get_wav( true, adj_event_note, event_vel );
  auto key_region = k.future.mutable_storage( frames_processed % FUTURE_LENGTH );
  auto total_region = total_future.mutable_storage( frames_processed % FUTURE_LENGTH );
  
  // Update key future and total future
  for ( size_t offset = 0; offset < 48000 * 30; offset++ ) {
    auto& key_sample = key_region[ offset ];
    auto& total_sample = total_region[ offset ];

    if ( vol_ratio > 0 )
      vol_ratio -= 0.0001;

    if ( !sustain_down ) {
      const std::pair<float, float> new_sample = { key_sample.first * vol_ratio,
                                                   key_sample.second * vol_ratio };

      // Update total future by getting delta of new key future and old key future
      total_sample.first += new_sample.first - key_sample.first;
      total_sample.second += new_sample.second - key_sample.second;

      // Update key future
      key_sample = new_sample;
    }

    // Add release sound
    if ( !note_repo.note_finished( false, adj_event_note, event_vel, offset ) ) {
      float amplitude_multiplier = exp10( -37 / 20.0 ) * 0.2;

      const std::pair<float, float> rel_sample = rel_samples[offset];

      // Update key future
      key_sample.first += rel_sample.first * amplitude_multiplier;
      key_sample.second += rel_sample.second * amplitude_multiplier;

      // Update total future
      total_sample.first += rel_sample.first * amplitude_multiplier;
      total_sample.second += rel_sample.second * amplitude_multiplier;
    }
  }
}

wav_frame_t Synthesizer::get_curr_sample() const
{
  return total_future.storage(frames_processed)[0];
}

void Synthesizer::advance_sample()
{
  frames_processed++;
}
