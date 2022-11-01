#include "cdf.hh"
#include "dnn_types.hh"
#include "exception.hh"
#include "inference.hh"
#include "mmap.hh"
#include "network.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"
#include "training.hh"

#include <array>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

using MyDNN = DNN_piano_roll_octave_prediction;
using Infer = NetworkInference<MyDNN, 1>;
using Input = typename Infer::Input;
using Output = typename Infer::Output;
using Training = NetworkTraining<MyDNN, 1>;
using InputMatrix
  = Eigen::Matrix<MyDNN::type, PIANO_ROLL_OCTAVE_NUMBER_OF_NOTES, PIANO_ROLL_OCTAVE_HISTORY_WINDOW_LENGTH>;
using OutputMatrix = Eigen::Matrix<MyDNN::type, PIANO_ROLL_OCTAVE_NUMBER_OF_NOTES, 1>;

const float LEARNING_RATE = 0.05;
const int REPETITIONS = 5;

enum Duration
{
  DURATION_SIXTEENTH = 1,
  DURATION_EIGHTH = 2,
  DURATION_QUARTER = 4,
  DURATION_DOTTED_QUARTER = 6,
  DURATION_HALF = 8,
  DURATION_WHOLE = 16,
};

enum Pitch
{
  PITCH_C = 0,
  PITCH_C_SHARP = 1,
  PITCH_D_FLAT = 1,
  PITCH_D = 2,
  PITCH_D_SHARP = 3,
  PITCH_E_FLAT = 3,
  PITCH_E = 4,
  PITCH_F = 5,
  PITCH_F_SHARP = 6,
  PITCH_G_FLAT = 6,
  PITCH_G = 7,
  PITCH_G_SHARP = 8,
  PITCH_A_FLAT = 8,
  PITCH_A = 9,
  PITCH_A_SHARP = 10,
  PITCH_B_FLAT = 10,
  PITCH_B = 11,
  PITCH_HIGH_C = 12,
};

using Note = pair<Pitch, Duration>;

#define whole( x ) make_pair( PITCH_##x, DURATION_WHOLE )
#define half( x ) make_pair( PITCH_##x, DURATION_HALF )
#define dotted_quarter( x ) make_pair( PITCH_##x, DURATION_DOTTED_QUARTER )
#define quarter( x ) make_pair( PITCH_##x, DURATION_QUARTER )
#define eighth( x ) make_pair( PITCH_##x, DURATION_EIGHTH )
#define sixteenth( x ) make_pair( PITCH_##x, DURATION_SIXTEENTH )

using Song = vector<Note>;

const Song HOT_CROSS_BUNS_LOW { quarter( E ),
                                quarter( D ),
                                half( C ),
                                quarter( E ),
                                quarter( D ),
                                half( C ),
                                eighth( C ),
                                eighth( C ),
                                eighth( C ),
                                eighth( C ),
                                eighth( D ),
                                eighth( D ),
                                eighth( D ),
                                eighth( D ),
                                quarter( E ),
                                quarter( D ),
                                half( C ) };

const Song HOT_CROSS_BUNS_HIGH { quarter( G ),
                                 quarter( F ),
                                 half( E ),
                                 quarter( G ),
                                 quarter( F ),
                                 half( E ),
                                 eighth( E ),
                                 eighth( E ),
                                 eighth( E ),
                                 eighth( E ),
                                 eighth( F ),
                                 eighth( F ),
                                 eighth( F ),
                                 eighth( F ),
                                 quarter( G ),
                                 quarter( F ),
                                 half( E ) };

const Song HOT_CROSS_BUNS_MIDDLE { quarter( F ),
                                   quarter( E ),
                                   half( D ),
                                   quarter( F ),
                                   quarter( E ),
                                   half( D ),
                                   eighth( D ),
                                   eighth( D ),
                                   eighth( D ),
                                   eighth( D ),
                                   eighth( E ),
                                   eighth( E ),
                                   eighth( E ),
                                   eighth( E ),
                                   quarter( F ),
                                   quarter( E ),
                                   half( D ) };

const Song TWINKLE_TWINKLE_LITTLE_STAR {
  quarter( E ), quarter( E ), quarter( G ), quarter( G ), quarter( A ), quarter( A ), half( G ),
  quarter( F ), quarter( F ), quarter( E ), quarter( E ), quarter( D ), quarter( D ), half( C ),
  quarter( G ), quarter( G ), quarter( F ), quarter( F ), quarter( E ), quarter( E ), half( D ),
  quarter( G ), quarter( G ), quarter( F ), quarter( F ), quarter( E ), quarter( E ), half( D ),
  quarter( E ), quarter( E ), quarter( G ), quarter( G ), quarter( A ), quarter( A ), half( G ),
  quarter( F ), quarter( F ), quarter( E ), quarter( E ), quarter( D ), quarter( D ), half( C ),
};

const Song HAPPY_BIRTHDAY_TO_YOU {
  eighth( C ),       eighth( C ),  quarter( D ), quarter( C ), quarter( F ), half( E ),   eighth( C ),
  eighth( C ),       quarter( D ), quarter( C ), quarter( G ), half( F ),    eighth( C ), eighth( C ),
  quarter( HIGH_C ), quarter( A ), quarter( F ), quarter( E ), quarter( D ), eighth( B ), eighth( B ),
  quarter( A ),      quarter( F ), quarter( G ), half( F ),
};

const Song ODE_TO_JOY {
  quarter( E ),        quarter( E ), quarter( F ), quarter( G ),        quarter( G ), quarter( F ),
  quarter( E ),        quarter( D ), quarter( C ), quarter( C ),        quarter( D ), quarter( E ),
  dotted_quarter( E ), eighth( D ),  half( D ),    quarter( E ),        quarter( E ), quarter( F ),
  quarter( G ),        quarter( G ), quarter( F ), quarter( E ),        quarter( D ), quarter( C ),
  quarter( C ),        quarter( D ), quarter( E ), dotted_quarter( D ), eighth( C ),  half( C ),
  quarter( D ),        quarter( D ), quarter( E ), quarter( C ),        quarter( D ), eighth( E ),
  eighth( F ),         quarter( E ), quarter( C ), quarter( D ),        eighth( E ),  eighth( F ),
  quarter( E ),        quarter( D ), quarter( C ), quarter( C ),        quarter( D ), quarter( G ),
  quarter( E ),        quarter( E ), quarter( F ), quarter( G ),        quarter( G ), quarter( F ),
  quarter( E ),        quarter( D ), quarter( C ), quarter( C ),        quarter( D ), quarter( E ),
  dotted_quarter( D ), eighth( C ),  half( C ) };

using SongData = pair<string, Song>;

const vector<SongData> SONGS {
  make_pair( "Hot Cross Buns Middle", HOT_CROSS_BUNS_MIDDLE ),
  make_pair( "Hot Cross Buns High", HOT_CROSS_BUNS_HIGH ),
  make_pair( "Hot Cross Buns Low", HOT_CROSS_BUNS_LOW ),
  make_pair( "Twinkle Twinkle Little Star", TWINKLE_TWINKLE_LITTLE_STAR ),
  make_pair( "Happy Birthday to You", HAPPY_BIRTHDAY_TO_YOU ),
  make_pair( "Ode to Joy", ODE_TO_JOY ),
};

using Timeslot = array<bool, PIANO_ROLL_OCTAVE_NUMBER_OF_NOTES>;

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

int train_on_next_timeslot( MyDNN& network, const Timeslot& next )
{
  static deque<Timeslot> timeslots;
  while ( timeslots.size() < PIANO_ROLL_OCTAVE_HISTORY_WINDOW_LENGTH ) {
    Timeslot empty {};
    fill( empty.begin(), empty.end(), 0.5 );
    timeslots.push_front( empty );
  }
  InputMatrix input_matrix;
  for ( ssize_t slot = 0; slot < input_matrix.cols(); slot++ ) {
    Timeslot current = timeslots[slot];
    for ( ssize_t key = 0; key < input_matrix.rows(); key++ ) {
      input_matrix( key, slot ) = current[key];
    }
  }
  Input input = reshape<Input, InputMatrix>( input_matrix );
  Output output;
  for ( ssize_t key = 0; key < input_matrix.rows(); key++ ) {
    output( key ) = next[key];
  }
  Infer infer;

  infer.apply( network, input );
  Output predicted = infer.output();

  Training train;
  train.train(
    network, input, [&output]( const auto& prediction ) { return prediction - output; }, LEARNING_RATE );

  int errors = 0;
  for ( ssize_t i = 0; i < output.rows(); i++ ) {
    int e = output( i );
    int p = predicted( i ) >= 0.5 ? 1 : 0;
    errors += e != p;
  }

  timeslots.push_back( next );
  timeslots.pop_front();
  return errors;
}

float train_on_song( MyDNN& network, Song song )
{
  Timeslot current;
  int notes_correct = 0;
  int notes_total = 0;
  int timeslots_correct = 0;
  int timeslots_total = 0;
  for ( const Note& note : song ) {
    Pitch pitch = note.first;
    Duration duration = note.second;
    fill( current.begin(), current.end(), 0 );
    current[pitch] = true;
    int errors = 0;
    for ( int i = 0; i < duration; i++ ) {
      int new_errors = train_on_next_timeslot( network, current );
      errors += new_errors;
      if ( new_errors == 0 )
        timeslots_correct++;
      timeslots_total++;
    }
    if ( errors == 0 ) {
      notes_correct++;
    }
    notes_total++;
  }
  (void)notes_correct;
  (void)notes_total;
  (void)timeslots_correct;
  (void)timeslots_total;
  return notes_correct / (float)notes_total;
  /* return timeslots_correct / (float)timeslots_total; */
}

void program_body( const string& filename )
{
  ios::sync_with_stdio( false );

  auto mynetwork_ptr = make_unique<MyDNN>();
  MyDNN& mynetwork = *mynetwork_ptr;

  // parse DNN_timestamp from file
  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( mynetwork );
  }
  srand( time( 0 ) );

  for ( const auto& song : SONGS ) {
    cout << song.first << ":\n";
    for ( size_t i = 0; i < REPETITIONS; i++ ) {
      float accuracy = train_on_song( mynetwork, song.second );
      cout << "\t" << accuracy * 100 << "%\n";
    }
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " filename\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
