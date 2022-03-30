#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <cmath>
#include <iostream>

using namespace std;

constexpr float LOW_XFOUT_LOVEL = 8; // Equivalent to MED_XFIN_LOVEL
constexpr float LOW_XFOUT_HIVEL = 59; // Equivalent to MED_XFIN_HIVEL
constexpr float HIGH_XFIN_LOVEL = 67; // Equivalent to MED_XFOUT_LOVEL
constexpr float HIGH_XFIN_HIVEL = 119; // Equivalent to MED_XFOUT_HIVEL

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

const wav_frame_t NoteRepository::get_sample( const bool direction, const size_t note, const uint8_t velocity, const unsigned long offset ) const
{
  if (direction) {
    if (velocity <= LOW_XFOUT_LOVEL) {
      return notes.at( note ).getSlow().view( offset );
    } else if (velocity <= LOW_XFOUT_HIVEL) {
      std::pair<float, float> new_samp = notes.at( note ).getSlow().view( offset );
      new_samp.first *= (LOW_XFOUT_HIVEL - velocity) / (LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL);
      new_samp.second *= (LOW_XFOUT_HIVEL - velocity) / (LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL);

      std::pair<float, float> med_samp = notes.at( note ).getMed().view( offset );
      new_samp.first += med_samp.first * ((velocity - LOW_XFOUT_LOVEL) / (LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL));
      new_samp.second += med_samp.second * ((velocity - LOW_XFOUT_LOVEL) / (LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL));

      return new_samp;
    } else if (velocity <= HIGH_XFIN_LOVEL) {
      return notes.at( note ).getMed().view( offset );
    } else if (velocity <= HIGH_XFIN_HIVEL) {
      std::pair<float, float> new_samp = notes.at( note ).getMed().view( offset );
      new_samp.first *= (HIGH_XFIN_HIVEL - velocity) / (HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL);
      new_samp.second *= (HIGH_XFIN_HIVEL - velocity) / (HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL);

      std::pair<float, float> fast_samp = notes.at( note ).getFast().view( offset );
      new_samp.first += fast_samp.first * ((velocity - HIGH_XFIN_LOVEL) / (HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL));
      new_samp.second += fast_samp.second * ((velocity - HIGH_XFIN_LOVEL) / (HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL));

      return new_samp;
    } else {
      return notes.at( note ).getFast().view( offset );
    }
  } 

  return notes.at( note ).getRel().view( offset );
}

bool NoteRepository::note_finished( const bool direction, const size_t note, const uint8_t velocity, const unsigned long offset ) const
{
  if (direction) {
    if (velocity <= LOW_XFOUT_LOVEL) {
      return notes.at( note ).getSlow().at_end( offset );
    } else if (velocity <= LOW_XFOUT_HIVEL) {
      return notes.at( note ).getMed().at_end( offset );
    } else if (velocity <= HIGH_XFIN_LOVEL) {
      return notes.at( note ).getMed().at_end( offset );
    } else if (velocity <= HIGH_XFIN_HIVEL) {
      return notes.at( note ).getFast().at_end( offset );
    } else {
      return notes.at( note ).getFast().at_end( offset );
    }
  } 

  return notes.at( note ).getRel().at_end( offset );
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
      notes.back().bend_pitch( pow( 2, -1.0 / 12.0 ) );
    } else {
      std::cerr << "Bending DOWN from " << name << "\n";
      notes.back().bend_pitch( pow( 2, 1.0 / 12.0 ) );
    }
  }
}
