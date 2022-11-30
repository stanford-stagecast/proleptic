#include "backprop.hh"
#include "dnn_types.hh"
#include "exception.hh"
#include "inference.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"

#include "time.h"
#include "training.hh"

#include <fstream>
#include <iostream>

using namespace std;

// Input generation parameters
static constexpr float note_timing_variation = 0.05;
static auto prng = get_random_engine();
static auto tempo_distribution = uniform_real_distribution<float>( 30, 240 );
static auto noise_distribution = normal_distribution<float>( 0, note_timing_variation );
static auto offset_distribution = uniform_real_distribution<float>( 0, 1 );
static constexpr int input_size = 16;

// Training parameters
static constexpr int number_of_iterations = 200000;
static constexpr float learning_rate = 0.0001;
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

Eigen::Matrix<float, batch_size, input_size> generate_input_notes( float tempo )
{
  Eigen::Matrix<float, batch_size, input_size> input_notes;
  const float seconds_per_beat = 60.0 / tempo;
  const float offset = offset_distribution( prng ) * seconds_per_beat;
  for ( auto batch = 0; batch < batch_size; batch++ ) {
    for ( auto i = 0; i < input_size; i++ ) {
      const float noise = noise_distribution( prng );
      input_notes( batch, i ) = seconds_per_beat * i * ( 1 + noise ) + offset;
    }
  }
  return input_notes;
}

void program_body( ostream& output )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  DNN_timestamp nn;
  RandomState rng;
  randomize_network( nn, rng );

  // initialize the training struct
  NetworkTraining<DNN_timestamp, batch_size> trainer;

  time_t current_time = time( NULL );

  for ( int iter_index = 0; iter_index < number_of_iterations; ++iter_index ) {
    cout << "iteration " << iter_index << "\n";
    const float tempo = tempo_distribution( prng );

    auto timestamps = generate_input_notes( tempo );
    cout << "timestamps = (" << timestamps << ")"
         << "\n";

    const float next_timestamp = timestamps( 0, 0 ) - 60.0 / tempo;
    Eigen::Matrix<float, batch_size, 2> expected;
    expected( 0, 0 ) = tempo;
    expected( 0, 1 ) = next_timestamp;

    // train the network using gradient descent
    NetworkInference<DNN_timestamp, batch_size> infer;
    infer.apply( nn, timestamps );
    auto predicted_before_update = infer.output();
    trainer.train(
      nn,
      timestamps,
      [&expected]( const auto& prediction ) {
        Eigen::Matrix<float, 1, 2> pd;
        pd( 0, 0 ) = 0.1 * ( prediction( 0, 0 ) - expected( 0, 0 ) );
        pd( 0, 1 ) = 4 * ( prediction( 0, 1 ) - expected( 0, 1 ) );
        return pd;
      },
      learning_rate );
    infer.apply( nn, timestamps );
    auto predicted_after_update = infer.output();

    cout << "ground truth tempo = " << tempo << "\n";
    cout << "predicted tempo (before update) = " << predicted_before_update( 0, 0 ) << "\n";
    float percent_diff_before = ( predicted_before_update( 0, 0 ) - tempo ) / tempo;
    cout << "percent diff (before update) = " << percent_diff_before * 100 << "%\n";
    cout << "predicted tempo (after update) = " << predicted_after_update( 0, 0 ) << "\n";
    float percent_diff_after = ( predicted_after_update( 0, 0 ) - tempo ) / tempo;
    cout << "percent diff (after update) = " << percent_diff_after * 100 << "%\n";

    cout << "ground truth timestamp = " << next_timestamp << "\n";
    cout << "predicted timestamp (before update) = " << predicted_before_update( 0, 1 ) << "\n";
    cout << "predicted timestamp (after update) = " << predicted_after_update( 0, 1 ) << "\n";

    cout << "\n";
  }

  cout << "Time taken: " << time( NULL ) - current_time << " (s)\n";

  string serialized_nn;
  {
    serialized_nn.resize( 1000000 );
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

  if ( argc < 1 || argc > 2 ) {
    cerr << "Usage: " << argv[0] << " [filename]\n";
    return EXIT_FAILURE;
  }

  string filename;
  if ( argc == 1 ) {
    filename = "/dev/null";
  } else if ( argc == 2 ) {
    filename = argv[1];
  }

  ofstream output;
  output.open( filename );

  if ( !output.is_open() ) {
    cerr << "Unable to open file '" << filename << "' for writing." << endl;
    return EXIT_FAILURE;
  }

  try {
    program_body( output );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
