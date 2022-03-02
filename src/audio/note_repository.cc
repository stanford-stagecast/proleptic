#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <iostream>

NoteRepository::NoteRepository()
{
  notes.emplace_back( "A0", 1 );
  notes.emplace_back( "C1", 4 );
  notes.emplace_back( "D#1", 7 );
  notes.emplace_back( "F#1", 10 );
  notes.emplace_back( "A1", 13 );
  notes.emplace_back( "C2", 16 );
  notes.emplace_back( "D#2", 19 );
  notes.emplace_back( "F#2", 22 );
  notes.emplace_back( "A2", 25 );
  notes.emplace_back( "C3", 28 );
  notes.emplace_back( "D#3", 31 );
  notes.emplace_back( "F#3", 34 );
  notes.emplace_back( "A3", 37 );
  notes.emplace_back( "C4", 40 );
  notes.emplace_back( "D#4", 43 );
  notes.emplace_back( "F#4", 46 );
  notes.emplace_back( "A4", 49 );
  notes.emplace_back( "C5", 52 );
  notes.emplace_back( "D#5", 55 );
  notes.emplace_back( "F#5", 58 );
  notes.emplace_back( "A5", 61 );
  notes.emplace_back( "C6", 64 );
  notes.emplace_back( "D#6", 67 );
}

bool NoteRepository::reached_end( sound curr_sound ) const
{
  size_t id = curr_sound.note - 21;
  if ( id % 3 != 0 ) {
    return true;
  }
  id /= 3;
  return notes.at( curr_sound.note - 21 )
    .getFileFromVel( curr_sound.direction, curr_sound.velocity )
    .at_end( curr_sound.curr_offset );
}

wav_frame_t NoteRepository::get_sample( sound curr_sound ) const
{
  size_t id = curr_sound.note - 21;
  if ( id % 3 != 0 ) {
    return { 0, 0 };
  }
  id /= 3;
  return notes.at( curr_sound.note - 21 )
    .getFileFromVel( curr_sound.direction, curr_sound.velocity )
    .view( curr_sound.curr_offset );
}
