#include "synthesizer.hh"
#include <iostream>
#include <set>

void Synthesizer::process_new_data( FileDescriptor& fd )
{
  midi_processor.read_from_fd( fd );

  while ( midi_processor.has_event() ) {
    sound new_sound = { (uint8_t)midi_processor.get_event_type(),
                        (uint8_t)midi_processor.get_event_note(),
                        (uint8_t)midi_processor.get_event_velocity(),
                        0 };

    active_sounds.push_back( new_sound );

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
      std::pair<float, float> curr_sample = note_repo.get_sample( *it );
      float vol_ratio = 1;
      if ( s.direction == 128 )
        vol_ratio = 5;

      // TODO: Change how we prevent clipping
      total_sample.first += curr_sample.first / vol_ratio;   // num_sounds;
      total_sample.second += curr_sample.second / vol_ratio; // num_sounds;
      it->curr_offset += 1;
      if ( note_repo.reached_end( s ) ) {
        cout << "finished\n\n";
        active_sounds.erase( it );
      } else {
        ++it;
      }
    }
  }

  return total_sample;
}