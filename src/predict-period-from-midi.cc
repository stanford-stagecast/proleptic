#define _USE_MATH_DEFINES

#include "backprop.hh"
#include "dnn_types.hh"
#include "exception.hh"
#include "inference.hh"
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
static constexpr int input_size = 16; // 64;

static auto tmp_distribution = uniform_real_distribution<float>( 1, 10 );
static auto train_piece_distribution = uniform_int_distribution<unsigned>( 0, 39 );
static auto val_piece_distribution = uniform_int_distribution<unsigned>( 0, 4 );

// Training parameters
static constexpr int number_of_iterations = 2000000;
static constexpr float learning_rate = 0.01;
static constexpr int batch_size = 1;

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
  // close the file
  in.close();
  return timestamps;
}

Eigen::Matrix<float, 1, input_size> pick_timestamp_segment( vector<float> midi_timestamps )
{
  Eigen::Matrix<float, 1, input_size> my_timestamps;
  float exit_flag = false;

  float noise = noise_distribution( prng );

  // first timestamp without applying noise
  // this is used to compute the ground truth for phase
  // float first_ts;

  while ( exit_flag == false ) {
    vector<float> timestamps;
    // choose a random time-point as the current time
    float current_time = static_cast<float>( rand() )
                         / ( static_cast<float>( RAND_MAX ) / midi_timestamps[midi_timestamps.size() - 1] );
    for ( int k = midi_timestamps.size() - 1; k >= 0; --k ) {
      if ( midi_timestamps[k] <= current_time ) {
        timestamps.push_back( current_time - midi_timestamps[k] );
      }
      // use deltas instead of raw timestamps as input
      if ( timestamps.size() == input_size + 1 ) {
        exit_flag = true;
        // my_timestamps( 0, 0 ) = timestamps[0];
        for ( int i = 0; i < input_size; ++i ) {
          noise = noise_distribution( prng );
          // noise = 0;
          my_timestamps( 0, i ) = timestamps[i + 1] - timestamps[i] + noise;
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
    string file_path = dirEntry.path().string();
    vector<float> midi_timestamp = read_timestamps_from_midi( file_path );

    // parse tempo from filename
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

float compute_period_error( float gt, float my )
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

float compute_phase_error( float gt, float my )
{
  // float error_rate1 = abs( my - gt );
  // float error_rate2 = abs( my - 2 * gt );
  // float error_rate3 = abs( my - 0.5 * gt );
  // float error_rate4 = abs( my - 0.25 * gt );
  // vector<float> errors;
  // errors.push_back( error_rate1 );
  // errors.push_back( error_rate2 );
  // errors.push_back( error_rate3 );
  // errors.push_back( error_rate4 );
  // return vector_min( errors );
  return abs( gt - my );
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

void program_body( ostream& output, const string& midi_train_database_path, const string& midi_val_database_path )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  DNN_period_16 nn;
  RandomState rng;
  randomize_network( nn, rng );

  // initialize the training struct
  NetworkTraining<DNN_period_16, batch_size> trainer;

  // read all songs from the training database folder
  auto train_songs_database = load_all_songs( midi_train_database_path );

  // variables needed for reading timestamps and tempos
  vector<float> midi_timestamps;
  int piece_index;
  float tempo;

  // read all songs from the validation database folder
  auto val_songs_database = load_all_songs( midi_val_database_path );

  // form a simple, fixed validation set for early stopping
  const int num_val_timestamps = 100;
  vector<tuple<Eigen::Matrix<float, 1, input_size>, float>> val_timestamps;
  for ( int k = 0; k < num_val_timestamps; ++k ) {
    while ( 1 ) {
      piece_index = val_piece_distribution( prng );
      midi_timestamps = get<0>( val_songs_database[piece_index] );
      if ( midi_timestamps.size() > input_size )
        break;
    }
    tempo = get<1>( val_songs_database[piece_index] );
    auto timestamps = pick_timestamp_segment( midi_timestamps );
    val_timestamps.push_back( make_tuple( timestamps, tempo ) );
  }

  // record current time for profiling purpose
  time_t current_time = time( NULL );

  // validation error of last 5000 iterations
  // float last_val_error = 10000.0;

  Eigen::Matrix<float, 1, input_size> val_current_timestamps;
  float val_tempo;
  float val_period;
  // float val_phase;
  float training_error = 0.0;

  for ( int iter_index = 0; iter_index < number_of_iterations; ++iter_index ) {

    // if (number_of_iterations == 1000000)
    //   learning_rate *= 0.5;

    // choose one of the pieces from the midi database for training
    while ( 1 ) {
      piece_index = train_piece_distribution( prng );
      midi_timestamps = get<0>( train_songs_database[piece_index] );
      if ( midi_timestamps.size() > input_size )
        break;
    }
    tempo = get<1>( train_songs_database[piece_index] );

    // obtain timestamps, period, and phase
    auto period = 60.0 / tempo;
    auto timestamps = pick_timestamp_segment( midi_timestamps );
    // auto phase = (timestamps(0,0) / period) * 2 * M_PI;
    // auto phase = compute_phase(timestamps(0,0), period);

    Eigen::Matrix<float, batch_size, 2> expected;
    expected( 0, 0 ) = period;
    // expected( 0, 0 ) = phase;
    // expected( 0, 1 ) = phase;

    // train the network using gradient descent
    NetworkInference<DNN_period_16, batch_size> infer;
    infer.apply( nn, timestamps );
    auto predicted_before_update = infer.output();

    // compute training error
    training_error = compute_period_error( period, predicted_before_update( 0, 0 ) );
    // training_error = compute_phase_error( phase, predicted_before_update( 0, 0 ) );

    trainer.train(
      nn,
      timestamps,
      [&expected]( const auto& prediction ) {
        Eigen::Matrix<float, 1, 1> pd;
        // loss function for period
        float period_loss_param = 5.0;

        float diff1 = period_loss_param * ( prediction( 0, 0 ) - expected( 0, 0 ) );
        float diff2 = period_loss_param * ( prediction( 0, 0 ) - 2 * expected( 0, 0 ) );
        float diff3 = period_loss_param * ( prediction( 0, 0 ) - 0.5 * expected( 0, 0 ) );
        float diff4 = period_loss_param * ( prediction( 0, 0 ) - 0.25 * expected( 0, 0 ) );
        vector<float> all_diffs;
        all_diffs.push_back( abs( diff1 ) );
        all_diffs.push_back( abs( diff2 ) );
        all_diffs.push_back( abs( diff3 ) );
        all_diffs.push_back( abs( diff4 ) );

        // ugly hard-coded, fix later!
        if ( vector_min( all_diffs ) == abs( diff1 ) ) {
          if ( abs( diff2 ) / abs( diff1 ) < 3.0 || abs( diff3 ) / abs( diff1 ) < 3.0 )
            pd( 0, 0 ) = 2 * diff1;
          else
            pd( 0, 0 ) = diff1;
        } else if ( vector_min( all_diffs ) == abs( diff2 ) ) {
          pd( 0, 0 ) = diff2;
        } else if ( vector_min( all_diffs ) == abs( diff3 ) ) {
          if ( abs( diff1 ) / abs( diff3 ) < 3.0 || abs( diff4 ) / abs( diff3 ) < 3.0 )
            pd( 0, 0 ) = 2 * diff3;
          else
            pd( 0, 0 ) = diff3;
        } else {
          pd( 0, 0 ) = diff4;
        }

        // loss function for phase
        // float phase_loss_param = 0.5;
        // // pd( 0, 1 ) = phase_loss_param * 2 * (cos(expected( 0, 1 )) * sin(prediction( 0, 1 )) - sin(expected(
        // 0, 1
        // // )) * cos(prediction( 0, 1 )));
        // pd( 0, 0 ) = phase_loss_param * 2 * (cos(expected( 0, 0 )) * sin(prediction( 0, 0 )) - sin(expected( 0, 0
        // )) * cos(prediction( 0, 0 )));
        return pd;
      },
      learning_rate );
    infer.apply( nn, timestamps );
    auto predicted_after_update = infer.output();

    if ( iter_index % 5000 == 0 ) {

      // do validation and early stopping (if necessary)
      float val_error = 0.0;
      float val_rule_based_error = 0.0;
      for ( auto val_element : val_timestamps ) {
        val_current_timestamps = get<0>( val_element );
        val_tempo = get<1>( val_element );
        val_period = 60.0 / val_tempo;
        // val_phase = compute_phase(val_current_timestamps(0,0), val_period);

        // get prediction
        // infer.apply( nn, val_current_timestamps );
        infer.apply( nn, val_current_timestamps );

        auto predicted_val = infer.output()( 0, 0 );
        auto period_rule_based = compute_period_rule_based( val_current_timestamps );
        // auto phase_rule_based = compute_phase(val_current_timestamps(0,0), period_rule_based);
        float error = compute_period_error( val_period, predicted_val );
        float rule_based_error = compute_period_error( val_period, period_rule_based );
        // float error = compute_phase_error( val_phase, predicted_val );
        // float rule_based_error = compute_phase_error( val_phase, phase_rule_based );
        val_error += error;
        val_rule_based_error += rule_based_error;
      }
      val_error /= val_timestamps.size();
      val_rule_based_error /= val_timestamps.size();

      // print useful info
      cout << "iteration " << iter_index << "\n";
      cout << "timestamps = (" << timestamps << ")"
           << "\n";
      cout << "ground truth period = " << expected( 0, 0 ) * 0.25 << " or " << expected( 0, 0 ) * 0.5 << " or "
           << expected( 0, 0 ) << " or " << expected( 0, 0 ) * 2 << "\n";
      cout << "predicted period (before update) = " << predicted_before_update( 0, 0 ) << "\n";
      cout << "predicted period (after update) = " << predicted_after_update( 0, 0 ) << "\n";
      cout << "training error = " << training_error << "\n";
      cout << "validation error = " << val_error << "\n";
      cout << "validation error (rule-based) = " << val_rule_based_error << "\n";
      // cout << "ground truth phase = " << expected( 0, 0 ) / M_PI << " pi\n";
      // cout << "predicted phase (before update) = " << predicted_before_update( 0, 0 ) / M_PI << " pi\n";
      // cout << "predicted phase (after update) = " << predicted_after_update( 0, 0 ) / M_PI << " pi\n";
      // cout << "training error = " << training_error / M_PI << " pi\n";
      // cout << "validation error = " << val_error / M_PI << " pi\n";
      // cout << "validation error (rule-based) = " << val_rule_based_error / M_PI << " pi\n";

      // check how val error goes before saving the model
      // if (iter_index > 20000 && val_error > last_val_error) {
      //   // if val error has decreased, stop now
      //   cout << "val error is increasing... early stopping...";
      //   break;
      // }
      // else {
      //   // otherwise, update last_val_error
      //   last_val_error = val_error;
      //   // save the model
      //   string serialized_nn;
      //   {
      //     serialized_nn.resize( 4000000 );
      //     Serializer serializer( string_span::from_view( serialized_nn ) );
      //     serialize( nn, serializer );
      //     serialized_nn.resize( serializer.bytes_written() );
      //     cout << "Output is " << serializer.bytes_written() << " bytes." << endl;
      //   }
      //   output << serialized_nn;
      // }

      cout << "\n";
    }
  }

  cout << "Time taken: " << time( NULL ) - current_time << " (s)\n";

  string serialized_nn;
  {
    serialized_nn.resize( 4000000 );
    Serializer serializer( string_span::from_view( serialized_nn ) );
    serialize( nn, serializer );
    serialized_nn.resize( serializer.bytes_written() );
    cout << "Output is " << serializer.bytes_written() << " bytes." << endl;
  }
  output << serialized_nn;
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc < 1 || argc > 4 ) {
    cerr << "Usage: " << argv[0] << " [midi_train_database] [midi_val_database] [dnn_filename]\n";
    return EXIT_FAILURE;
  }

  string filename;
  if ( argc == 3 ) {
    filename = "/dev/null";
  } else if ( argc == 4 ) {
    filename = argv[3];
  }

  ofstream output;
  output.open( filename );

  if ( !output.is_open() ) {
    cerr << "Unable to open file '" << filename << "' for writing." << endl;
    return EXIT_FAILURE;
  }

  try {
    program_body( output, argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
