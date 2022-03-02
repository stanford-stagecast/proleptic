#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <iostream>

NoteRepository::NoteRepository()
{

  notes.push_back( { "A0", 1 } );
  notes.push_back( { "C1", 4 } );
  notes.push_back( { "D#1", 7 } );
  notes.push_back( { "F#1", 10 } );
  notes.push_back( { "A1", 13 } );
  notes.push_back( { "C2", 16 } );
  notes.push_back( { "D#2", 19 } );
  notes.push_back( { "F#2", 22 } );
  notes.push_back( { "A2", 25 } );
  notes.push_back( { "C3", 28 } );
  notes.push_back( { "D#3", 31 } );
  notes.push_back( { "F#3", 34 } );
  notes.push_back( { "A3", 37 } );
  notes.push_back( { "C4", 40 } );
  notes.push_back( { "D#4", 43 } );
  notes.push_back( { "F#4", 46 } );
  notes.push_back( { "A4", 49 } );
  notes.push_back( { "C5", 52 } );
  notes.push_back( { "D#5", 55 } );
  notes.push_back( { "F#5", 58 } );
  notes.push_back( { "A5", 61 } );
  notes.push_back( { "C6", 64 } );
  notes.push_back( { "D#6", 67 } );
}

bool NoteRepository::reached_end( sound curr_sound )
{
  size_t id = curr_sound.note - 21;
  if ( id % 3 != 0 ) {
    return true;
  }
  id /= 3;
  return notes[curr_sound.note - 21]
    .getFileFromVel( curr_sound.direction, curr_sound.velocity )
    .at_end( curr_sound.curr_offset );
}

wav_frame_t NoteRepository::get_sample( sound curr_sound )
{
  size_t id = curr_sound.note - 21;
  if ( id % 3 != 0 ) {
    return { 0, 0 };
  }
  id /= 3;
  return notes[curr_sound.note - 21]
    .getFileFromVel( curr_sound.direction, curr_sound.velocity )
    .view( curr_sound.curr_offset );
}