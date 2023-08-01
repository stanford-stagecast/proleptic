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

void read_as_unsigned_int( ifstream& stream, uint8_t& target )
{
  unsigned int value;
  stream >> value;
  target = value;
}

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* read MIDI file */
  ifstream midi_data { midi_filename }; // opens midi_filename given
  midi_data.unsetf( ios::dec );
  midi_data.unsetf( ios::oct );
  midi_data.unsetf( ios::hex );

  MatchFinder match_finder;

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp;
    read_as_unsigned_int( midi_data, ev.type );
    read_as_unsigned_int( midi_data, ev.note );
    read_as_unsigned_int( midi_data, ev.velocity );

    GlobalScopeTimer<Timer::Category::ProcessPianoEvent> timer;
    match_finder.process_event( ev );
    match_finder.predict_next_event();
  }
  match_finder.summary( cout );
  cout << "\n";
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
    global_timer().summary( cout );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    global_timer().summary( cout );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
