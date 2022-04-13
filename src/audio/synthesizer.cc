#include "synthesizer.hh"
#include <cmath>
#include <iostream>

constexpr unsigned int NUM_KEYS = 88;
constexpr unsigned int KEY_OFFSET = 21;

constexpr unsigned int KEY_DOWN = 144;
constexpr unsigned int KEY_UP = 128;
constexpr unsigned int SUSTAIN = 176;

Synthesizer::Synthesizer()
{
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    keys.push_back( { {}, {} } );
  }
}

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    if (midi_processor.get_event_type() == SUSTAIN) {
      //std::cerr << (size_t) midi_processor.get_event_type() << " " << (size_t) midi_processor.get_event_note() << " " << (size_t)midi_processor.get_event_velocity() << "\n";
      if (midi_processor.get_event_velocity() == 127) sustain_down = true;
      else  sustain_down = false;
    } else if (midi_processor.get_event_type() == KEY_DOWN || midi_processor.get_event_type() == KEY_UP) {
      bool direction = midi_processor.get_event_type() == KEY_DOWN ? true : false;
      auto& k = keys.at( midi_processor.get_event_note() - KEY_OFFSET );

      if ( !direction ) {
        k.releases.push_back({ 0, midi_processor.get_event_velocity(), 1.0, false });
        k.presses.back().released = true;
      } else {
        k.presses.push_back({ 0, midi_processor.get_event_velocity(), 1.0, false });
      }

      
    }
    midi_processor.pop_event();
  }
}

wav_frame_t Synthesizer::calculate_curr_sample() const
{
  std::pair<float, float> total_sample = { 0, 0 };

  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& k = keys.at( i );
    size_t active_presses = k.presses.size();
    size_t active_releases = k.releases.size();

    for (size_t j = 0; j < active_presses; j++) {
      float amplitude_multiplier = k.presses.at(j).vol_ratio * 0.2; /* to avoid clipping */

      const std::pair<float, float> curr_sample = note_repo.get_sample(true, i, k.presses.at(j).velocity, k.presses.at(j).offset );

      total_sample.first += curr_sample.first * amplitude_multiplier;
      total_sample.second += curr_sample.second * amplitude_multiplier;
    }

    for (size_t j = 0; j < active_releases; j++) {

      float amplitude_multiplier = exp10( -37 / 20.0 ) * 0.2; /* to avoid clipping */

      const std::pair<float, float> curr_sample = note_repo.get_sample( false, i, k.releases.at(j).velocity, k.releases.at(j).offset );

      total_sample.first += curr_sample.first * amplitude_multiplier;
      total_sample.second += curr_sample.second * amplitude_multiplier;
    }

  }

  return total_sample;
}

void Synthesizer::advance_sample()
{
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& k = keys.at( i );
    size_t active_presses = k.presses.size();
    size_t active_releases = k.releases.size();

    for (size_t j = 0; j < active_presses; j++) {
      k.presses.at(j).offset++;

      if ( note_repo.note_finished( true, i, k.presses.at(j).velocity, k.presses.at(j).offset ) ) {
        k.presses.erase(k.presses.begin());
        j--;
        active_presses--;
      } else if ( ( k.presses.at(j).released && !sustain_down ) & ( k.presses.at(j).vol_ratio > 0 ) ) {
        k.presses.at(j).vol_ratio -= 0.0001;
      }
    }

    for (size_t j = 0; j < active_releases; j++) {
      k.releases.at(j).offset++;

      if ( note_repo.note_finished( false, i, k.releases.at(j).velocity, k.releases.at(j).offset ) ) {
        k.releases.erase(k.releases.begin());
        j--;
        active_releases--;
      }
    }

    
  }
}
