#include "synthesizer.hh"
#include <cmath>
#include <iostream>

constexpr unsigned int NUM_KEYS = 88;
constexpr unsigned int KEY_OFFSET = 21;

Synthesizer::Synthesizer()
{
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    sounds.push_back( { {}, {}, {}, {} } );
  }
}

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    bool direction = midi_processor.get_event_type() == 144 ? true : false;
    auto& s = sounds.at( midi_processor.get_event_note() - KEY_OFFSET );

    if ( !direction ) {
      s.releases.push_back(0);
      s.volumes.back() = 0.99;
    } else {
      s.presses.push_back(0);
      s.volumes.push_back(1.0);
      s.velocities.push_back(midi_processor.get_event_velocity());
    }

    midi_processor.pop_event();
  }
}

wav_frame_t Synthesizer::calculate_curr_sample() const
{
  std::pair<float, float> total_sample = { 0, 0 };

  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& s = sounds.at( i );
    size_t active_presses = s.presses.size();
    size_t active_releases = s.releases.size();

    for (size_t j = 0; j < active_presses; j++) {
      const auto& wav_file = note_repo.get_wav( true, i, s.velocities.at(j) );

      float amplitude_multiplier = s.volumes.at(j) * 0.7; /* to avoid clipping */


      const std::pair<float, float> curr_sample = wav_file.view( s.presses.at(j) );

      total_sample.first += curr_sample.first * amplitude_multiplier;
      total_sample.second += curr_sample.second * amplitude_multiplier;
    }

    for (size_t j = 0; j < active_releases; j++) {
      const auto& wav_file = note_repo.get_wav( false, i, 10 ); // arbitrary velocity for now

      float amplitude_multiplier = exp10( -37 / 20.0 ) * 0.7; /* to avoid clipping */

      const std::pair<float, float> curr_sample = wav_file.view( s.releases.at(j) );

      total_sample.first += curr_sample.first * amplitude_multiplier;
      total_sample.second += curr_sample.second * amplitude_multiplier;
    }

  }

  return total_sample;
}

void Synthesizer::advance_sample()
{
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& s = sounds.at( i );
    

    for (size_t j = 0; j < s.presses.size(); j++) {
      s.presses.at(j)++;

      if ( note_repo.get_wav( true, i, s.velocities.at(j) ).at_end( s.presses.at(j) ) ) {
        s.presses.pop_front();
        s.velocities.pop_front();
        s.volumes.pop_front();
        j--;
      } else if ( ( s.volumes.at(j) < 1.0 ) & ( s.volumes.at(j) > 0 ) ) {
        s.volumes.at(j) -= 0.0001;
      }
    }

    for (size_t j = 0; j < s.releases.size(); j++) {
      s.releases.at(j)++;

      if ( note_repo.get_wav( false, i, 10 ).at_end( s.releases.at(j) ) ) {
        s.releases.pop_front();
        j--;
      }
    }

    
  }
}
