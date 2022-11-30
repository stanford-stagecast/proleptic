#include "autoencoder.hh"
#include "backprop.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "midi_file.hh"
#include "mmap.hh"
#include "piano_roll.hh"
#include "randomize_network.hh"
#include "serdes.hh"
#include "training.hh"

#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static vector<MidiFile> midi_files {};

float vector_min( vector<float> vec )
{
  float min = vec[0];
  for ( float k : vec ) {
    if ( k < min ) {
      min = k;
    }
  }
  return min;
}

int vector_min_index( vector<float> my_vector )
{
  float min = numeric_limits<float>::max();
  int min_index = 0;
  for ( int i = 0; i < (int)my_vector.size(); ++i ) {
    if ( my_vector[i] < min ) {
      min = my_vector[i];
      min_index = i;
    }
  }
  return min_index;
}

float compute_metronome_error( float current_beat, float quarter_beat_length )
{
  float floor_value = (float)floor( current_beat );
  float ceil_value = (float)ceil( current_beat );
  vector<float> all_diff;

  float current_value = floor_value;
  while ( current_value <= ceil_value ) {
    all_diff.push_back( abs( current_beat - current_value ) );
    current_value += quarter_beat_length;
  }

  return vector_min( all_diff );
}

void program_body()
{
  ios::sync_with_stdio( false );

  // read midi files
  const auto& midi = midi_files[1];
  const vector<float> time_in_beats = midi.time_in_beats();
  // const vector<uint32_t> periods = midi.periods();

  // extract the metronome from time_in_beats
  // note: assuming litte or no change in tempo, the metronome should have
  // beat positions that are consistent
  vector<float> metronome_errors;
  for ( int i = 0; i < 3; ++i )
    metronome_errors.push_back( 0.0 );
  for ( float current_beat : time_in_beats ) {
    metronome_errors[0] += compute_metronome_error( current_beat, 0.125 );
    metronome_errors[1] += compute_metronome_error( current_beat, 0.25 );
    metronome_errors[2] += compute_metronome_error( current_beat, 0.5 );
  }

  // determine length of a quarter beat for the metronome
  int metronome_quarter_length_index = vector_min_index( metronome_errors );
  float metronome_quarter_length = 0.0;
  switch ( metronome_quarter_length_index ) {
    case 0:
      metronome_quarter_length = 0.125;
      break;
    case 1:
      metronome_quarter_length = 0.25;
      break;
    case 2:
      metronome_quarter_length = 0.5;
      break;
  }

  // generate the beat positions for the metronome
  vector<float> metronome_beats;
  float current_metronome_beat = 0.0;
  while ( current_metronome_beat < time_in_beats.back() ) {
    metronome_beats.push_back( current_metronome_beat );
    current_metronome_beat += metronome_quarter_length;
  }

  // visualization of the metronome beats
  // (in progress, for now, just print...)
  // for ( float mb : metronome_beats )
  //   cout << mb << endl;
}

int main( int argc, char* argv[] )
{
  (void)argv;
  if ( argc < 0 ) {
    abort();
  }

  if ( Eigen::nbThreads() > 1 ) {
    // hack: Eigen defaults to the number of cores visible, but that's usually 2x the number of useful cores
    // because of hyperthreading
    Eigen::setNbThreads( Eigen::nbThreads() / 2 );
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " [midi directory]\n";
    return EXIT_FAILURE;
  }

  string midi_directory = argv[1];

  try {
    for ( const auto& entry : filesystem::directory_iterator( midi_directory ) ) {
      if ( entry.path().extension() == ".mid" ) {
        cout << "Using file: " << entry.path() << "\n";
        midi_files.emplace_back( MidiFile( entry.path() ) );
        if ( midi_files.back().tracks().size() == 0 ) {
          cout << "No tracks found; skipping!"
               << "\n";
          midi_files.pop_back();
        }
      }
    }
  } catch ( const filesystem::filesystem_error& e ) {
    cerr << "Error finding midi files:\n";
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }

  if ( midi_files.empty() ) {
    cerr << "No midi files found!" << endl;
    return EXIT_FAILURE;
  }
  cout << "Got " << midi_files.size() << " midi files.\n";

  while ( true ) {
    try {
      program_body();
      break;
    } catch ( const runtime_error& e ) {
      cerr << e.what() << "\n";
    } catch ( const exception& e ) {
      cerr << e.what() << "\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}