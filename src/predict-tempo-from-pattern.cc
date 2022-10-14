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
#include <fstream>
#include <iostream>
#include <tuple>

using namespace std;

// Input generation parameters
static constexpr float note_timing_variation = 0.05;
static auto prng = get_random_engine();
static auto tempo_distribution = uniform_real_distribution<float>( 30, 240 );
static auto noise_distribution = normal_distribution<float>( 0, note_timing_variation );
static auto pattern_length_distribution = uniform_int_distribution<unsigned>( 2, 8 );
static auto reverse_distribution = binomial_distribution<bool>( 1, 0.5 );
static constexpr int input_size = 16;

static auto tmp_distribution = uniform_real_distribution<float>( 1, 10 );

// Training parameters
static constexpr int number_of_iterations = 200000;
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

int gcd( int a, int b )
{
  if ( a == 0 )
    return b;
  return gcd( b % a, a );
}

int findGCD( vector<int> pattern )
{
  int result = pattern.at( 0 ) + 1;
  for ( long unsigned int i = 1; i < pattern.size(); ++i ) {
    int p = pattern.at( i ) + 1;

    result = gcd( result, p );

    if ( result == 1 ) {
      return 1;
    }
  }
  return result;
}

vector<int> generate_pattern( void )
{
  // Using [Euclidean Rhythms](https://en.wikipedia.org/wiki/Euclidean_rhythm),
  // based on http://cgm.cs.mcgill.ca/~godfried/publications/banff.pdf.  This
  // can be calculated using Bresenham's line-drawing algorithm
  // (https://www.tandfonline.com/doi/full/10.1080/17459730902916545), which is
  // also based on the Euclidean GCD algorithm.
  //
  // The end result is that we want to space a fixed number of "pulses" as
  // close to equally as possible on a fixed field of "timeslots", where the
  // timeslots are integer times.  Bresenham's line-drawing algorithm does this
  // implicitly, by spacing the "rises" out over the "run".
  //
  // Returns a vector where each element represents the number of empty spaces
  // (rests) between two notes.
  const unsigned pattern_length = pattern_length_distribution( prng );
  auto pattern_pulse_count_distribution = uniform_int_distribution<unsigned>( 1, pattern_length );
  const unsigned pattern_pulse_count = pattern_pulse_count_distribution( prng );

  auto line_function = [&]( float x, float y ) {
    // in the form Ax + By + C = 0
    return pattern_pulse_count * x - pattern_length * y;
  };

  vector<int> spacing;
  int current_x = 0;
  int current_y = 0;
  int steps_since_last_rise = 0;
  while ( spacing.size() < pattern_pulse_count ) {
    // we could go to (x + 1, y) or (x + 1, y + 1), so we look at whether the
    // midpoint is above or below our target line
    float midpoint = line_function( current_x + 1, current_y + 0.5 );
    if ( midpoint > 0 ) {
      current_y++;
      spacing.push_back( steps_since_last_rise );
      steps_since_last_rise = 0;
    } else {
      steps_since_last_rise++;
    }
    current_x++;
  }

  if ( reverse_distribution( prng ) ) {
    reverse( spacing.begin(), spacing.end() );
  }
  return spacing;
}

auto generate_input_notes_from_pattern( vector<int> pattern, float tempo )
{
  Eigen::Matrix<float, 1, input_size + 1> input_notes;
  Eigen::Matrix<float, 1, input_size> diff;

  // tempo
  float seconds_per_beat = 60.0 / tempo;

  // offset
  auto offset_distribution = uniform_real_distribution<float>( 0, seconds_per_beat );
  const float offset = offset_distribution( prng ) * seconds_per_beat;

  int num_notes_generated = 1;
  int current_pattern_index = 0;
  float noise = noise_distribution( prng );
  input_notes( 0, 0 ) = offset;
  while ( num_notes_generated < 17 ) {
    int i = pattern[current_pattern_index] + 1;
    noise = noise_distribution( prng );
    input_notes( 0, num_notes_generated )
      = input_notes( 0, num_notes_generated - 1 ) + seconds_per_beat * i * ( 1 + noise );
    diff( 0, num_notes_generated - 1 )
      = input_notes( 0, num_notes_generated ) - input_notes( 0, num_notes_generated - 1 );
    num_notes_generated++;
    current_pattern_index = ( current_pattern_index + 1 ) % pattern.size();
  }

  tuple<Eigen::Matrix<float, batch_size, input_size>, float> result;
  result = make_tuple( diff, seconds_per_beat );
  return result;
}

// Eigen::Matrix<float, batch_size, input_size> generate_input_notes( float tempo )
auto generate_input_notes( float tempo )
{
  Eigen::Matrix<float, batch_size, input_size> input_notes;
  const float seconds_per_beat = 60.0 / tempo;

  auto offset_distribution = uniform_real_distribution<float>( 0, seconds_per_beat );

  const float offset = offset_distribution( prng ) * seconds_per_beat;
  for ( auto batch = 0; batch < batch_size; batch++ ) {
    for ( auto i = 0; i < input_size; i++ ) {
      const float noise = noise_distribution( prng );
      input_notes( batch, i ) = seconds_per_beat * i * ( 1 + noise ) + offset;
    }
  }

  tuple<Eigen::Matrix<float, batch_size, input_size>, float> result;
  result = make_tuple( input_notes, offset );
  return result;
}

void program_body( ostream& output )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  DNN_tempo nn;
  RandomState rng;
  randomize_network( nn, rng );

  // initialize the training struct
  NetworkTraining<DNN_tempo, batch_size> trainer;

  time_t current_time = time( NULL );

  for ( int iter_index = 0; iter_index < number_of_iterations; ++iter_index ) {
    cout << "iteration " << iter_index << "\n";
    const float tempo = tempo_distribution( prng );

    vector<int> pattern = generate_pattern();

    // QZ: we process the pattern first
    int my_gcd = findGCD( pattern );
    if ( my_gcd == 0 )
      cerr << "error!" << endl;
    for ( long unsigned int i = 0; i < pattern.size(); ++i ) {
      pattern.at( i ) = pattern.at( i ) / my_gcd;
    }

    auto generated_timestamps = generate_input_notes_from_pattern( pattern, tempo );
    auto timestamps = get<0>( generated_timestamps );
    auto spacing = get<1>( generated_timestamps );

    cout << "timestamps = (" << timestamps << ")"
         << "\n";

    Eigen::Matrix<float, batch_size, 1> expected;
    expected( 0, 0 ) = spacing;

    // train the network using gradient descent
    NetworkInference<DNN_tempo, batch_size> infer;
    infer.apply( nn, timestamps );
    auto predicted_before_update = infer.output();
    trainer.train(
      nn,
      timestamps,
      [&expected]( const auto& prediction ) {
        Eigen::Matrix<float, 1, 1> pd;
        pd( 0, 0 ) = 1.0 * ( prediction( 0, 0 ) - expected( 0, 0 ) );
        return pd;
      },
      learning_rate );
    infer.apply( nn, timestamps );
    auto predicted_after_update = infer.output();

    cout << "ground truth tempo = " << spacing << "\n";
    cout << "predicted tempo (before update) = " << predicted_before_update( 0, 0 ) << "\n";
    cout << "predicted tempo (after update) = " << predicted_after_update( 0, 0 ) << "\n";

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