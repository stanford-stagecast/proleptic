#define _USE_MATH_DEFINES

#include "backprop.hh"
#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "inference.hh"
#include "mmap.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"

#include "time.h"
#include "training.hh"

#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>

using namespace std;

// Input generation parameters
static constexpr float note_timing_variation = 0.05;
static auto prng = get_random_engine();
static auto tempo_distribution = uniform_real_distribution<float>( 30, 240 );
static auto noise_distribution = normal_distribution<float>( 0, note_timing_variation );
static auto pattern_length_distribution = uniform_int_distribution<unsigned>( 2, 8 );
static auto reverse_distribution = binomial_distribution<bool>( 1, 0.5 );
static constexpr int input_size = 64;

static auto tmp_distribution = uniform_real_distribution<float>( 1, 10 );

static auto test_piece_distribution = uniform_int_distribution<unsigned>( 0, 4 );

// Training parameters
// static constexpr int number_of_iterations = 100;
static constexpr float learning_rate = 0.001;
static constexpr int batch_size = 1;

const complex<double> complex_i( 0.0, 1.0 );

struct RandomState
{
  default_random_engine prng { get_random_engine() };
  normal_distribution<float> parameter_distribution { 0.0, 0.1 };

  float sample() { return parameter_distribution( prng ); }
};

template<LayerT Layer>
void randomize_layer( Layer& layer, RandomState& rng )
{
  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) = rng.sample();
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) = rng.sample();
  }
}

template<NetworkT Network>
void randomize_network( Network& network, RandomState& rng )
{
  randomize_layer( network.first, rng );

  if constexpr ( not Network::is_last ) {
    randomize_network( network.rest, rng );
  }
}

vector<float> read_timestamps_from_midi( string midi_file_name )
{
  vector<float> timestamps;
  ifstream in( midi_file_name.c_str() );
  string str;
  // Read the next line from File untill it reaches the end.
  while ( getline( in, str ) ) {
    // Line contains string of length > 0 then save it in vector
    if ( str.size() > 0 )
      timestamps.push_back( stof( str ) );
  }
  // Close The File
  in.close();
  return timestamps;
}

Eigen::Matrix<float, 1, input_size> pick_timestamp_segment( vector<float> midi_timestamps )
{
  Eigen::Matrix<float, 1, input_size> my_timestamps;
  float exit_flag = false;

  float noise = noise_distribution( prng );
  ;
  while ( exit_flag == false ) {
    vector<float> timestamps;
    float current_time = static_cast<float>( rand() )
                         / ( static_cast<float>( RAND_MAX / midi_timestamps[midi_timestamps.size() - 1] ) );
    // float current_time = 20.0;
    for ( int k = midi_timestamps.size() - 1; k >= 0; --k ) {
      if ( midi_timestamps[k] <= current_time ) {
        timestamps.push_back( current_time - midi_timestamps[k] );
      }
      if ( timestamps.size() == input_size ) { // change to 16 later
        exit_flag = true;
        my_timestamps( 0, 0 ) = timestamps[0];
        // for (int i = 1; i < 16; ++i) {
        for ( int i = 1; i < input_size; ++i ) {
          // noise = noise_distribution( prng );
          noise = 0;
          my_timestamps( 0, i ) = timestamps[i] - timestamps[i - 1] + noise;
          // my_timestamps(0,i) = timestamps[i+1] - timestamps[i] + noise;
        }
        break;
      }
    }
  }

  return my_timestamps;
}

auto load_all_songs( string song_directory )
{
  vector<tuple<vector<float>, float>> database;

  // read all songs in the database directory
  using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;
  const char* delim = "-";
  for ( const auto& dirEntry : recursive_directory_iterator( song_directory ) ) {
    // cout << dirEntry.path() << endl;
    string file_path = dirEntry.path().string();
    vector<float> midi_timestamp = read_timestamps_from_midi( file_path );

    // parse tempo
    float tempo = 0.0;
    string tempo_string;
    for ( int k = file_path.length(); k >= 0; --k ) {
      if ( file_path[k] == *delim ) {
        tempo_string = file_path.substr( k + 1, file_path.length() - k );
        break;
      }
    }
    tempo = stof( tempo_string );

    tuple<vector<float>, float> song_entry = make_tuple( midi_timestamp, tempo );
    database.push_back( song_entry );
  }

  return database;
}

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

float compute_error_rate( float gt, float my )
{
  float error_rate1 = abs( my - gt );
  float error_rate2 = abs( my - 2 * gt );
  float error_rate3 = abs( my - 0.5 * gt );
  float error_rate4 = abs( my - 0.25 * gt );
  vector<float> errors;
  errors.push_back( error_rate1 );
  errors.push_back( error_rate2 );
  errors.push_back( error_rate3 );
  errors.push_back( error_rate4 );
  return vector_min( errors );
}

float compute_period_rule_based( Eigen::Matrix<float, 1, input_size> timestamps )
{

  float smallest_interval = 100.0;
  for ( size_t i = 1; i < input_size; i++ ) {
    if ( timestamps( 0, i ) < smallest_interval ) {
      smallest_interval = timestamps( 0, i );
    }
  }

  float average_interval = 0.0;
  size_t count = 0;

  for ( size_t i = 1; i < input_size; i++ ) {
    for ( int x = 1; x < 6; x++ ) {
      if ( abs( timestamps( 0, i ) / (float)x - smallest_interval ) / smallest_interval < 0.1 ) {
        average_interval += timestamps( 0, i ) / x;
        count++;
      }
    }
  }
  average_interval /= count;

  return average_interval;
}

float compute_phase( float first_timestamp, float period )
{
  float sixteenth_note_interval = period / 4;
  while ( first_timestamp > sixteenth_note_interval ) {
    first_timestamp -= sixteenth_note_interval;
  }
  return ( first_timestamp / sixteenth_note_interval ) * 2 * M_PI;
}

void program_body( const string& filename, const string& midi_test_database_path )
{
  ios::sync_with_stdio( false );

  auto mynetwork_ptr = make_unique<DNN_period>();
  DNN_period& nn = *mynetwork_ptr;

  // parse DNN_period from file
  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }

  // read all songs for testing
  auto test_songs_database = load_all_songs( midi_test_database_path );

  // pick a song for test now
  int num_notes = 0;

  vector<float> all_timestamps;
  int test_piece_index;
  float tempo;
  float period;
  while ( num_notes < input_size ) {
    test_piece_index = test_piece_distribution( prng );
    all_timestamps = get<0>( test_songs_database[test_piece_index] );
    num_notes = all_timestamps.size();
  }
  tempo = get<1>( test_songs_database[test_piece_index] );
  period = 60.0 / tempo;

  // proceed from the start of the song per millisecond
  float start_time = all_timestamps[input_size + 1];
  float end_time = all_timestamps.back();

  // start testing (per millisecond)
  float current_time = start_time;
  while ( current_time <= end_time ) {

    // network inference
    NetworkInference<DNN_period, batch_size> infer;

    // get the input timestamps based on current time
    Eigen::Matrix<float, 1, input_size> timestamps;
    vector<float> timestamps_raw;
    for ( int k = all_timestamps.size() - 1; k >= 0; --k ) {
      if ( all_timestamps[k] <= current_time ) {
        timestamps_raw.push_back( current_time - all_timestamps[k] );
      }
      if ( timestamps_raw.size() == input_size + 1 ) { // change to 16 later
        // my_timestamps(0,0) = timestamps[0];
        // for (int i = 1; i < 16; ++i) {
        for ( int i = 0; i < input_size; ++i ) {
          float noise = noise_distribution( prng );
          // noise = 0;
          timestamps( 0, i ) = timestamps_raw[i + 1] - timestamps_raw[i] + noise;
          // my_timestamps(0,i) = timestamps[i+1] - timestamps[i] + noise;
        }
        break;
      }
    }

    // predict period
    infer.apply( nn, timestamps );
    auto prediction = infer.output();
    float period_predicted = prediction( 0, 0 );
    float period_rule_based = compute_period_rule_based( timestamps );

    // compute phase manually
    float phase = compute_phase( timestamps_raw[0], period );
    float phase_predicted = compute_phase( timestamps_raw[0], period_predicted );
    float phase_rule_based = compute_phase( timestamps_raw[0], period_rule_based );

    // log
    cout << "current time = " << current_time << "\n";
    cout << "timestamps = (" << timestamps << ")"
         << "\n";
    cout << "GT period = " << period * 0.25 << " or " << period * 0.5 << " or " << period << " or " << period * 2
         << "\n";
    cout << "DL period = " << period_predicted << "\n";
    cout << "RB period = " << period_rule_based << "\n";
    cout << "GT phase = " << phase * 4 / M_PI << " pi or " << phase * 2 / M_PI << " pi or " << phase / M_PI
         << " pi or " << phase * 0.5 / M_PI << " pi\n";
    cout << "DL phase = " << phase_predicted / M_PI << " pi\n";
    cout << "RB phase = " << phase_rule_based / M_PI << " pi\n";
    cout << "\n";

    // update current_time
    current_time += 0.01;
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " [midi_test_database] [dnn_filename]\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[2], argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
