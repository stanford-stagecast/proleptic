#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

#include "exception.hh"
#include "file_descriptor.hh"
#include "midi_processor.hh"
#include "timer.hh"

using namespace std;

struct MidiEvent
{
  uint64_t timestamp;                  // when
  unsigned short type, note, velocity; // actual midi data
};

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* read MIDI file */
  ifstream midi_data { midi_filename }; // opens midi_filename given
  midi_data.unsetf( ios::dec );
  midi_data.unsetf( ios::oct );
  midi_data.unsetf( ios::hex );

  vector<MidiEvent> events; // declare vector of events

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    events.emplace_back(); // make new event
    MidiEvent& ev = events.back();
    midi_data >> ev.timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event
  }

  cerr << "Read " << midi_filename << " with " << events.size() << " events.\n"; // done

  const uint64_t initial_timestamp = Timer::timestamp_ns();

  for ( const auto& ev : events ) {
    uint64_t now = Timer::timestamp_ns() - initial_timestamp; // compute current time
    uint64_t target_ns = MILLION * ev.timestamp;              // compute target time stamp for this current event

    while ( target_ns > now ) {
      this_thread::sleep_for( chrono::nanoseconds( target_ns - now ) ); // sleep until target time stamp
      now = Timer::timestamp_ns() - initial_timestamp;
    }

    /*
     * TO DO: Have code that runs every 5 ms (instead of sleep until next event, sleep until 5 ms from now.)
     * Upon waking up we look at all the events that occurred since last wakeup (could be empty).
     * Call fn with that set of events (could be empty). Fn will be responsible for:
     * 1. Store data in some data struct
     * 2. Find most similar part of history (if any).
     */
  }
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " midi_filename\n";
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
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
