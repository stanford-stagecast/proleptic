#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

#include "match-finder.hh"
#include "timer.hh"

using namespace std;

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

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event

    while ( timestamp >= end_of_chunk ) {
      match_finder.process_events( events_in_chunk );
      events_in_chunk.clear();
      end_of_chunk += chunk_duration_ms;
    }
    events_in_chunk.push_back( std::move( ev ) );
  }
  match_finder.print_stats();
  // match_finder.print_predict();
  // match_finder.print_storage();

  /*
   * TO DO:
   * Each time that MatchFinder::process_events is called for a KeyDown, it should find ALL times
   * that the same key was KeyDowned in the past, and for all of THOSE times where there was any
   * subsequent KeyDown, it should make a list of all the UNIQUE keys that came immediately after.
   * This could be an empty list (if it's the first time this key has been seen in a KeyDown),
   * or it could be a list of up to 88 entries (it could have preceded every other key going down
   * at some point in the past).
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
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
