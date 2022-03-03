#include <iostream>
#include <set>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "midi_processor.hh"
#include "stats_printer.hh"
#include "synthesizer.hh"
#include "wav_wrapper.hh"
#include <alsa/asoundlib.h>

using namespace std;

void program_body( const string_view device_prefix, const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* create event loop */
  auto event_loop = make_shared<EventLoop>();

  /* find the audio device */
  auto [name, interface_name] = ALSADevices::find_device( { device_prefix } );

  /* claim exclusive access to the audio device */
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  const string first_input_filename = "/home/yasminem/D#1v8.5-PA.wav";
  const string second_input_filename = "/home/yasminem/C4v16.wav";

  /* use ALSA to initialize and configure audio device */
  const auto short_name = device_prefix.substr( 0, 16 );
  auto playback_interface = make_shared<AudioInterface>( interface_name, short_name, SND_PCM_STREAM_PLAYBACK );
  AudioInterface::Configuration config;
  config.sample_rate = 48000; /* samples per second */
  config.buffer_size = 96;    /* maximum samples of queued audio = 2 milliseconds */
  config.period_size = 16;    /* chunk size for kernel's management of audio buffer */
  config.avail_minimum = 64;  /* device is writeable with 64 samples can be written */
  playback_interface->set_config( config );
  playback_interface->initialize();

  /* get ready to play an audio signal */
  ChannelPair audio_signal { 16384 }; // the output signal
  size_t samples_written = 0;

  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDONLY ) ) };
  Synthesizer synth;

  /* rule #1: read events from MIDI piano and adjust synthesizer state as a result */
  event_loop->add_rule( "read MIDI data", piano, Direction::In, [&] { synth.process_new_data( piano ); } );

  /* rule #2: write synthesizer output to speaker (but no more than 1 millisecond into the future) */
  event_loop->add_rule(
    "add next frame from wav file",
    [&] {
      while ( samples_written <= playback_interface->cursor() + 48 ) {
        pair<float, float> samp = synth.calculate_curr_sample();
        // cout << "total samp: " << samp.first << "\n";
        audio_signal.safe_set( samples_written, samp );
        samples_written++;
        synth.advance_sample();
      }
    },
    /* when should this rule run? commit to an output signal until 1 millisecond in the future */
    [&] { return samples_written <= playback_interface->cursor() + 48; } );

  /* rule #3: play the output signal whenever space available in audio output buffer */
  event_loop->add_rule(
    "play wav file",
    playback_interface->fd(), /* file descriptor event cares about */
    Direction::Out,           /* execute rule when file descriptor is "writeable"
                                 -> there's room in the output buffer (config.buffer_size) */
    [&] {
      playback_interface->play( samples_written, audio_signal );
      /* now that we've played these samples, pop them from the outgoing audio signal */
      audio_signal.pop_before( playback_interface->cursor() );
    },
    [&] {
      return samples_written > playback_interface->cursor();
    },     /* rule should run as long as any new samples available to play */
    [] {}, /* no callback if EOF or closed */
    [&] {  /* on error such as buffer overrun/underrun, recover the ALSA interface */
          playback_interface->recover();
          return true;
    } );

  /* add a task that prints statistics occasionally */
  StatsPrinterTask stats_printer { event_loop };

  stats_printer.add( playback_interface );

  /* run the event loop forever */
  while ( event_loop->wait_next_event( stats_printer.wait_time_ms() ) != EventLoop::Result::Exit ) {
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
