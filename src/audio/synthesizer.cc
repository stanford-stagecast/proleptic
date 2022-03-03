#include "synthesizer.hh"
#include <cmath>
#include <iostream>

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    active_sounds.emplace_back(
      midi_processor.get_event_type(), midi_processor.get_event_note(), midi_processor.get_event_velocity(), 0 );

    midi_processor.pop_event();
  }
}

wav_frame_t Synthesizer::calculate_curr_sample() const
{
  std::pair<float, float> total_sample = { 0, 0 };

  for ( const auto& s : active_sounds ) {
    const auto& wav_file = note_repo.get_wav( s.direction, s.note, s.velocity );

    const float amplitude_multiplier = ( s.direction == 144 ) ? 1.0 : exp10( -37 / 20.0 );

    const std::pair<float, float> curr_sample = wav_file.view( s.curr_offset );

    total_sample.first += curr_sample.first * amplitude_multiplier;
    total_sample.second += curr_sample.second * amplitude_multiplier;
  }

  return total_sample;
}

void Synthesizer::advance_sample()
{
  auto it = active_sounds.begin();
  while ( it != active_sounds.end() ) {
    auto& s = *it;

    s.curr_offset++;

    if ( note_repo.get_wav( s.direction, s.note, s.velocity ).at_end( s.curr_offset ) ) {
      it = active_sounds.erase( it );
    } else {
      ++it;
    }
  }
}
