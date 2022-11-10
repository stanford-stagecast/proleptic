#include "backprop.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "midi_file.hh"
#include "randomize_network.hh"
#include "serdes.hh"
#include "training.hh"
#include "visualizer.hh"

#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static vector<MidiFile> midi_files {};

// Types
#define HISTORY 64
#define BATCH 1
using MyDNN = DNN_piano_roll_compressor;
using Infer = NetworkInference<MyDNN, BATCH>;
using Training = NetworkTraining<MyDNN, BATCH>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;
using Visualizer = TrainingVisualizer<MyDNN, BATCH>;

auto sigmoid = []( const double x ) { return 1.0 / ( 1.0 + exp( -x ) ); };

auto one_minus_x = []( const double x ) { return 1.0 - x; };

auto d_sigmoid_wrt_x = []( const double x ) {
  float sx = sigmoid( x );
  return sx * ( 1.0 - sx );
};

auto ln = []( const double x ) { return log( x ); };

double loss( const Output& expected, const Output& predicted )
{
  Output oneMinusExpected = expected.unaryExpr( one_minus_x );
  Output oneMinusPredicted = expected.unaryExpr( one_minus_x );
  Output logPredicted = predicted.unaryExpr( ln );
  Output logOneMinusPredicted = oneMinusPredicted.unaryExpr( ln );
  return -( expected.cwiseProduct( logPredicted ) + oneMinusExpected.cwiseProduct( logOneMinusPredicted ) ).sum();
}

Output pd_loss( const Output& expected, const Output& predicted )
{
  Output sigmoid_predicted = predicted.unaryExpr( sigmoid );

  Output pd = ( -expected.cwiseQuotient( sigmoid_predicted )
                + expected.unaryExpr( one_minus_x ).cwiseQuotient( sigmoid_predicted.unaryExpr( one_minus_x ) ) )
                .cwiseProduct( predicted.unaryExpr( d_sigmoid_wrt_x ) );

  return pd;
}

void generate_datum( Input& input, Output& output, string& name )
{
  for ( size_t row = 0; row < BATCH; row++ ) {
    const auto& midi = midi_files[uniform_int_distribution<size_t>( 0, midi_files.size() - 1 )( prng )];
    name = midi.name;
    const auto roll = midi.piano_roll();
    const size_t index = uniform_int_distribution<size_t>( 0, roll.size() - 1 )( prng );
    for ( size_t i = 21; i <= 108; i++ ) {
      input( row, i - 21 ) = roll[index][i] > 0.5;
      output( row, i - 21 ) = roll[index][i] > 0.5;
    }
  }
};

void program_body( ostream& outstream )
{
  (void)outstream;
  ios::sync_with_stdio( false );

  // initialize a randomized network
  const auto nn_ptr = make_unique<MyDNN>();
  MyDNN& nn = *nn_ptr;
  RandomState rng;
  randomize_network( nn, rng );

  auto input_ptr = make_unique<Input>();
  Input& input = *input_ptr;
  auto expected_ptr = make_unique<Output>();
  Output& expected = *expected_ptr;
  auto train = make_unique<Training>();

  Visualizer viz( loss, pd_loss );
  do {
    auto infer = make_unique<Infer>();
    string name;
    generate_datum( input, expected, name );
    viz.train( input, expected );
  } while ( true );

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
  (void)argv;
  if ( argc < 0 ) {
    abort();
  }

  if ( Eigen::nbThreads() > 1 ) {
    Eigen::setNbThreads( Eigen::nbThreads() / 2 );
  }

  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << "[midi directory] [dnn filename]\n";
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

  string filename = argv[2];

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
