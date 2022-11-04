#pragma once

#include "autoencoder.hh"
#include "dnn_types.hh"
#include "mmap.hh"
#include "piano_roll.hh"
#include "serdes.hh"
#include "training.hh"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

struct PeriodPredictorData;

class PeriodPredictor
{
public:
  PeriodPredictor( const std::string& filename );
  ~PeriodPredictor();

  std::pair<std::chrono::steady_clock::time_point, std::chrono::steady_clock::duration> predict_next_note_time(
    const std::vector<std::chrono::steady_clock::time_point>& timestamps );

  float predict_period( const std::array<float, 16>& past_timestamps );

private:
  std::unique_ptr<PeriodPredictorData> data_ {};
};

class SimpleNN
{
private:
  using Predictor = DNN_piano_roll_prediction;
  std::unique_ptr<Predictor> predictor_;

public:
  using Key = uint8_t;
  using Time = std::chrono::steady_clock::time_point;
  using Duration = std::chrono::steady_clock::duration;
  struct KeyPress
  {
    Key key;
    Time time;

    KeyPress( Key k, Time t )
      : key( k )
      , time( t )
    {}
  };
  struct Timeslot
  {
    Time start;
    Duration length;

    Timeslot( Time s, Duration l )
      : start( s )
      , length( l )
    {}
  };
  using Column = std::array<bool, 88>;
  using Roll = std::vector<Column>;
  static constexpr size_t HISTORY = PIANO_ROLL_HISTORY;

  SimpleNN( const std::string& predictor_file )
    : predictor_( std::make_unique<Predictor>() )
  {
    {
      ReadOnlyFile dnn_on_disk { predictor_file };
      Parser parser { dnn_on_disk };
      parser.object( *predictor_ );
    }
  }
  ~SimpleNN() = default;

  Timeslot predict_next_timeslot( const std::vector<KeyPress>& timestamps )
  {
    using namespace std;
    using namespace chrono;
    const auto now = steady_clock::now();
    if ( timestamps.size() < 2 ) {
      return Timeslot( now, milliseconds( 0 ) );
    }
    Duration smallest_interval( std::numeric_limits<long>::max() );

    for ( size_t i = 1; i < min( timestamps.size(), (size_t)16 ); i++ ) {
      auto candidate = timestamps[i].time - timestamps[i - 1].time;
      bool different_keys = timestamps[i].key == timestamps[i - 1].key;
      if ( different_keys and candidate < milliseconds( 100 ) )
        continue;
      if ( candidate < smallest_interval ) {
        smallest_interval = candidate;
      }
    }

    Duration average_interval { 0 };
    size_t count = 0;

    for ( size_t i = 1; i < min( timestamps.size(), (size_t)16 ); i++ ) {
      auto candidate = timestamps[i].time - timestamps[i - 1].time;
      for ( int x = 1; x < 6; x++ ) {
        if ( abs( candidate.count() / (float)x - smallest_interval.count() ) / (float)smallest_interval.count()
             < 0.1 ) {
          average_interval += candidate / x;
          count++;
        }
      }
    }
    average_interval /= count;

    Time next_change = timestamps.back().time;

    while ( next_change < now ) {
      next_change += average_interval;
    }

    return Timeslot( next_change, average_interval );
  }

  Column predict_next_note_values( const Roll& roll )
  {
    using namespace std;
    using Infer = NetworkInference<Predictor, 1>;

    using Input = typename Infer::Input;
    using Output = typename Infer::Output;

    auto unknown = make_unique<Output>();
    for ( ssize_t i = 0; i < unknown->size(); i++ ) {
      ( *unknown )( i ) = PianoRollEvent::Unknown;
    }
    auto inputs = make_unique<std::vector<Output>>();

    if ( roll.size() < HISTORY )
      for ( size_t i = 0; i < HISTORY - roll.size(); i++ ) {
        inputs->push_back( *unknown );
      }

    for ( const auto& column : roll ) {
      Output current;
      for ( size_t i = 0; i < column.size(); i++ ) {
        current( i ) = column[i];
      }
      inputs->push_back( current );
    }

    auto combined_input = make_unique<Input>();
    for ( size_t column = 0; column < inputs->size(); column++ ) {
      for ( size_t row = 0; row < 88; row++ ) {
        ( *combined_input )( row * HISTORY + column ) = ( *inputs )[column]( row );
      }
    }

    auto infer = make_unique<Infer>();
    infer->apply( *predictor_, *combined_input );

    auto output = infer->output();

    Column column;
    for ( size_t i = 0; i < column.size(); i++ ) {
      column[i] = output( i ) > 0.5;
    }

    return column;
  }

  void train_next_note_values( const std::vector<Column>& roll, const Column& next )
  {
    using namespace std;
    using Infer = NetworkInference<Predictor, 1>;
    using Train = NetworkTraining<Predictor, 1>;

    using Input = typename Infer::Input;
    using Output = typename Infer::Output;

    auto unknown = make_unique<Output>();
    for ( ssize_t i = 0; i < unknown->size(); i++ ) {
      ( *unknown )( i ) = PianoRollEvent::Unknown;
    }
    auto inputs = make_unique<std::vector<Output>>();

    if ( roll.size() < HISTORY )
      for ( size_t i = 0; i < HISTORY - roll.size(); i++ ) {
        inputs->push_back( *unknown );
      }

    for ( const auto& column : roll ) {
      Output current;
      for ( size_t i = 0; i < column.size(); i++ ) {
        current( i ) = column[i];
      }
      inputs->push_back( current );
    }

    auto combined_input = make_unique<Input>();
    for ( size_t column = 0; column < inputs->size(); column++ ) {
      for ( size_t row = 0; row < 88; row++ ) {
        ( *combined_input )( row * HISTORY + column ) = ( *inputs )[column]( row );
      }
    }

    Output expected;
    for ( size_t i = 0; i < next.size(); i++ ) {
      expected( i ) = next[i];
    }

    auto train = make_unique<Train>();
    train->train(
      *predictor_,
      *combined_input,
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
      0.01 );
  }
};

struct OctavePredictorData;

class OctavePredictor
{
public:
  using Key = uint8_t;
  using Time = std::chrono::steady_clock::time_point;
  using Duration = std::chrono::steady_clock::duration;
  struct KeyPress
  {
    Key key;
    Time time;

    KeyPress( Key k, Time t )
      : key( k )
      , time( t )
    {}
  };
  struct Timeslot
  {
    Time start;
    Duration length;

    Timeslot( Time s, Duration l )
      : start( s )
      , length( l )
    {}
  };
  using NoteValuesInTimeslot = std::array<bool, 12>;

  OctavePredictor( const std::string& filename );
  ~OctavePredictor();

  Timeslot predict_next_timeslot( const std::vector<KeyPress>& timestamps );

  NoteValuesInTimeslot predict_next_note_values( const std::vector<NoteValuesInTimeslot>& notes );
  void train_next_note_values( const std::vector<NoteValuesInTimeslot>& notes, const NoteValuesInTimeslot& next );

private:
  std::unique_ptr<OctavePredictorData> data_ {};
};
