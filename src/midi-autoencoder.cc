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
static constexpr float TARGET_ACCURACY = 0.99;
static constexpr float LEARNING_RATE = 0.01;
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
      input( row, i - 21 ) = roll[index][i] > 0.5;
      output( row, i - 21 ) = roll[index][i] > 0.5;
    }
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

  AccuracyMeasurement<AVERAGE_WINDOW> successes;
  AccuracyMeasurement<AVERAGE_WINDOW> differences;
  AccuracyMeasurement<AVERAGE_WINDOW> zeroes;
  AccuracyMeasurement<AVERAGE_WINDOW> accuracies;
  AccuracyMeasurement<AVERAGE_WINDOW> precisions;
  AccuracyMeasurement<AVERAGE_WINDOW> recalls;
  AccuracyMeasurement<AVERAGE_WINDOW> f_scores;
  AccuracyMeasurement<AVERAGE_WINDOW> selectivities;
  AccuracyMeasurement<AVERAGE_WINDOW> balanced;
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

    Output mapped = output.unaryExpr( []( double x ) { return (double)( x > 0.5 ); } );
    size_t true_positive = 0;
    size_t false_positive = 0;
    size_t true_negative = 0;
    size_t false_negative = 0;
    for ( size_t i = 0; i < 88; i++ ) {
      true_positive += ( expected( i ) and mapped( i ) );
      false_positive += ( not expected( i ) and mapped( i ) );
      true_negative += ( not expected( i ) and not mapped( i ) );
      false_negative += ( expected( i ) and not mapped( i ) );
    }
    successes.push( mapped == expected );
    differences.push( mapped.sum() - expected.sum() );
    zeroes.push( expected.sum() == 0 );
    accuracies.push( ( true_positive + true_negative ) / 88.f );

    bool precision_valid = ( true_positive + false_positive ) > 0;
    float precision = true_positive / (float)( true_positive + false_positive );
    bool recall_valid = ( true_positive + false_negative ) > 0;
    float recall = true_positive / (float)( true_positive + false_negative );
    bool f_score_valid = precision_valid and recall_valid and ( precision + recall ) > 0;
    float f_score = 2.f * ( precision * recall ) / ( precision + recall );

    bool selectivity_valid = ( true_negative + false_positive ) > 0;
    float selectivity = true_negative / (float)( true_negative + false_positive );

    if ( precision_valid )
      precisions.push( precision );

    if ( recall_valid )
      recalls.push( recall );

    if ( f_score_valid )
      f_scores.push( f_score );

    if ( selectivity_valid )
      selectivities.push( selectivity );

    if ( recall_valid and selectivity_valid )
      balanced.push( .5f * ( recall + selectivity ) );

    float learning_rate = train->train_with_backoff(
      nn,
      input,
      [&]( const auto& predicted ) {
        auto sigmoid = []( const auto x ) { return 1.0 / ( 1.0 + exp( -x ) ); };
        auto one_minus_x = []( const auto x ) { return 1.0 - x; };
        auto sigmoid_prime = [&]( const auto x ) { return sigmoid( x ) * ( 1.0 - sigmoid( x ) ); };

        Output sigmoid_predicted = predicted.unaryExpr( sigmoid );

        Output pd
          = ( -expected.cwiseQuotient( sigmoid_predicted )
              + expected.unaryExpr( one_minus_x ).cwiseQuotient( sigmoid_predicted.unaryExpr( one_minus_x ) ) )
              .cwiseProduct( predicted.unaryExpr( sigmoid_prime ) );

        return pd;
      },
      LEARNING_RATE );

    if ( false and mapped != expected ) {
      cout << "Expected: " << expected << "\n";
      cout << "Got:      " << mapped << "\n";
      cout << endl;
    }

    static std::chrono::time_point last_update_time
      = std::chrono::steady_clock::now() - std::chrono::milliseconds( 100 );
    if ( std::chrono::steady_clock::now() - last_update_time > std::chrono::milliseconds( 10 ) ) {
      cout << "\033[2J\033[H";
      cout << "Iteration " << iteration << "\n";
      cout << "File: " << name << "\n";
      cout << "Norm: " << output.norm() << "\n";
      cout << "Learning Rate: " << learning_rate << "\n";
      cout << "Threads: " << Eigen::nbThreads() << "\n";
      cout << "Difference: " << differences.mean() << "\n";
      cout << "Baseline: " << zeroes.mean() << "\n";
      cout << "\n";
      cout << "Exact: " << successes.mean() << "\n";
      cout << "Accuracy: " << accuracies.mean() << "\n";
      cout << "Balanced: " << balanced.mean() << "\n";
      cout << "\n";
      cout << "F Score: " << f_scores.mean() << "\n";
      cout << "Precision: " << precisions.mean() << "\n";
      cout << "Recall: " << recalls.mean() << "\n";
      cout << "Selectivity: " << selectivities.mean() << "\n";
      cout << endl;
      last_update_time = std::chrono::steady_clock::now();
    }
    iteration++;
  } while ( not f_scores.ready() or f_scores.mean() < TARGET_ACCURACY );

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
