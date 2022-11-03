#include "backprop.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "midi_file.hh"
#include "randomize_network.hh"
#include "serdes.hh"
#include "training.hh"

#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static vector<MidiFile> midi_files {};

// Training parameters
static constexpr float TARGET_ACCURACY = 0.95;
static constexpr float LEARNING_RATE = 0.1;
static constexpr size_t AVERAGE_WINDOW = 1000;

// Types
#define HISTORY 64
#define BATCH 1
using MyDNN = DNN_piano_roll_compressor;
using Infer = NetworkInference<MyDNN, BATCH>;
using Training = NetworkTraining<MyDNN, BATCH>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;

void generate_datum( Input& input, Output& output, string& name )
{
  for ( size_t row = 0; row < BATCH; row++ ) {
    const auto& midi = midi_files[uniform_int_distribution<size_t>( 0, midi_files.size() - 1 )( prng )];
    name = midi.name;
    const auto roll = midi.piano_roll();
    const size_t index = uniform_int_distribution<size_t>( 0, roll.size() - 1 )( prng );
    for ( size_t i = 21; i <= 108; i++ ) {
      input( row, i - 21 ) = roll[index][i];
      output( row, i - 21 ) = roll[index][i];
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

  deque<bool> successes;
  deque<bool> zeroes;
  size_t iteration = 0;
  do {
    auto infer = make_unique<Infer>();
    string name;
    generate_datum( input, expected, name );
    float baseline = 1 - ( expected.sum() / (float)expected.cols() );

    if ( baseline == 1.0 and ( rand() % 16 != 0 ) ) {
      continue;
    }

    infer->apply( nn, input );
    Output& output = infer->output();

    Output mapped = output.unaryExpr( []( double x ) { return (double)( x >= 0.5 ); } );
    successes.push_back( mapped == expected );
    zeroes.push_back( expected.sum() == 0 );
    if ( successes.size() > AVERAGE_WINDOW ) {
      successes.pop_front();
      zeroes.pop_front();
    }

    float learning_rate = train->train_with_backoff(
      nn, input, [&expected]( const auto& predicted ) { return predicted - expected; }, LEARNING_RATE );

    if ( expected.sum() != 0 ) {
      cout << "Iteration " << iteration << "\n";
      cout << "File: " << name << "\n";
      cout << "Norm: " << output.norm() << "\n";
      cout << "Learning Rate: " << learning_rate << "\n";
      cout << "Threads: " << Eigen::nbThreads() << "\n";
      cout << "Input: " << input.sum() << "\n";
      cout << "Output: " << output.unaryExpr( []( const auto x ) { return x >= 0.5; } ).sum() << "\n";
      cout << "Baseline: " << accumulate( zeroes.begin(), zeroes.end(), 0.f ) / (float)zeroes.size() << "\n";
      cout << "Accuracy: " << accumulate( successes.begin(), successes.end(), 0.f ) / (float)successes.size()
           << "\n";
      cout << endl;
    }
    iteration++;
  } while ( successes.size() < AVERAGE_WINDOW
            or accumulate( successes.begin(), successes.end(), 0.f ) / (float)successes.size() < TARGET_ACCURACY );

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
