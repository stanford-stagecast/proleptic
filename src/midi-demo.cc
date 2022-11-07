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
static constexpr float LEARNING_RATE = 0.05;

// Types
#define HISTORY PIANO_ROLL_HISTORY
#define BATCH 1
using MyDNN = DNN_piano_roll_prediction;
using Infer = NetworkInference<MyDNN, BATCH>;
using Train = NetworkTraining<MyDNN, BATCH>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;

using Single = Eigen::Matrix<double, 88, 1>;

void generate_datum( array<Single, HISTORY>& input, Single& output, string& name )
{
  const auto& midi = midi_files[uniform_int_distribution<size_t>( 0, midi_files.size() - 1 )( prng )];
  name = midi.name;
  const PianoRoll<128>& roll = midi.piano_roll();
  const ssize_t start_index = uniform_int_distribution<ssize_t>( 0, roll.size() - 1 )( prng ) - HISTORY;

  for ( size_t i = 0; i < HISTORY; i++ ) {
    for ( size_t key = 0; key < 88; key++ ) {
      if ( (ssize_t)i + start_index < 0 ) {
        input[i][key] = PianoRollEvent::Unknown;
      } else {
        input[i][key] = roll[i + start_index][key + 21];
      }
    }
  }
  for ( size_t key = 0; key < 88; key++ ) {
    output[key] = roll[start_index + HISTORY][key + 21];
  }
};

template<size_t N>
struct AccuracyMeasurement
{
  std::deque<float> accuracies {};

  void push( float accuracy )
  {
    accuracies.push_back( accuracy );
    if ( accuracies.size() > N ) {
      accuracies.pop_front();
    }
  }

  float mean() const
  {
    if ( accuracies.size() == 0.0 )
      return 0.0;
    return accumulate( accuracies.begin(), accuracies.end(), 0.f ) / accuracies.size();
  }

  size_t count() const { return accuracies.size(); }

  bool ready() const { return count() == N; }
};

void program_body( const string& filename )
{
  ios::sync_with_stdio( false );

  // initialize a randomized network
  const auto nn_ptr = make_unique<MyDNN>();
  MyDNN& nn = *nn_ptr;

  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }
  auto input_ptr = make_unique<Input>();
  Input& input = *input_ptr;
  auto expected_ptr = make_unique<Output>();
  Output& expected = *expected_ptr;

  auto infer_ptr = make_unique<Infer>();
  Infer& infer = *infer_ptr;

  auto train_ptr = make_unique<Train>();
  Train& train = *train_ptr;

  for ( const auto& file : midi_files ) {
    const auto& roll = file.piano_roll();
    cout << "MIDI File: " << file.name << "\n";

    const size_t columns = roll.size();
    cout << "Columns: " << columns << "\n";

    using namespace chrono;
    for ( size_t attempt = 0; attempt < 10; attempt++ ) {
      size_t correct = 0;
      size_t total = 0;

      duration total_time_inferring = nanoseconds( 0 );
      duration total_time_training = nanoseconds( 0 );

      for ( size_t i = PIANO_ROLL_HISTORY; i < columns; i++ ) {
        for ( size_t j = 0; j < PIANO_ROLL_HISTORY; j++ ) {
          for ( size_t k = 0; k < 88; k++ ) {
            input( k * PIANO_ROLL_HISTORY + j ) = roll[i - PIANO_ROLL_HISTORY + j][k];
          }
        }
        for ( size_t k = 0; k < 88; k++ ) {
          expected( k ) = roll[i][k];
        }

        time_point start = steady_clock::now();
        infer.apply( nn, input );
        time_point end = steady_clock::now();
        total_time_inferring += end - start;
        Output& output = infer.output();

        Output mapped = output.unaryExpr( []( const auto x ) { return x < 0.0 ? 0.0 : 1.0; } );

        if ( mapped == expected ) {
          correct++;
        }
        total++;

        /* cout << "\n"; */
        /* cout << mapped(Eigen::seq(30, 70)) << "\n"; */
        /* cout << expected(Eigen::seq(30, 70)) << "\n"; */

        start = steady_clock::now();
        float learning_rate = train.train_with_backoff(
          nn,
          input,
          [&]( const auto& predicted ) {
            static const auto sigmoid = []( const auto x ) { return 1.0 / ( 1.0 + exp( -x ) ); };
            static const auto one_minus_x = []( const auto x ) { return 1.0 - x; };
            static const auto sigmoid_prime = [&]( const auto x ) { return sigmoid( x ) * ( 1.0 - sigmoid( x ) ); };

            Output sigmoid_predicted = predicted.unaryExpr( sigmoid );

            Output pd
              = ( -expected.cwiseQuotient( sigmoid_predicted )
                  + expected.unaryExpr( one_minus_x ).cwiseQuotient( sigmoid_predicted.unaryExpr( one_minus_x ) ) )
                  .cwiseProduct( predicted.unaryExpr( sigmoid_prime ) );

            return pd;
          },
          LEARNING_RATE );
        end = steady_clock::now();
        total_time_training += end - start;
        (void)learning_rate;
        /* cout << learning_rate << "\n"; */
      }
      cout << correct << "/" << total << " (" << duration_cast<milliseconds>( total_time_inferring ).count()
           << "ms inferring; " << duration_cast<milliseconds>( total_time_training ).count() << "ms training)\n";
      cout << flush;
    }
    break;
  }
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

  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " [midi directory] [dnn filename]\n";
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
  try {
    program_body( filename );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
