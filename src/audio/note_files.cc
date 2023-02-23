#include "note_files.hh"
#include <iostream>
#include <cmath>

using namespace std;

static const string suff_slow = "v1-PA.wav";
static const string suff_med = "v8.5-PA.wav";
static const string suff_fast = "v16.wav";

NoteFiles::NoteFiles( const string& sample_directory,
                      const string& note,
                      const size_t key_num,
                      const bool has_damper )
  : slow( sample_directory + note + suff_slow )
  , med( sample_directory + note + suff_med )
  , fast( sample_directory + note + suff_fast )
  , rel( sample_directory + "rel" + to_string( key_num ) + ".wav" )
  , slow_name ( note + suff_slow )
  , med_name ( note + suff_med )
  , fast_name ( note + suff_fast )
  , has_damper_( has_damper )
{}

void NoteFiles::bend_pitch( const double pitch_bend_ratio )
{
  string suffix = "";
  if (pitch_bend_ratio == pow( 2, -1.0 / 12.0 )) {
    suffix = "-2";
  } else {
    suffix = "-1";
  }
  slow.bend_pitch( pitch_bend_ratio, "new_wavs/" + slow_name + suffix + ".wav" );
  med.bend_pitch( pitch_bend_ratio, "new_wavs/" + med_name + suffix + ".wav" );
  fast.bend_pitch( pitch_bend_ratio, "new_wavs/" + fast_name + suffix + ".wav" );
}
