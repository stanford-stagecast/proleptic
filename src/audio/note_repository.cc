#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <iostream>

using namespace std;

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
  add_notes( "F#6", 3, false );
  add_notes( "A6", 3, false );
  add_notes( "C7", 3, false );
  add_notes( "D#7", 3, false );
  add_notes( "F#7", 3, false );
  add_notes( "A7", 3, false );
  add_notes( "C8", 2, false );

  cerr << "Added " << notes.size() << " notes\n";
}

const WavWrapper& NoteRepository::get_wav( const uint8_t direction,
                                           const uint8_t note,
                                           const uint8_t velocity ) const
{
  return notes.at( note - 21 ).getFileFromVel( direction, velocity );
}

void NoteRepository::add_notes( const string& name, const unsigned int num_notes, const bool has_damper )
{
  unsigned int note_num_base = notes.size() + 1;
  for ( unsigned int i = 0; i < num_notes; i++ ) {
    unsigned int release_sample_num = note_num_base + i;

    notes.emplace_back( name, release_sample_num, has_damper );
    /* do we need to bend the pitch? */

    const unsigned int pitch_bend_modulus = ( release_sample_num - 1 ) % 3;

    if ( pitch_bend_modulus == 0 ) {
      std::cerr << "NOT bending for: " << name << " = " << release_sample_num << "\n";
    } else if ( pitch_bend_modulus == 1 ) {
      std::cerr << "Bending UP from " << name << "\n";
    } else {
      std::cerr << "Bending DOWN from " << name << "\n";
    }
  }
}
