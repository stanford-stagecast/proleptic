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
static constexpr int input_size = 64;

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

  while ( exit_flag == false ) {
    vector<float> timestamps;
    // choose a random time-point as the current time
    float current_time = static_cast<float>( rand() )
                         / ( static_cast<float>( RAND_MAX / midi_timestamps[midi_timestamps.size() - 1] ) );
    for ( int k = midi_timestamps.size() - 1; k >= 0; --k ) {
      if ( midi_timestamps[k] <= current_time ) {
        timestamps.push_back( current_time - midi_timestamps[k] );
      }
      // use deltas instead of raw timestamps as input
      if ( timestamps.size() == input_size ) {
        exit_flag = true;
        my_timestamps( 0, 0 ) = timestamps[0];
        for ( int i = 1; i < input_size; ++i ) {
          // noise = noise_distribution( prng );
          noise = 0;
          my_timestamps( 0, i ) = timestamps[i] - timestamps[i - 1] + noise;
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

float cap_range( float phase )
{
  float capped_phase = phase;
  while ( capped_phase < 0 || capped_phase > 2 * M_PI ) {
    if ( capped_phase < 0 ) {
      capped_phase += 2 * M_PI;
    } else {
      capped_phase -= 2 * M_PI;
    }
  }
  return capped_phase;
}

tuple<float, float> compute_error( float gt_period, float my_period, float gt_phase, float my_phase )
{
  float error_rate1 = abs( my_period - gt_period );
  float error_rate2 = abs( my_period - 2 * gt_period );
  float error_rate3 = abs( my_period - 0.5 * gt_period );
  float error_rate4 = abs( my_period - 0.25 * gt_period );
  vector<float> errors;
  errors.push_back( error_rate1 );
  errors.push_back( error_rate2 );
  errors.push_back( error_rate3 );
  errors.push_back( error_rate4 );

  if ( vector_min( errors ) == error_rate1 ) {
    return make_tuple( error_rate1,
                       min( cap_range( abs( my_phase - gt_phase ) ),
                            (float)( 2 * M_PI ) - cap_range( abs( my_phase - gt_phase ) ) ) );
  } else if ( vector_min( errors ) == error_rate2 ) {
    return make_tuple( error_rate2,
                       min( cap_range( abs( my_phase - 0.5 * gt_phase ) ),
                            (float)( 2 * M_PI ) - cap_range( abs( my_phase - 0.5 * gt_phase ) ) ) );
  } else if ( vector_min( errors ) == error_rate3 ) {
    return make_tuple( error_rate3,
                       min( cap_range( abs( my_phase - 2 * gt_phase ) ),
                            (float)( 2 * M_PI ) - cap_range( abs( my_phase - 2 * gt_phase ) ) ) );
  } else {
    return make_tuple( error_rate4,
                       min( cap_range( abs( my_phase - 4 * gt_phase ) ),
                            (float)( 2 * M_PI ) - cap_range( abs( my_phase - 4 * gt_phase ) ) ) );
  }
}

void program_body( ostream& output, const string& midi_train_database_path, const string& midi_val_database_path )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  DNN_period_phase nn;
  RandomState rng;
  randomize_network( nn, rng );

  // initialize the training struct
  NetworkTraining<DNN_period_phase, batch_size> trainer;

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
  float val_phase;
  float training_period_error = 0.0;
  float training_phase_error = 0.0;

  for ( int iter_index = 0; iter_index < number_of_iterations; ++iter_index ) {

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
    // auto period = tempo;
    auto timestamps = pick_timestamp_segment( midi_timestamps );
    auto phase = ( timestamps( 0, 0 ) / period ) * 2 * M_PI;

    Eigen::Matrix<float, batch_size, 2> expected;
    expected( 0, 0 ) = period;
    expected( 0, 1 ) = phase;

    // train the network using gradient descent
    NetworkInference<DNN_period_phase, batch_size> infer;
    infer.apply( nn, timestamps );
    auto predicted_before_update = infer.output();

    // compute training error
    tuple<float, float> training_error
      = compute_error( period, predicted_before_update( 0, 0 ), phase, predicted_before_update( 0, 1 ) );
    training_period_error = get<0>( training_error );
    training_phase_error = get<1>( training_error );

    trainer.train(
      nn,
      timestamps,
      [&expected]( const auto& prediction ) {
        Eigen::Matrix<float, 1, 2> pd;
        // need loss function for period and phase
        float period_loss_param = 1.0;
        float phase_loss_param = 0.1;

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
          pd( 0, 1 ) = phase_loss_param * 2
                       * ( cos( expected( 0, 1 ) ) * sin( prediction( 0, 1 ) )
                           - sin( expected( 0, 1 ) ) * cos( prediction( 0, 1 ) ) );
        } else if ( vector_min( all_diffs ) == abs( diff2 ) ) {
          pd( 0, 0 ) = diff2;
          pd( 0, 1 ) = phase_loss_param * 2
                       * ( cos( 0.5 * expected( 0, 1 ) ) * sin( prediction( 0, 1 ) )
                           - sin( 0.5 * expected( 0, 1 ) ) * cos( prediction( 0, 1 ) ) );
        } else if ( vector_min( all_diffs ) == abs( diff3 ) ) {
          if ( abs( diff1 ) / abs( diff3 ) < 3.0 || abs( diff4 ) / abs( diff3 ) < 3.0 )
            pd( 0, 0 ) = 2 * diff3;
          else
            pd( 0, 0 ) = diff3;
          pd( 0, 1 ) = phase_loss_param * 2
                       * ( cos( 2 * expected( 0, 1 ) ) * sin( prediction( 0, 1 ) )
                           - sin( 2 * expected( 0, 1 ) ) * cos( prediction( 0, 1 ) ) );
        } else {
          pd( 0, 0 ) = diff4;
          pd( 0, 1 ) = phase_loss_param * 2
                       * ( cos( 4 * expected( 0, 1 ) ) * sin( prediction( 0, 1 ) )
                           - sin( 4 * expected( 0, 1 ) ) * cos( prediction( 0, 1 ) ) );
        }

        return pd;
      },
      learning_rate );
    infer.apply( nn, timestamps );
    auto predicted_after_update = infer.output();

    if ( iter_index % 5000 == 0 ) {

      // do validation and early stopping (if necessary)
      float val_period_error = 0.0;
      float val_phase_error = 0.0;
      for ( auto val_element : val_timestamps ) {
        val_current_timestamps = get<0>( val_element );
        val_tempo = get<1>( val_element );
        val_period = 60.0 / val_tempo;
        val_phase = ( val_current_timestamps( 0, 0 ) / val_period ) * 2 * M_PI;
        // get prediction
        infer.apply( nn, val_current_timestamps );
        auto predicted_val = infer.output();
        tuple<float, float> validation_error
          = compute_error( val_period, predicted_val( 0, 0 ), val_phase, predicted_val( 0, 1 ) );
        float period_error = get<0>( validation_error );
        float phase_error = get<1>( validation_error );
        val_period_error += period_error;
        val_phase_error += phase_error;
      }
      val_period_error /= val_timestamps.size();
      val_phase_error /= val_timestamps.size();

      // print useful info
      cout << "iteration " << iter_index << "\n";
      cout << "timestamps = (" << timestamps << ")"
           << "\n";
      cout << "ground truth period = " << expected( 0, 0 ) * 0.25 << " or " << expected( 0, 0 ) * 0.5 << " or "
           << expected( 0, 0 ) << " or " << expected( 0, 0 ) * 2 << "\n";
      cout << "predicted period (before update) = " << predicted_before_update( 0, 0 ) << "\n";
      cout << "predicted period (after update) = " << predicted_after_update( 0, 0 ) << "\n";
      cout << "ground truth phase = " << expected( 0, 1 ) * 4 / M_PI << " pi or " << expected( 0, 1 ) * 2 / M_PI
           << " pi or " << expected( 0, 1 ) / M_PI << " pi or " << expected( 0, 1 ) * 0.5 / M_PI << " pi\n";
      cout << "predicted phase (before update) = " << predicted_before_update( 0, 1 ) / M_PI << " pi\n";
      cout << "predicted phase (after update) = " << predicted_after_update( 0, 1 ) / M_PI << " pi\n";
      cout << "training_period_error error = " << training_period_error << "\n";
      cout << "training_phase_error error = " << training_phase_error / M_PI << " pi\n";
      cout << "val_period_error error = " << val_period_error << "\n";
      cout << "val_phase_error error = " << val_phase_error / M_PI << " pi\n";

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
