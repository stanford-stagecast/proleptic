#include <array>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <queue>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "eventloop.hh"
#include "exception.hh"
#include "match-finder.hh"
#include "midi_processor.hh"
#include "timer.hh"

using namespace std;

static constexpr uint64_t chunk_duration_ns = 5 * MILLION;
static constexpr uint64_t stats_interval_ns = 1 * BILLION;
static constexpr uint64_t status_dot_interval_ns = BILLION / 20;
static constexpr char CLEAR_SCREEN[] = "\033[H\033[2J\033[3J";
static constexpr uint64_t DELAY_VALUE = 500; // how long to play the prediction after playing the note

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  EventLoop event_loop;

  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDWR ) ) };

  MidiProcessor midi;

  MatchFinder match_finder;

  queue<MidiEvent> outgoing_predictions;

  event_loop.add_rule( "read MIDI event + predict next", piano, Direction::In, [&] {
    midi.read_from_fd( piano );
    while ( midi.has_event() ) {
      match_finder.process_event(
        MidiEvent { midi.get_event_type(), midi.get_event_note(), midi.get_event_velocity() } );
      midi.pop_event();

      auto maybe_prediction = match_finder.predict_next_event();
      if ( maybe_prediction.has_value() ) {
        outgoing_predictions.push( maybe_prediction.value() );
      }
    }
  } );

  uint64_t next_stats_print_time = Timer::timestamp_ns();

  event_loop.add_rule(
    "print stats",
    [&] {
      cerr << CLEAR_SCREEN;
      global_timer().summary( cerr );
      cerr << "\n";
      event_loop.summary( cerr );
      event_loop.reset_summary();
      cerr << "\n";
      match_finder.summary( cerr );
      // match_finder.print_data_structure( cerr ); // output storage data structure to screen

      cerr << endl;
      next_stats_print_time = Timer::timestamp_ns() + stats_interval_ns;
    },
    [&] { return Timer::timestamp_ns() >= next_stats_print_time; } );

  event_loop.add_rule(
    "play prediction",
    piano,
    Direction::Out,
    [&] {
      while ( !outgoing_predictions.empty() ) {
        piano.write( outgoing_predictions.front().to_string_view() );
        outgoing_predictions.pop();
      }
    },
    [&] { return not outgoing_predictions.empty(); } );

  uint64_t next_dot = Timer::timestamp_ns() + status_dot_interval_ns;

  event_loop.add_rule(
    "print status dot",
    [&] {
      cerr << ".";
      next_dot += status_dot_interval_ns;
    },
    [&] { return Timer::timestamp_ns() >= next_dot; } );

  while ( event_loop.wait_next_event( 1 ) != EventLoop::Result::Exit ) {}
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " midi_device [typically /dev/snd/midi*]\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }
    program_body( argv[1] );
    global_timer().summary( cout );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    global_timer().summary( cout );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
