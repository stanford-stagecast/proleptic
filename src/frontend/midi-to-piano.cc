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
  uint64_t timestamp;
  unsigned short type, note, velocity;
};

void program_body( const string& midi_device, const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  FileDescriptor piano { CheckSystemCall( midi_device, open( midi_device.c_str(), O_WRONLY ) ) };

  /* read MIDI file */
  ifstream midi_data { midi_filename };
  midi_data.unsetf( ios::dec );
  midi_data.unsetf( ios::oct );
  midi_data.unsetf( ios::hex );

  vector<MidiEvent> events;

  while ( not midi_data.eof() ) {
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    events.emplace_back();
    MidiEvent& ev = events.back();
    midi_data >> ev.timestamp >> ev.type >> ev.note >> ev.velocity;
  }

  cerr << "Read " << midi_filename << " with " << events.size() << " events.\n";

  const uint64_t initial_timestamp = Timer::timestamp_ns();
  for ( const auto& ev : events ) {
    std::array<char, 3> data { char( ev.type ), char( ev.note ), char( ev.velocity ) };

    uint64_t now = Timer::timestamp_ns() - initial_timestamp;

    uint64_t target_ns = MILLION * ev.timestamp;

    while ( target_ns > now ) {
      this_thread::sleep_for( chrono::nanoseconds( target_ns - now ) );
      now = Timer::timestamp_ns() - initial_timestamp;
    }

    piano.write( { data.begin(), data.size() } );
  }
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " midi_device [typically /dev/snd/midi*] midi_filename\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
