#include "note_files.hh"
#include <iostream>

using namespace std;

static const string prefix = "../../../../SlenderSalamander48khz24bit/samples/";
static const string suff_slow = "v1-PA.wav";
static const string suff_med = "v8.5-PA.wav";
static const string suff_fast = "v16.wav";

NoteFiles::NoteFiles( const string& note, const size_t key_num, const bool has_damper )
  : slow( prefix + note + suff_slow )
  , med( prefix + note + suff_med )
  , fast( prefix + note + suff_fast )
  , rel( prefix + "rel" + to_string( key_num ) + ".wav" )
  , has_damper_( has_damper )
{}

void NoteFiles::bend_pitch( const double pitch_bend_ratio )
{
  slow.bend_pitch( pitch_bend_ratio );
  med.bend_pitch( pitch_bend_ratio );
  fast.bend_pitch( pitch_bend_ratio );
}
