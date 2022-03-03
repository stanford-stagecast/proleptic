#include "synthesizer.hh"
#include <iostream>
#include <set>

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    active_sounds.emplace_back(
      midi_processor.get_event_type(), midi_processor.get_event_note(), midi_processor.get_event_velocity(), 0 );

    midi_processor.pop_event();
  }
}

wav_frame_t Synthesizer::calculate_curr_sample()
{
  std::pair<float, float> total_sample = { 0, 0 };
  size_t num_sounds = active_sounds.size();

  if ( num_sounds > 0 ) {
    for ( std::vector<sound>::iterator it = active_sounds.begin(); it != active_sounds.end(); ) {
      sound s = *it;
      const auto& wav_file = note_repo.get_wav( s.direction, s.note, s.velocity );
      std::pair<float, float> curr_sample = wav_file.view( s.curr_offset );
      float vol_ratio = 1;
      if ( s.direction == 128 )
        vol_ratio = 20;

      // TODO: Change how we prevent clipping
      total_sample.first += curr_sample.first / vol_ratio;   // num_sounds;
      total_sample.second += curr_sample.second / vol_ratio; // num_sounds;
      it->curr_offset += 1;
      if ( wav_file.at_end( s.curr_offset ) ) {
        cerr << "finished\n\n";
        active_sounds.erase( it );
      } else {
        ++it;
      }
    }
  }

  return total_sample;
}
