#include <fcntl.h>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>

#include "exception.hh"
#include "file_descriptor.hh"
#include "midi_processor.hh"
#include "timer.hh"

using namespace std;

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDONLY ) ) };

  MidiProcessor midi;

  optional<uint64_t> ns_of_first_event;

  while ( not piano.eof() ) {
    midi.read_from_fd( piano );

    if ( midi.has_event() ) {
      const uint64_t event_ts = Timer::timestamp_ns();
      if ( not ns_of_first_event.has_value() ) {
        ns_of_first_event.emplace( event_ts );
      }

      const uint64_t ms_since_first_event = ( event_ts - ns_of_first_event.value() ) / MILLION;
      cout << ms_since_first_event << " 0x" << hex << static_cast<int>( midi.get_event_type() ) << " 0x"
           << static_cast<int>( midi.get_event_note() ) << " 0x" << static_cast<int>( midi.get_event_velocity() )
           << endl;

      midi.pop_event();
    }
  }
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
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
