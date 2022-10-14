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
#include "training.hh"

#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static auto pattern_length_distribution = uniform_int_distribution<unsigned>( 1, 16 );
static auto reverse_distribution = binomial_distribution<bool>( 1, 0.5 );
static auto note_distribution = uniform_int_distribution<unsigned>( 0, PIANO_ROLL_NUMBER_OF_NOTES - 1 );

// Training parameters
static constexpr int number_of_iterations = 50000;
static constexpr float learning_rate = 0.03;

// Types
using MyDNN = DNN_piano_roll_octave_prediction;
using Infer = NetworkInference<MyDNN, 1>;
using Training = NetworkTraining<MyDNN, 1>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;
using InputMatrix = Eigen::Matrix<MyDNN::type, PIANO_ROLL_NUMBER_OF_NOTES, PIANO_ROLL_HISTORY_WINDOW_LENGTH>;
using OutputMatrix = Eigen::Matrix<MyDNN::type, PIANO_ROLL_NUMBER_OF_NOTES, 1>;

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

vector<pair<int, int>> generate_pattern( void )
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
  cout << "Generating E(" << pattern_pulse_count << ", " << pattern_length << ")"
       << "\n";
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

  vector<pair<int, int>> data;
  for ( const auto& space : spacing ) {
    data.push_back( make_pair( space, note_distribution( prng ) ) );
  }
  return data;
}

template<class OutT, class InT>
OutT reshape( const InT& input )
{
  OutT output;
  assert( input.rows() * input.cols() == output.rows() * output.cols() );

  ssize_t inr = 0;
  ssize_t inc = 0;
  ssize_t outr = 0;
  ssize_t outc = 0;

  while ( inr < input.rows() and inc < input.cols() ) {
    output( outr, outc ) = input( inr, inc );
    inc++;
    if ( inc >= input.cols() ) {
      inc = 0;
      inr++;
    }
    outc++;
    if ( outc >= output.cols() ) {
      outc = 0;
      outr++;
    }
  }

  return output;
}

Input generate_input_notes( Output& true_output )
{
  InputMatrix input_notes;
  OutputMatrix output_notes;
  auto spacing = generate_pattern();

  auto rotation_distribution = uniform_int_distribution<int>( 0, spacing.size() - 1 );
  const int rotation = rotation_distribution( prng );
  auto begin_offset_distribution = uniform_int_distribution<int>( -input_notes.cols() / 2, input_notes.cols() / 2 );
  const unsigned begin_offset = max<int>( 0, begin_offset_distribution( prng ) );

  array<pair<int, int>, PIANO_ROLL_HISTORY_WINDOW_LENGTH> timeslots {};
  int timeslot_index = 0;
  int spacing_index = rotation;

  int n = input_notes.cols() + output_notes.cols();

  while ( timeslot_index < n ) {
    int beats = spacing[spacing_index].first;
    for ( int i = 0; i < beats; i++ ) {
      timeslots[timeslot_index++] = make_pair( 0, 0 );
      if ( timeslot_index > n )
        break;
    }
    timeslots[timeslot_index++] = make_pair( 1, spacing[spacing_index].second );
    spacing_index = ( spacing_index + 1 ) % spacing.size();
  }

  for ( ssize_t i = 0; i < input_notes.cols(); i++ ) {
    for ( ssize_t j = 0; j < input_notes.rows(); j++ ) {
      input_notes( j, i ) = i < begin_offset ? 0.5 : 0;
    }
    if ( i >= begin_offset ) {
      input_notes( timeslots[i].second, i ) = timeslots[i].first;
    }
  }

  for ( ssize_t i = 0; i < output_notes.cols(); i++ ) {
    for ( ssize_t j = 0; j < output_notes.rows(); j++ ) {
      output_notes( j, i ) = 0;
    }
    output_notes( timeslots[i].second, i ) = timeslots[i].first;
  }

  true_output = reshape<Output, OutputMatrix>( output_notes );
  return reshape<Input, InputMatrix>( input_notes );
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
      cout << "timestamps = (" << reshape<InputMatrix, Input>( timestamps ) << " )"
           << "\n";
      cout << "ground truth output = \t\t\t" << expected << "\n";
      cout << "prediction =  \t\t\t\t" << prediction << "\n";
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
           << "): " << (float)count_correct / (float)count_tested * 100.f << "%"
           << "\n";
      cout << "\n";
    }

    cout << endl;

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
    cout << "Output is " << serializer.bytes_written() << " bytes."
         << "\n";
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
    cerr << "Unable to open file '" << filename << "' for writing."
         << "\n";
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
