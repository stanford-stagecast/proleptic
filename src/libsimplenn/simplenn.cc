#include "simplenn.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "mmap.hh"
#include "serdes.hh"
#include "training.hh"

using namespace std;
using namespace chrono;

// This is a hack so the client code doesn't have to include any headers except "simplenn.hh" (especially
// since the actual type of "DNN" might change).
struct NetworkData
{
  DNN_timestamp network {};
};

SimpleNN::SimpleNN( const string& filename )
{
  data_ = make_unique<NetworkData>();
  DNN_timestamp& nn = data_->network;

  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }
}

float SimpleNN::predict_next_timestamp( const std::array<float, 16>& past_timestamps )
{
  using Infer = NetworkInference<DNN_timestamp, 1>;
  using Input = typename Infer::Input;
  Input input( past_timestamps.data() );
  Infer infer;
  infer.apply( data_->network, input );
  float timestamp = infer.output()( 1 );
  return timestamp;
}

SimpleNN::~SimpleNN() = default;

struct PianoRollPredictorData
{
  DNN_piano_roll_prediction network {};
};

PianoRollPredictor::PianoRollPredictor( const string& filename )
{
  data_ = make_unique<PianoRollPredictorData>();
  DNN_piano_roll_prediction& nn = data_->network;

  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }
}

PianoRollPredictor::~PianoRollPredictor() = default;

pair<steady_clock::time_point, steady_clock::duration> PianoRollPredictor::predict_next_note_time(
  const vector<steady_clock::time_point>& timestamps )
{
  const auto now = steady_clock::now();
  if ( timestamps.size() < 2 ) {
    return make_pair( now, milliseconds( 0 ) );
  }
  steady_clock::duration smallest_interval( std::numeric_limits<long>::max() );

  for ( size_t i = 1; i < min( timestamps.size(), (size_t)16 ); i++ ) {
    auto candidate = timestamps[i] - timestamps[i - 1];
    if ( candidate < smallest_interval ) {
      smallest_interval = candidate;
    }
  }

  steady_clock::duration average_interval { 0 };
  size_t count = 0;

  for ( size_t i = 1; i < min( timestamps.size(), (size_t)16 ); i++ ) {
    auto candidate = timestamps[i] - timestamps[i - 1];
    for ( int x = 1; x < 6; x++ ) {
      if ( abs( candidate.count() / (float)x - smallest_interval.count() ) / (float)smallest_interval.count()
           < 0.1 ) {
        average_interval += candidate / x;
        count++;
      }
    }
  }
  average_interval /= count;

  steady_clock::time_point next_change = timestamps.back();

  while ( next_change < now ) {
    next_change += average_interval;
  }

  return make_pair( next_change, average_interval );
}

bool PianoRollPredictor::predict_next_note_value( const vector<bool>& notes )
{
  using Infer = NetworkInference<DNN_piano_roll_prediction, 1>;
  using Input = typename Infer::Input;

  if ( notes.size() == 0 ) {
    return false;
  }

  Input input = Input::Constant( 0 );
  ssize_t offset = input.cols() - notes.size();
  for ( ssize_t i = 0; i < input.cols(); i++ ) {
    if ( i < offset ) {
      input( i ) = 0.5;
    } else {
      input( i ) = notes[i - offset];
    }
  }

  Infer infer;
  infer.apply( data_->network, input );
  return infer.output()( 0 ) > 0.5;
}

void PianoRollPredictor::train_next_note_value( const vector<bool>& notes, bool next )
{
  using Infer = NetworkInference<DNN_piano_roll_prediction, 1>;
  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  using Train = NetworkTraining<DNN_piano_roll_prediction, 1>;

  Input input = Input::Constant( 0 );
  ssize_t offset = input.cols() - notes.size();
  for ( ssize_t i = 0; i < input.cols(); i++ ) {
    if ( i < offset ) {
      input( i ) = 0.5;
    } else {
      input( i ) = notes[i - offset];
    }
  }

  Train train;

  train.train(
    data_->network, input, [next]( const auto& predicted ) { return predicted - Output::Constant( next ); }, 0.01 );
}
