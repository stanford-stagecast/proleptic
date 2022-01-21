#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"

using namespace std;

void program_body( const string_view device_prefix )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* create event loop */
  auto event_loop = make_shared<EventLoop>();

  /* find the audio device */
  auto [name, interface_name] = ALSADevices::find_device( { device_prefix } );

  /* claim exclusive access to the audio device */
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  /* use ALSA to initialize and configure audio device */
  AudioInterface playback_interface { interface_name, "playback", SND_PCM_STREAM_PLAYBACK };
  AudioInterface::Configuration config;
  config.sample_rate = 48000; /* samples per second */
  config.buffer_size = 192;   /* maximum samples of queued audio = 4 milliseconds */
  config.period_size = 48;    /* kernel will generally transfer units of 1 millisecond */
  config.avail_minimum = 48;  /* kernel will wake us up when 1 millisecond can be written */
  playback_interface.set_config( config );
  playback_interface.initialize();

  /* get ready to play an audio signal */
  ChannelPair audio_signal { 16384 };  // the output signal
  size_t next_sample_to_calculate = 0; // what's the next sample # to be written to the output signal?

  /* rule #1: write a continuous sine wave (but no more than 1 millisecond into the future) */
  event_loop->add_rule(
    "calculate sine wave",
    [&] {
      const double time = next_sample_to_calculate / double( config.sample_rate );
      /* compute the sine wave amplitude (middle A, 440 Hz) */
      audio_signal.safe_set( next_sample_to_calculate,
                             { 0.99 * sin( 2 * M_PI * 440 * time ), 0.99 * sin( 2 * M_PI * 440 * time ) } );
      next_sample_to_calculate++;
    },
    /* when should this rule run? commit to an output signal until 1 millisecond in the future */
    [&] { return next_sample_to_calculate <= playback_interface.cursor() + 4800; } );

  /* rule #2: play the output signal whenever space available in audio output buffer */
  event_loop->add_rule(
    "play sine wave",
    playback_interface.fd(), /* file descriptor event cares about */
    Direction::Out,          /* execute rule when file descriptor is "writeable"
                                -> there's room in the output buffer (config.buffer_size) */
    [&] {
      playback_interface.play( next_sample_to_calculate, audio_signal );
      /* now that we've played these samples, pop them from the outgoing audio signal */
      audio_signal.pop_before( playback_interface.cursor() );
    },
    [&] {
      return next_sample_to_calculate > playback_interface.cursor();
    },     /* rule should run as long as any new samples available to play */
    [] {}, /* no callback if EOF or closed */
    [&] {  /* on error such as buffer overrun/underrun, recover the ALSA interface */
          playback_interface.recover();
          return true;
    } );

  /* run the event loop forever */
  while ( event_loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
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
