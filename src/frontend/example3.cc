#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"

using namespace std;

void program_body( const string_view device_prefix )
{
  auto [name, interface_name] = ALSADevices::find_device( { device_prefix } );
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  AudioInterface audio { interface_name, "playback", SND_PCM_STREAM_PLAYBACK };

  audio.initialize();
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " [device_prefix]\n";

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
