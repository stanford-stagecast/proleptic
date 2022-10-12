#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "http_server.hh"
#include "inference.hh"
#include "mmap.hh"
#include "network.hh"
#include "nngraph.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"
#include "socket.hh"
#include "training.hh"

#include <iostream>

using namespace std;

using MyDNN = DNN_piano_roll_prediction;
using Infer = NetworkInference<MyDNN, 1>;
using Input = typename Infer::Input;
using Output = typename Infer::Output;
using Training = NetworkTraining<MyDNN, 1>;

const float LEARNING_RATE = 0.15;

// Assume one interval = one sixteenth note
const vector<int> HOT_CROSS_BUNS = { 4, 4, 8, 4, 4, 8, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 8 };
const int REPETITIONS = 10;

void print_notes( const vector<int>& notes )
{
  unsigned intervals = 0;
  for ( unsigned i = 0; i < notes.size(); i++ ) {
    int note = notes[i];
    intervals += note;
    switch ( note ) {
      case 1:
        cout << "𝅘𝅥𝅯 ";
        break;
      case 2:
        cout << "𝅘𝅥𝅮  ";
        break;
      case 4:
        cout << "𝅘𝅥 ";
        break;
      case 8:
        cout << "𝅗𝅥 ";
        break;
      case 16:
        cout << "𝅝 ";
        break;
      default:
        cout << note << " ";
        break;
    }
    if ( intervals == 0 || i == notes.size() - 1 )
      continue;
    if ( intervals % 16 == 0 ) {
      cout << "| ";
    }
    if ( ( i + 1 ) % ( notes.size() / REPETITIONS ) == 0 ) {
      cout << endl;
    }
  }
  cout << endl;
}

void print_piano_roll( const vector<int>& roll )
{
  for ( unsigned i = 0; i < roll.size(); i++ ) {
    cout << roll[i];
  }
  cout << endl;
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

  vector<int> notes;
  for ( unsigned i = 0; i < REPETITIONS; i++ ) {
    notes.insert( notes.end(), HOT_CROSS_BUNS.begin(), HOT_CROSS_BUNS.end() );
  }
  vector<int> piano_roll;

  for ( unsigned i = 0; i < notes.size(); i++ ) {
    piano_roll.push_back( 1 );
    for ( int j = 0; j < notes[i] - 1; j++ ) {
      piano_roll.push_back( 0 );
    }
  }

  cout << "Hot Cross Buns" << endl;
  cout << "Notes: " << endl;
  print_notes( notes );
  cout << "True: " << endl;
  print_piano_roll( piano_roll );

  Input input;

  unsigned correct = 0;
  unsigned total = 0;

  vector<int> played;
  for ( unsigned i = 0; i < piano_roll.size(); i++ ) {
    if ( i < input.cols() ) {
      for ( unsigned j = 0; j < input.cols() - i; j++ ) {
        input( j ) = 0;
      }
      for ( unsigned j = 0; j < i; j++ ) {
        input( input.cols() - i + j ) = piano_roll[j];
      }
    } else {
      for ( unsigned j = 0; j < input.cols(); j++ ) {
        input( j ) = piano_roll[i + j - input.cols()];
      }
    }
    Output expected { piano_roll[i] };

    Infer infer;
    infer.apply( mynetwork, input );

    Training train;

    train.train(
      mynetwork, input, [&]( const Output& output ) { return output - expected; }, LEARNING_RATE );

    Output predicted = infer.output();

    const float threshold = 0.5;
    bool note_predicted = predicted( 0 ) > threshold;
    bool note_expected = expected( 0 ) > threshold;
    if ( note_predicted == note_expected )
      correct++;
    total++;
    if ( total == piano_roll.size() / REPETITIONS ) {
      cout << "Repetition " << ( i * REPETITIONS ) / piano_roll.size() << ": " << correct << "/" << total << " = "
           << ( correct / (float)total * 100 ) << "%" << endl;
      correct = 0;
      total = 0;
    }
    played.push_back( note_predicted );
  }

  cout << "Played: " << endl;
  vector<int> played_notes;
  int count = 0;
  for ( unsigned i = 1; i < played.size(); i++ ) {
    if ( played[i] == 1 ) {
      played_notes.push_back( count + 1 );
      count = 0;
    } else {
      count++;
    }
  }
  print_notes( played_notes );
  print_piano_roll( played );
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
