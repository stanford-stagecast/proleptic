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
static constexpr float TARGET_ACCURACY = 0.80;
static constexpr float LEARNING_RATE = 0.01;
static constexpr size_t ACCURACY_MEASUREMENT_PERIOD = 1000;

// Types
#define HISTORY PIANO_ROLL_HISTORY
#define BATCH 1
using MyDNN = DNN_piano_roll_compressed_prediction;
using Infer = NetworkInference<MyDNN, BATCH>;
using Training = NetworkTraining<MyDNN, BATCH>;
using Output = typename Infer::Output;
using Input = typename Infer::Input;

using Single = Eigen::Matrix<double, 88, 1>;

using Compressor = DNN_piano_roll_compressor;
using Codec = Autoencoder<double, 88, PIANO_ROLL_COMPRESSED_SIZE>;
using Encoder = typename Codec::Encoder;
using Decoder = typename Codec::Decoder;
using EncoderInfer = NetworkInference<Encoder, 1>;
using DecoderInfer = NetworkInference<Decoder, BATCH>;

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
    if ( accuracy > 1.0 or accuracy < 0.0 ) {
      throw runtime_error( "invalid accuracy" );
    }
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

void program_body( string infilename, ostream& outstream )
{
  (void)outstream;
  auto compressor_ptr = make_unique<Compressor>();
  Compressor& compressor = *compressor_ptr;

  // parse DNN_timestamp from file
  {
    ReadOnlyFile dnn_on_disk { infilename };
    Parser parser { dnn_on_disk };
    parser.object( compressor );
  }
  const Codec codec( compressor );
  const Encoder& enc = codec.encoder();
  const Decoder& dec = codec.decoder();
  (void)dec;
  auto e_infer = make_unique<EncoderInfer>();
  auto d_infer = make_unique<DecoderInfer>();

  ios::sync_with_stdio( false );

  // initialize a randomized network
  const auto nn_ptr = make_unique<MyDNN>();
  MyDNN& nn = *nn_ptr;
  RandomState rng;
  randomize_network( nn, rng );

  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> accuracies;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> compressed_accuracies;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> f_scores;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> precisions;
  AccuracyMeasurement<ACCURACY_MEASUREMENT_PERIOD> recalls;
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
  do {
    string name;

    bool trivial = true;
    for ( size_t batch = 0; batch < BATCH; batch++ ) {
      generate_datum( raw_input, raw_expected, name );
      for ( size_t i = 0; i < HISTORY; i++ ) {
        EncoderInfer::Output compressed_single;
        Single raw_single = raw_input[i];
        e_infer->apply( enc, raw_single );
        compressed_single = e_infer->output();
        size_t index = i;
        for ( size_t j = 0; j < PIANO_ROLL_COMPRESSED_SIZE; j++ ) {
          input( batch, index ) = compressed_single[j];
          index += HISTORY;
        }
      }
      if ( raw_expected != raw_input.back() ) {
        trivial = false;
      }
      EncoderInfer::Output compressed_single;
      Single raw_single = raw_expected;
      e_infer->apply( enc, raw_single );
      compressed_single = e_infer->output();
      expected.row( batch ) = compressed_single;
    }

    auto infer = make_unique<Infer>();
    infer->apply( nn, input );

    Output& output = infer->output();

    DecoderInfer::Output raw_output;
    d_infer->apply( dec, output );
    raw_output = d_infer->output();

    size_t num_correct = 0;
    size_t num_tested = 0;
    size_t num_1_correct = 0;
    size_t num_1_tested = 0;
    size_t num_0_correct = 0;
    size_t num_0_tested = 0;
    for ( ssize_t i = 0; i < raw_output.size(); i++ ) {
      if ( raw_expected( i ) < 0.5 ) {
        if ( raw_output( i ) < 0.5 ) {
          num_0_correct++;
          num_correct++;
        }
        num_0_tested++;
        num_tested++;
      } else if ( raw_expected( i ) >= 0.5 ) {
        if ( raw_output( i ) >= 0.5 ) {
          num_1_correct++;
          num_correct++;
        }
        num_1_tested++;
        num_tested++;
      }
    }
    for ( ssize_t i = 0; i < output.size(); i++ ) {
      num_tested++;
    }
    float f_score;
    float precision;
    float recall;
    bool precision_valid = true;
    bool recall_valid = true;
    bool f_score_valid = true;
    {
      size_t true_positive = num_1_correct;
      size_t false_positive = num_1_tested - num_1_correct;
      size_t false_negative = num_0_tested - num_0_correct;
      if ( true_positive + false_positive == 0 ) {
        precision = 1.0; // no irrelevant 1s
        precision_valid = false;
      } else {
        precision = true_positive / (float)( true_positive + false_positive );
      }
      if ( true_positive + false_negative == 0 ) {
        recall = 1.0; // no missing 1s
        recall_valid = false;
      } else {
        recall = true_positive / (float)( true_positive + false_negative );
      }
      /* constexpr float beta = 1; */
      f_score_valid = precision_valid and recall_valid;
      if ( precision + recall == 0.0 ) {
        f_score = 0.f;
        f_score_valid = false;
      } else {
        f_score = 2.f * ( precision * recall ) / ( precision + recall );
      }
    }
    float compressed_accuracy = 1.f - acos( output.normalized().dot( expected.normalized() ) ) / ( (float)M_PI );
    float accuracy = num_correct / (float)num_tested;

    if ( not trivial ) {
      accuracies.push( accuracy );
      if ( f_score_valid )
        f_scores.push( f_score );
      compressed_accuracies.push( compressed_accuracy );
      if ( precision_valid )
        precisions.push( precision );
      if ( recall_valid )
        recalls.push( recall );
    }

    float learning_rate = train->train_with_backoff(
      nn, input, [&expected]( const auto& predicted ) { return predicted - expected; }, LEARNING_RATE );

    num_tested++;

    static std::chrono::time_point last_update_time
      = std::chrono::steady_clock::now() - std::chrono::milliseconds( 100 );
    if ( std::chrono::steady_clock::now() - last_update_time > std::chrono::milliseconds( 10 ) ) {
      cout << "\033[2J\033[H";
      cout << "Iteration " << ( iteration + 1 ) << "\n";
      cout << "File: " << name << "\n";
      cout << "Norm: " << output.norm() << "\n";
      cout << "Learning Rate: " << learning_rate << endl;
      cout << "Threads: " << Eigen::nbThreads() << endl;
      cout << "Current accuracy: " << accuracy << "\n";
      cout << "Rolling accuracy: " << accuracies.mean() << "\n";
      cout << "Compressed: " << compressed_accuracies.mean() << "\n";
      cout << "F Scores: " << f_scores.mean() << "\n";
      cout << "Precision: " << precisions.mean() << "\n";
      cout << "Recall: " << recalls.mean() << "\n";
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

  if ( argc != 4 ) {
    cerr << "Usage: " << argv[0] << "[compressor dnn] [midi directory] [dnn filename]\n";
    return EXIT_FAILURE;
  }

  string compressor = argv[1];

  string midi_directory = argv[2];

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

  string filename = argv[3];

  ofstream output;
  output.open( filename );

  if ( !output.is_open() ) {
    cerr << "Unable to open file '" << filename << "' for writing."
         << "\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( compressor, output );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
