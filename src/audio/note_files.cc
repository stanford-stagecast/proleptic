#include "note_files.hh"
#include <iostream>

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
  , has_damper_( has_damper )
{}

void NoteFiles::bend_pitch( const double pitch_bend_ratio )
{
  slow.bend_pitch( pitch_bend_ratio );
  med.bend_pitch( pitch_bend_ratio );
  fast.bend_pitch( pitch_bend_ratio );
}
