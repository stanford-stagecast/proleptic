#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <list>
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
  unsigned short type, note, velocity; // actual midi data
};

const auto PIANO_OFFSET = 21;

class Hash
{
  int num_buckets;
  list<MidiEvent>* hash_table;

public:
  Hash( int num_buckets ) { hash_table = new list<MidiEvent>[num_buckets]; }

  int hash_function( unsigned short note )
  {
    // return ( note - PIANO_OFFSET );
    return ( note % num_buckets );
  }

  void insert_event( MidiEvent ev )
  {
    int index = hash_function( ev.note );
    hash_table[index].push_back( ev );
  }
};

uint64_t max_tracker = 0;
auto tot_tracker = 0;
auto num_tracker = 0;

class MatchFinder
{
public:
  void process_events( uint64_t starting_ts, uint64_t ending_ts, const vector<MidiEvent>& events, Hash& storage )
  {
    // cout << "Processing chunk from " << starting_ts << ".." << ending_ts << " ms:";
    for ( const auto& ev : events ) {
      // cout << " [" << ev.type << " " << ev.note << " " << ev.velocity << "]";
      uint64_t epoch = Timer::timestamp_ns();
      storage.insert_event( ev );
      uint64_t end = Timer::timestamp_ns();

      tot_tracker += ( end - epoch );
      num_tracker += 1;

      if ( ( end - epoch ) > max_tracker ) {
        max_tracker = ( end - epoch );
      }
    } /*
    if ( events.empty() ) {
      cout << " (none)";
    }
    cout << "\n";
    */
  }
};

static constexpr uint64_t chunk_duration_ms = 5;

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* read MIDI file */
  ifstream midi_data { midi_filename }; // opens midi_filename given
  midi_data.unsetf( ios::dec );
  midi_data.unsetf( ios::oct );
  midi_data.unsetf( ios::hex );

  vector<MidiEvent> events_in_chunk;
  uint64_t end_of_chunk = chunk_duration_ms;
  MatchFinder match_finder;

  uint64_t epoch = Timer::timestamp_ns();

  Hash storage( 88 );

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event

    while ( timestamp >= end_of_chunk ) {
      match_finder.process_events( end_of_chunk - chunk_duration_ms, end_of_chunk, events_in_chunk, storage );
      events_in_chunk.clear();
      end_of_chunk += chunk_duration_ms;
    }
    events_in_chunk.push_back( move( ev ) );
  }

  /*
   * TO DO: Have code that runs every 5 ms (instead of sleep until next event, sleep until 5 ms from now.)
   * Upon waking up we look at all the events that occurred since last wakeup (could be empty).
   * Call fn with that set of events (could be empty). Fn will be responsible for:
   * 1. Store data in some data struct
   * 2. Find most similar part of history (if any).
   */
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
    cout << "Maximum time: " << ( max_tracker / MILLION ) << " ms\n";
    cout << "Average time: " << ( tot_tracker / MILLION / num_tracker ) << " ms\n";
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
