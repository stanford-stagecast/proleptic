#include <alsa/asoundlib.h>
#include <chrono>
#include <deque>
#include <iostream>
#include <random>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "midi_processor.hh"
#include "simplenn.hh"
#include "stats_printer.hh"

using namespace std;
using namespace chrono;

static constexpr unsigned int audio_horizon = 16; /* samples */
static constexpr float max_amplitude = 0.9;
static constexpr float note_decay_rate = 0.9998;
static constexpr auto simulated_latency = milliseconds( 100 );

static string nn_file = "/usr/local/share/piano-roll-rhythm.dnn";

void program_body( const string_view audio_device, const string& midi_device )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* create event loop */
  auto event_loop = make_shared<EventLoop>();

  /* find the audio device */
  auto [name, interface_name] = ALSADevices::find_device( { audio_device } );

  /* claim exclusive access to the audio device */
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  /* use ALSA to initialize and configure audio device */
  const auto short_name = audio_device.substr( 0, 16 );
  auto playback_interface = make_shared<AudioInterface>( interface_name, short_name, SND_PCM_STREAM_PLAYBACK );
  AudioInterface::Configuration config;
  config.sample_rate = 48000;           /* samples per second */
  config.buffer_size = 48;              /* maximum samples of queued audio = 1 millisecond */
  config.period_size = 16;              /* chunk size for kernel's management of audio buffer */
  config.avail_minimum = audio_horizon; /* device is writeable when full horizon can be written */
  long microseconds_per_samp = static_cast<long>( 1000000 / double( config.sample_rate ) + 0.5 );
  playback_interface->set_config( config );
  playback_interface->initialize();

  /* open the MIDI device */
  FileDescriptor piano { CheckSystemCall( midi_device, open( midi_device.c_str(), O_RDONLY ) ) };
  MidiProcessor midi;
  deque<steady_clock::time_point> press_queue {};
  std::deque<bool> piano_roll {};
  size_t num_notes = 0;

  /* get ready to play an audio signal */
  ChannelPair audio_signal { 16384 };  // the output signal
  size_t next_sample_to_calculate = 0; // what's the next sample # to be written to the output signal?

  /* current amplitude of the sine waves */
  float amp_left = 0, amp_right = 0;

  steady_clock::time_point last_note_pred { steady_clock::now() };
  steady_clock::time_point next_note_pred { steady_clock::now() };
  steady_clock::time_point last_pred_time { steady_clock::now() };
  steady_clock::time_point curr_time { steady_clock::now() };
  bool should_play_next_pred = false;
  steady_clock::duration next_note_duration {};
  steady_clock::duration last_note_duration {};
  PianoRollPredictor nn { nn_file };
  float time_since_pred_note = 0;

  /* rule #1: write a continuous sine wave (but no more than 1.3 ms into the future) */
  event_loop->add_rule(
    "calculate sine wave",
    [&] {
      curr_time = steady_clock::now();
      while ( next_sample_to_calculate <= playback_interface->cursor() + audio_horizon ) {
        const double time = next_sample_to_calculate / double( config.sample_rate );
        /* compute the sine wave amplitude (middle A, 440 Hz) */
        audio_signal.safe_set(
          next_sample_to_calculate,
          { amp_left * sin( 2 * M_PI * 440 * time ), amp_right * sin( 2 * M_PI * 523.25 * time ) } );
        amp_left *= note_decay_rate;
        // amp_right = some equation based on note_decay_rate and next_note_pred
        if ( should_play_next_pred && next_note_pred <= curr_time
             && ( curr_time - next_note_pred ) < simulated_latency ) {
          time_since_pred_note
            = ( config.sample_rate * duration_cast<microseconds>( curr_time - next_note_pred ).count() )
              / 1000000.f;
          amp_right = max_amplitude * pow( note_decay_rate, time_since_pred_note );
        } else {
          amp_right *= note_decay_rate;
        }
        next_sample_to_calculate++;
        curr_time += microseconds( microseconds_per_samp );
      }
    },
    /* when should this rule run? commit to an output signal until some horizon in the future */
    [&] { return next_sample_to_calculate <= playback_interface->cursor() + audio_horizon; } );

  /* rule #2: play the output signal whenever space available in audio output buffer */
  event_loop->add_rule(
    "play sine wave",
    playback_interface->fd(), /* file descriptor event cares about */
    Direction::Out,           /* execute rule when file descriptor is "writeable"
                                 -> there's room in the output buffer (config.buffer_size) */
    [&] {
      playback_interface->play( next_sample_to_calculate, audio_signal );
      /* now that we've played these samples, pop them from the outgoing audio signal */
      audio_signal.pop_before( playback_interface->cursor() );
    },
    [&] {
      return next_sample_to_calculate > playback_interface->cursor();
    },     /* rule should run as long as any new samples available to play */
    [] {}, /* no callback if EOF or closed */
    [&] {  /* on error such as buffer overrun/underrun, recover the ALSA interface */
          playback_interface->recover();
          return true;
    } );

  default_random_engine rng;
  /* unsigned tempo = 60; */
  normal_distribution<float> tempo_distribution( 60, 20 );
  /* rule #3: read MIDI data */
  event_loop->add_rule(
    "read MIDI event",
    piano,
    Direction::In,
    [&] { midi.read_from_fd( piano ); },
    [&] { return not midi.data_timeout(); } );

  /* rule #4: process MIDI data */
  event_loop->add_rule(
    "process MIDI event",
    [&] {
      while ( midi.has_event() ) {
        steady_clock::time_point time_val = midi.get_event_time();
        if ( midi.get_event_type() == 144 ) { /* key down */
          amp_left = max_amplitude;
          // keep a few extra in case some of the new presses are discarded to simulate latency
          if ( num_notes < 64 + 4 ) {
            press_queue.push_back( time_val );
            num_notes++;
          } else {
            press_queue.push_back( time_val );
            press_queue.pop_front();
          }
        }
        midi.pop_event();
      }
    },
    [&] { return midi.has_event(); } );

  /* rule #5: get DNN prediction */
  event_loop->add_rule(
    "get DNN prediction",
    [&] {
      const auto now = steady_clock::now();
      const auto adjusted_now = now - simulated_latency;
      std::vector<steady_clock::time_point> past_timestamps;

      for ( const auto& press : press_queue ) {
        if ( press > adjusted_now ) {
          continue;
        }
        past_timestamps.push_back( press );
      }

      if ( next_note_pred < adjusted_now ) {
        last_note_pred = next_note_pred;
        const auto prediction = nn.predict_next_note_time( past_timestamps );
        next_note_pred = prediction.first;
        last_note_duration = next_note_duration;
        next_note_duration = prediction.second;
        piano_roll.push_back( false );
        if ( piano_roll.size() > 65 ) {
          piano_roll.pop_front();
        }
      }

      for ( const auto& press : past_timestamps ) {
        if ( press > last_note_pred - last_note_duration / 2 && press <= next_note_pred - next_note_duration / 2 ) {
          piano_roll.back() = true;
        }
      }

      if ( piano_roll.size() < 2 ) {
        should_play_next_pred = false;
        last_pred_time = now;
        return;
      }

      /*
      for ( const bool note : piano_roll ) {
        cerr << note;
      }
      cerr << " ";
      */
      std::vector<bool> old_notes( piano_roll.begin(), piano_roll.end() - 1 );
      nn.train_next_note_value( old_notes, piano_roll.back() );

      std::vector<bool> notes( piano_roll.begin() + 1, piano_roll.end() );
      should_play_next_pred = nn.predict_next_note_value( notes );
      /* cerr << should_play_next_pred;
      cerr << "\n"; */

      last_pred_time = now;
    },
    [&] { return duration_cast<milliseconds>( steady_clock::now() - last_pred_time ).count() >= 10; } );

  /* add a task that prints statistics occasionally */
  StatsPrinterTask stats_printer { event_loop };

  stats_printer.add( playback_interface );

  /* run the event loop forever */
  while ( event_loop->wait_next_event( stats_printer.wait_time_ms() ) != EventLoop::Result::Exit ) {}
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " [audio_device] [midi_device]\n";

  cerr << "Available audio devices:";

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
