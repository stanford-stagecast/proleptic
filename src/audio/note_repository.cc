#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <iostream>

NoteRepository::NoteRepository()
{
  add_notes( "A0", 2 );
  add_notes( "C1", 3 );
  add_notes( "D#1", 3 );
  add_notes( "F#1", 3 );
  add_notes( "A1", 3 );
  add_notes( "C2", 3 );
  add_notes( "D#2", 3 );
  add_notes( "F#2", 3 );
  add_notes( "A2", 3 );
  add_notes( "C3", 3 );
  add_notes( "D#3", 3 );
  add_notes( "F#3", 3 );
  add_notes( "A3", 3 );
  add_notes( "C4", 3 );
  add_notes( "D#4", 3 );
  add_notes( "F#4", 3 );
  add_notes( "A4", 3 );
  add_notes( "C5", 3 );
  add_notes( "D#5", 3 );
  add_notes( "F#5", 3 );
  add_notes( "A5", 3 );
  add_notes( "C6", 3 );
  add_notes( "D#6", 3 );

  // keys below here do not have dampers
  add_notes( "F#6", 3 );
  add_notes( "A6", 3 );
  add_notes( "C7", 3 );
  add_notes( "D#7", 3 );
  add_notes( "F#7", 3 );
  add_notes( "A7", 3 );
  add_notes( "C8", 2 );

  cerr << "Added " << notes.size() << " notes\n";
}

const WavWrapper& NoteRepository::get_wav( const uint8_t direction,
                                           const uint8_t note,
                                           const uint8_t velocity ) const
{
  size_t id = note - 21;
  id /= 3; /* XXX */

  return notes.at( id ).getFileFromVel( direction, velocity );
}

void NoteRepository::add_notes( const string& name, const unsigned int num_notes )
{
  unsigned int note_num_base = notes.size() + 1;
  for ( unsigned int i = 0; i < num_notes; i++ ) {
    notes.emplace_back( name, note_num_base + i );
  }
}
