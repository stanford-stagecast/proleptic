#include <array>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
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

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  EventLoop event_loop;

  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDONLY ) ) };

  MidiProcessor midi;

  vector<MidiEvent> events_in_chunk;
  uint64_t end_of_chunk = Timer::timestamp_ns() + chunk_duration_ns;
  MatchFinder match_finder;

  event_loop.add_rule( "read MIDI data", piano, Direction::In, [&] {
    midi.read_from_fd( piano );
    while ( midi.has_event() ) {
      events_in_chunk.emplace_back(
        MidiEvent { midi.get_event_type(), midi.get_event_note(), midi.get_event_velocity() } );
      midi.pop_event();
    }
  } );

  event_loop.add_rule(
    "process MIDI chunk",
    [&] {
      match_finder.process_events( events_in_chunk );
      events_in_chunk.clear();
      end_of_chunk += chunk_duration_ns;
    },
    [&] { return Timer::timestamp_ns() >= end_of_chunk; } );

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
      cerr << endl;
      next_stats_print_time = Timer::timestamp_ns() + stats_interval_ns;
    },
    [&] { return Timer::timestamp_ns() >= next_stats_print_time; } );

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
