#include "note_repository.hh"

#include <iostream>

NoteRepository::NoteRepository()
{
  // TODO: Finalize and implement WAV storage
  WavWrapper c1Slow { "../../../../C1v1-PA.wav" };
  WavWrapper c1Med { "../../../../C1v8.5-PA.wav" };
  WavWrapper c1Fast { "../../../../C1v16.wav" };
  WavWrapper c1Rel { "../../../../rel4.wav" };

  WavWrapper a2Slow { "../../../../A2v1-PA.wav" };
  WavWrapper a2Med { "../../../../A2v8.5-PA.wav" };
  WavWrapper a2Fast { "../../../../A2v16.wav" };

  // Actual idx: 95*4 + 0 + 0
  wav_files.push_back( c1Slow );
  // Actual idx: 95*4 + 0 + 1
  wav_files.push_back( c1Med );
  // Actual idx: 95*4 + 0 + 0
  wav_files.push_back( c1Fast );
  // Actual idx: 95*4 + 3 + 0
  wav_files.push_back( c1Rel );

  wav_files.push_back( a2Slow );

  wav_files.push_back( a2Med );

  wav_files.push_back( a2Fast );

  wav_files.push_back( c1Rel );
}

note_id NoteRepository::get_note_idx( sound curr_sound )
{
  // TODO: Switch back to this when indexes are fixed
  // size_t note_idx = curr_sound.note;
  size_t note_idx = 0;

  if ( curr_sound.note == 96 )
    note_idx = 4;
  if ( curr_sound.direction == 128 ) {
    note_idx += 3;
  } else {
    // TODO: Create less arbitrary velocity boundaries,
    // or mix velocity more continously
    if ( curr_sound.velocity > 50 )
      note_idx++;
    if ( curr_sound.velocity > 120 )
      note_idx++;
  }

  return note_idx;
}

bool NoteRepository::reached_end( sound curr_sound )
{
  note_id id = get_note_idx( curr_sound );

  return wav_files[id].at_end( curr_sound.curr_offset );
}

wav_frame_t NoteRepository::get_sample( sound curr_sound )
{
  note_id id = get_note_idx( curr_sound );

  return wav_files[id].view( curr_sound.curr_offset );
}