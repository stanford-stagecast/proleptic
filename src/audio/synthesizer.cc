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
  add_shallow_key_press( adj_event_note, event_vel );
}

void Synthesizer::add_shallow_key_press( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = false;

  NoteRepository::WavCombination combo = note_repo.get_keydown_combination( adj_event_note, event_vel );
  auto key_region = k.future.mutable_storage( frames_processed % FUTURE_LENGTH );
  auto total_region = total_future.mutable_storage( frames_processed % FUTURE_LENGTH );

  const size_t max_size = combo.a->size();
  if ( max_size > total_region.size() or max_size > key_region.size() or max_size > FUTURE_LENGTH ) {
    throw runtime_error( "note too big!" );
  }

  size_t offset = 0;
  const vector<wav_frame_t>& a = *combo.a;
  const vector<wav_frame_t>& b = *combo.b;

  // Update key future and total future
  for ( int i = 0; i < max_size; i++ ) {
    const std::pair<float, float> curr_sample = { a[i].first * combo.a_weight + b[i].first * combo.b_weight,
                                                  a[i].second * combo.a_weight + b[i].second * combo.b_weight };

    // Update key future
    key_region[i].first += curr_sample.first;
    key_region[i].second += curr_sample.second;

    // Update total future
    total_region[i].first += curr_sample.first;
    total_region[i].second += curr_sample.second;
  }
}

void Synthesizer::add_key_release( uint8_t adj_event_note, uint8_t event_vel )
{
  auto& k = keys.at( adj_event_note );
  k.released = true;

  float vol_ratio = 1.0;
  auto key_region = k.future.mutable_storage( frames_processed % FUTURE_LENGTH );
  auto total_region = total_future.mutable_storage( frames_processed % FUTURE_LENGTH );

  // Update key future and total future
  for ( size_t offset = 0; offset < FUTURE_LENGTH; offset++ ) {
    auto& key_sample = key_region[offset];
    auto& total_sample = total_region[offset];

    if ( vol_ratio > 0 )
      vol_ratio -= 0.0001;

    if ( !sustain_down ) {
      const std::pair<float, float> new_sample = { key_sample.first * vol_ratio, key_sample.second * vol_ratio };

      // Update total future by getting delta of new key future and old key future
      total_sample.first += new_sample.first - key_sample.first;
      total_sample.second += new_sample.second - key_sample.second;

      // Update key future
      key_sample = new_sample;
    }
  }
}

wav_frame_t Synthesizer::get_curr_sample() const
{
  return total_future.storage( frames_processed )[0];
}

void Synthesizer::advance_sample()
{
  frames_processed++;
}
