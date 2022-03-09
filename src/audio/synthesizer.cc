#include "synthesizer.hh"
#include <cmath>
#include <iostream>

constexpr unsigned int NUM_KEYS = 88;
constexpr unsigned int KEY_OFFSET = 21;

Synthesizer::Synthesizer()
{
  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    sounds.push_back( { false, true, 0, 0, 1.0 } );
  }
}

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    bool direction = midi_processor.get_event_type() == 144 ? true : false;
    auto& s = sounds.at( midi_processor.get_event_note() - KEY_OFFSET );

    // s.direction = direction;
    if ( !direction ) {
      s.ratio = 0.99;
    } else {
      s.ratio = 1.0;
      s.velocity = midi_processor.get_event_velocity();
      s.active = true;
      s.curr_offset = 0;
    }

    midi_processor.pop_event();
  }
}

wav_frame_t Synthesizer::calculate_curr_sample() const
{
  std::pair<float, float> total_sample = { 0, 0 };

  for ( size_t i = 0; i < NUM_KEYS; i++ ) {
    auto& s = sounds.at( i );

    if ( s.active ) {
      const auto& wav_file = note_repo.get_wav( s.direction, i, s.velocity );

      float amplitude_multiplier = s.direction ? s.ratio : exp10( -37 / 20.0 );

      amplitude_multiplier *= 0.7; /* to avoid clipping */

      const std::pair<float, float> curr_sample = wav_file.view( s.curr_offset );

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

    s.curr_offset++;

    if ( note_repo.get_wav( s.direction, i, s.velocity ).at_end( s.curr_offset ) ) {
      s.active = false;
    } else if ( ( s.ratio < 1.0 ) & ( s.ratio > 0 ) ) {
      s.ratio -= 0.0001;
    }
  }
}
