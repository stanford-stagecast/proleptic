/**
 * This predictor takes a different approach than the others:
 * - assume we can extract tempo and offset from a series of input notes
 * - using the tempo and offset, we can convert input notes into a "piano roll" (for now, a sequence of booleans
 * representing beats)
 * - we can treat predicition as a classification problem; is the next timeslot on the roll going to be on or off?
 * - since this is a classification problem, our network is much more stable, so we can train much faster (several
 * orders of magnitude faster)
 * - we can predict multiple future notes in parallel, which may give backprop more to work off of during live
 * training
 *
 * This approach seems to provide much higher accuracy than the previous one of training directly on timestamps; we
 * get to 100% accuracy within 20,000 iterations of training.  However accuracy starts to drop if the pattern length
 * exceeds half our input length; i.e., if the network can't see at least two full repetitions of the pattern.  When
 * we have patterns of length 16 (exactly the same as our input size), the accuracy drops to around 75% after
 * 100,000 iterations.
 */
#include "backprop.hh"
#include "dnn_types.hh"
#include "exception.hh"
#include "inference.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"
#include "training.hh"

#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static auto pattern_length_distribution = uniform_int_distribution<unsigned>( 1, 16 );
static auto reverse_distribution = binomial_distribution<bool>( 1, 0.5 );

// Training parameters
static constexpr int number_of_iterations = 20000;
static constexpr float learning_rate = 0.01;

// Types
using MyDNN = DNN_piano_roll_rhythm_prediction;
using Infer = NetworkInference<MyDNN, 1>;
using Training = NetworkTraining<MyDNN, 1>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;

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
  cout << "Generating E(" << pattern_pulse_count << ", " << pattern_length << ")" << endl;
  ;

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

Input generate_input_notes( Output& true_output )
{
  Input input_notes;
  auto spacing = generate_pattern();

  auto rotation_distribution = uniform_int_distribution<int>( 0, spacing.size() - 1 );
  const int rotation = rotation_distribution( prng );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  array<int, 64> timeslots {};
  int index = 0;
  int j = rotation;

  int n = input_notes.cols() + true_output.cols();
  assert( n <= 64 );

  while ( index < n ) {
    int beats = spacing[j];
    for ( int i = 0; i < beats; i++ ) {
      assert( index < 64 );
      timeslots[index++] = 0;
      if ( index >= n )
        goto exit_loop;
    }
    assert( index < 64 );
    timeslots[index++] = 1;
    j = ( j + 1 ) % spacing.size();
  }
exit_loop:

  for ( int i = 0; i < input_notes.cols(); i++ ) {
    assert( i < 64 );
    input_notes( i ) = timeslots[i];
  }

  for ( int i = 0; i < true_output.cols(); i++ ) {
    assert( i + input_notes.cols() < 64 );
    true_output( i ) = timeslots[i + input_notes.cols()];
  }

#pragma GCC diagnostic pop
  return input_notes;
}

void program_body( ostream& outstream )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  MyDNN nn;
  RandomState rng;
  randomize_network( nn, rng );

  // initialize the training struct
  Training trainer;

  int count_correct = 0;
  int count_tested = 0;
  for ( int iter_index = 0; iter_index < number_of_iterations; ++iter_index ) {

    Output expected;

    auto timestamps = generate_input_notes( expected );

    // train the network using gradient descent
    Infer infer;
    infer.apply( nn, timestamps );
    auto predicted_before_update = infer.output();
    trainer.train(
      nn, timestamps, [&expected]( const auto& prediction ) { return prediction - expected; }, learning_rate );
    infer.apply( nn, timestamps );
    auto predicted_after_update = infer.output();

    {
      Output prediction = predicted_after_update.unaryExpr( []( auto x ) { return x < 0.5 ? 0.f : 1.f; } );
      cout << "iteration " << iter_index << "\n";
      cout << "timestamps = (" << timestamps << " )"
           << "\n";
      cout << "ground truth output = \t\t\t" << expected << "\n";
      cout << "prediction =  \t\t\t\t" << prediction << endl;
      cout << "predicted output (before update) = \t" << predicted_before_update << "\n";
      cout << "predicted output (after update) = \t" << predicted_after_update << "\n";

      const float T = 0.5;
      auto matching = [=]( auto x, auto y ) { return ( ( x < T ) && ( y < T ) ) || ( ( x > T ) && y > T ); };
      int matches = 0;
      for ( int i = 0; i < expected.cols(); i++ ) {
        matches += matching( predicted_before_update( i ), expected( i ) );
      }
      if ( matches == expected.cols() ) {
        count_correct++;
      }

      count_tested++;
      cout << "Current accuracy (since iteration " << iter_index - count_tested
           << "): " << (float)count_correct / (float)count_tested * 100.f << "%" << endl;
      cout << endl;
    }

    if ( iter_index % 1000 == 0 ) {
      count_correct = 0;
      count_tested = 0;
    }
  }

  string serialized_nn;
  {
    serialized_nn.resize( 100000000 );
    Serializer serializer( string_span::from_view( serialized_nn ) );
    serialize( nn, serializer );
    serialized_nn.resize( serializer.bytes_written() );
    cout << "Output is " << serializer.bytes_written() << " bytes." << endl;
  }
  outstream << serialized_nn;
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
