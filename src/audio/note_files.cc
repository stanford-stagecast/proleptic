#include "note_files.hh"
#include <iostream>

using namespace std;

static const string prefix = "../../../../SlenderSalamander48khz24bit/samples/";
static const string suff_slow = "v1-PA.wav";
static const string suff_med = "v8.5-PA.wav";
static const string suff_fast = "v16.wav";

NoteFiles::NoteFiles( const string& note, const size_t key_num )
  : slow( prefix + note + suff_slow )
  , med( prefix + note + suff_med )
  , fast( prefix + note + suff_fast )
  , rel( prefix + "rel" + to_string( key_num ) + ".wav" )
{}

const WavWrapper& NoteFiles::getFileFromVel( uint8_t dir, uint8_t vel ) const
{
  if ( dir == 128 ) {
    return rel;
  } else {
    // TODO: Create less arbitrary velocity boundaries,
    // or mix velocity more continously
    if ( vel > 100 )
      return fast;
    if ( vel > 50 )
      return med;
  }

  return slow;
}
