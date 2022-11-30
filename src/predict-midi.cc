#include "autoencoder.hh"
#include "backprop.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "midi_file.hh"
#include "piano_roll.hh"
#include "randomize_network.hh"
#include "serdes.hh"
#include "training.hh"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace std;

// Input generation parameters
static auto prng = get_random_engine();
static vector<MidiFile> midi_files {};

// Training parameters
static constexpr float TARGET_ACCURACY = 0.99;
static constexpr float LEARNING_RATE = 0.01;
static constexpr size_t ACCURACY_MEASUREMENT_PERIOD = 1000;

// Types
#define HISTORY PIANO_ROLL_HISTORY
#define BATCH 1
using MyDNN = DNN_piano_roll_prediction;
using Infer = NetworkInference<MyDNN, BATCH>;
using Training = NetworkTraining<MyDNN, BATCH>;
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

void program_body( ostream& outstream )
{
  (void)outstream;

  ios::sync_with_stdio( false );

  // initialize a randomized network
  const auto nn_ptr = make_unique<MyDNN>();
  MyDNN& nn = *nn_ptr;
  RandomState rng;
  randomize_network( nn, rng );

  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> accuracies;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> f_scores;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> precisions;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> recalls;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> selectivities;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> durations;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> ratio;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> expected_ratio;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> balanced;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> pds;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> complexity;
  auto input_ptr = make_unique<Input>();
  Input& input = *input_ptr;
  auto expected_ptr = make_unique<Output>();
  Output& expected = *expected_ptr;

  auto raw_input_ptr = make_unique<array<Single, HISTORY>>();
  auto raw_expected_ptr = make_unique<Single>();
  array<Single, HISTORY>& raw_input = *raw_input_ptr;
  Single& raw_expected = *raw_expected_ptr;

  auto train = make_unique<Training>();
  size_t iteration = 0;

  using namespace chrono;
  do {
    auto start = steady_clock::now();
    string name;

    bool trivial = true;
    for ( size_t batch = 0; batch < BATCH; batch++ ) {
      generate_datum( raw_input, raw_expected, name );
      for ( size_t i = 0; i < HISTORY; i++ ) {
        Single single = raw_input[HISTORY - i];
        size_t index = i;
        for ( size_t j = 0; j < 88; j++ ) {
          input( batch, index ) = single[j];
          index += HISTORY;
        }
      }
      if ( raw_expected != raw_input.back() ) {
        trivial = false;
      }
      expected.row( batch ) = raw_expected;
    }

    auto infer = make_unique<Infer>();
    infer->apply( nn, input );

    Output output = infer->output();

    if ( isnan( output.norm() ) ) {
      throw runtime_error( "network exploded" );
    }

    size_t true_positive = 0;
    size_t false_positive = 0;
    size_t true_negative = 0;
    size_t false_negative = 0;
    for ( size_t i = 0; i < 88; i++ ) {
      true_positive += ( expected( i ) and ( output( i ) > 0 ) );
      false_positive += ( not expected( i ) and ( output( i ) > 0 ) );
      true_negative += ( not expected( i ) and not( output( i ) > 0 ) );
      false_negative += ( expected( i ) and not( output( i ) > 0 ) );
    }
    bool precision_valid = ( true_positive + false_positive ) > 0;
    float precision = true_positive / (float)( true_positive + false_positive );
    bool recall_valid = ( true_positive + false_negative ) > 0;
    float recall = true_positive / (float)( true_positive + false_negative );
    bool f_score_valid = precision_valid and recall_valid and ( precision + recall ) > 0;
    float f_score = 2.f * ( precision * recall ) / ( precision + recall );

    bool selectivity_valid = ( true_negative + false_positive ) > 0;
    float selectivity = true_negative / (float)( true_negative + false_positive );

    float accuracy = ( true_positive + true_negative ) / 88.f;

    complexity.push( not trivial );
    if ( not trivial ) {
      accuracies.push( accuracy );

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

      ratio.push( ( true_positive + false_positive ) / 88.f );
      expected_ratio.push( ( true_positive + false_negative ) / 88.f );
    }

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

        pds.push( pd.norm() );
        return pd;
      },
      LEARNING_RATE );

    auto end = steady_clock::now();

    auto duration = end - start;
    durations.push( duration_cast<microseconds>( duration ).count() / 1000000.f );

    static std::chrono::time_point last_update_time
      = std::chrono::steady_clock::now() - std::chrono::milliseconds( 100 );
    if ( std::chrono::steady_clock::now() - last_update_time > std::chrono::milliseconds( 10 ) ) {
      cout << "\033[2J\033[H";
      cout << "Iteration " << ( iteration + 1 ) << "\n";
      cout << "File: " << name << "\n";
      cout << "Norm: " << output.norm() << "\n";
      cout << "Learning Rate: " << learning_rate << endl;
      cout << "Threads: " << Eigen::nbThreads() << endl;
      cout << "Current: " << output.norm() << endl;
      cout << "\n";
      cout << "F Scores: " << f_scores.mean() << "\n";
      cout << "Precision: " << precisions.mean() << "\n";
      cout << "Recall: " << recalls.mean() << "\n";
      cout << "Selectivity: " << selectivities.mean() << "\n";
      cout << "\n";
      cout << "Balanced: " << balanced.mean() << "\n";
      cout << "Ratio: " << ratio.mean() << "\n";
      cout << "Real Ratio: " << expected_ratio.mean() << "\n";
      cout << "Time: " << durations.mean() * 1000 << " milliseconds\n";
      cout << "PDs: " << pds.mean() << "\n";
      cout << "Complex: " << complexity.mean() << "\n";
      cout << flush;
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

  ofstream output;
  output.open( filename );

  if ( !output.is_open() ) {
    cerr << "Unable to open file '" << filename << "' for writing."
         << "\n";
    return EXIT_FAILURE;
  }

  while ( true ) {
    try {
      program_body( output );
      break;
    } catch ( const runtime_error& e ) {
      cerr << e.what() << "\n";
    } catch ( const exception& e ) {
      cerr << e.what() << "\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
