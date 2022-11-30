#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "spans.hh"
#include "stats_printer.hh"
#include "wav_wrapper.hh"

#include <alsa/asoundlib.h>

using namespace std;

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* create event loop */
  auto event_loop = make_shared<EventLoop>();

  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDONLY ) ) };

  string data( 1024, 'x' );
  auto data_writable = string_span::from_view( data );

  event_loop->add_rule( "read MIDI data", piano, Direction::In, [&] {
    const auto bytes_read = piano.read( data_writable );
    string_view data_read_this_time { data.data(), bytes_read };

    for ( const uint8_t byte : data_read_this_time ) {
      if ( byte != 0xfe ) {
        cerr << "Piano gave us: " << static_cast<unsigned int>( byte ) << "\n";
      }
    }
  } );

  /* add a task that prints statistics occasionally */
  //  StatsPrinterTask stats_printer { event_loop };

  //  stats_printer.add( playback_interface );

  /* run the event loop forever */
  while ( event_loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {}
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " [midi_filename]\n";

  /*
  cerr << "Available devices:";

  const auto devices = ALSADevices::list();

  if ( devices.empty() ) {
    cerr << " none\n";
  } else {
    cerr << "\n";
    for ( const auto& dev : devices ) {
      for ( const auto& interface : dev.interfaces ) {
        cerr << "  '" << interface.second << "'\n";
      }
    }
  }
  */
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
