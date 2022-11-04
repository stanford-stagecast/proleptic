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

static string predictor_file = "/usr/local/share/piano-roll-predictor.dnn";

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
  config.buffer_size = 48 * 20;         /* maximum samples of queued audio = 1 millisecond */
  config.period_size = 16;              /* chunk size for kernel's management of audio buffer */
  config.avail_minimum = audio_horizon; /* device is writeable when full horizon can be written */
  long microseconds_per_samp = static_cast<long>( 1000000 / double( config.sample_rate ) + 0.5 );
  playback_interface->set_config( config );
  playback_interface->initialize();

  /* open the MIDI device */
  FileDescriptor piano { CheckSystemCall( midi_device, open( midi_device.c_str(), O_RDONLY ) ) };
  MidiProcessor midi;
  struct Press
  {
    steady_clock::time_point time;
    int key;
    int velocity;
    Press( steady_clock::time_point t, int k, int v )
      : time( t )
      , key( k )
      , velocity( v )
    {}
  };
  deque<Press> pending_queue {};
  deque<Press> history_queue {};
  deque<array<bool, 88>> piano_roll { {} };
  for ( size_t i = 0; i < 88; i++ ) {
    piano_roll.back()[i] = PianoRollEvent::Unknown;
  }

  /* get ready to play an audio signal */
  ChannelPair audio_signal { 16384 };  // the output signal
  size_t next_sample_to_calculate = 0; // what's the next sample # to be written to the output signal?

  /* current amplitude of the sine waves */
  array<float, 88> amp_left = {}, amp_right = {};

  steady_clock::time_point last_note_pred { steady_clock::now() };
  steady_clock::time_point next_note_pred { steady_clock::now() };
  steady_clock::time_point last_pred_time { steady_clock::now() };
  steady_clock::time_point curr_time { steady_clock::now() };
  array<bool, 88> should_play_next_pred {};
  steady_clock::duration next_note_duration = milliseconds( 1000 );
  steady_clock::duration last_note_duration = milliseconds( 1000 );
  bool seen_note = false;
  float time_since_pred_note = 0;
  SimpleNN nn { predictor_file };

  /* rule #1: write a continuous sine wave (but no more than 1.3 ms into the future) */
  event_loop->add_rule(
    "calculate sine wave",
    [&] {
      curr_time = steady_clock::now();
      while ( next_sample_to_calculate <= playback_interface->cursor() + audio_horizon ) {
        const double time = next_sample_to_calculate / double( config.sample_rate );
        /* compute the sine wave amplitude (middle A, 440 Hz) */
        const float a0 = 27.50;
        float total_amp_left = 0, total_amp_right = 0;
        for ( size_t i = 0; i < 88; i++ ) {
          const float frequency = a0 * pow( 2.f, i / 12.f );
          // const float frequency = 440.f;
          total_amp_left += amp_left[i] * sin( 2 * M_PI * frequency * time );
          total_amp_right += amp_right[i] * sin( 2 * M_PI * ( 2 * frequency ) * time );
        }
        total_amp_left *= 0.2;
        total_amp_right *= 0.2;
        if ( not seen_note )
          total_amp_right = 0.0;
        audio_signal.safe_set( next_sample_to_calculate, { total_amp_left, total_amp_right } );
        for ( size_t i = 0; i < 88; i++ ) {
          amp_left[i] *= note_decay_rate;
        }
        // amp_right = some equation based on note_decay_rate and next_note_pred
        for ( size_t i = 0; i < 88; i++ ) {
          if ( should_play_next_pred[i] && next_note_pred <= curr_time
               && ( curr_time - next_note_pred ) < simulated_latency ) {
            time_since_pred_note
              = ( config.sample_rate * duration_cast<microseconds>( curr_time - next_note_pred ).count() )
                / 1000000.f;
            amp_right[i] = max_amplitude * pow( note_decay_rate, time_since_pred_note );
          } else {
            amp_right[i] *= note_decay_rate;
          }
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
        if ( midi.get_event_type() == 144 || midi.get_event_type() == 128 ) { /* key down */
          uint8_t note_val = midi.get_event_note();
          uint8_t velocity_val = midi.get_event_type() == 128 ? 0 : midi.get_event_velocity();
          if ( note_val >= 21 and note_val <= 108 ) {
            ssize_t note_index = ( note_val - 21 );
            amp_left[note_index] = velocity_val ? velocity_val / 127.f * max_amplitude : amp_left[note_index];
          }
          pending_queue.push_back( Press( time_val, note_val, velocity_val ) );
        }
        midi.pop_event();
      }
    },
    [&] { return midi.has_event(); } );

  /* rule #4.5: add latency to MIDI events */
  event_loop->add_rule(
    "simulate latency on MIDI events",
    [&] {
      history_queue.push_back( pending_queue.front() );
      pending_queue.pop_front();
      if ( history_queue.size() > 512 ) {
        history_queue.pop_front();
      }
    },
    [&] {
      const auto now = steady_clock::now();
      return !pending_queue.empty() and pending_queue.front().time < now - simulated_latency;
    } );

  /* rule #5: get DNN prediction */
  event_loop->add_rule(
    "get DNN prediction",
    [&] {
      const auto now = steady_clock::now();
      const auto adjusted_now = now - simulated_latency;
      std::vector<SimpleNN::KeyPress> past_timestamps;

      for ( const auto& press : history_queue ) {
        if ( press.velocity > 0 and press.key >= 21 and press.key <= 108 ) {
          past_timestamps.push_back( SimpleNN::KeyPress( press.key - 21, press.time ) );
          seen_note = true;
        }
      }

      if ( next_note_pred < adjusted_now ) {
        last_note_pred = next_note_pred;
        const auto prediction = nn.predict_next_timeslot( past_timestamps );
        next_note_pred = prediction.start;
        last_note_duration = next_note_duration;
        next_note_duration = prediction.length;
        piano_roll.push_back( piano_roll.back() );
        if ( piano_roll.size() > SimpleNN::HISTORY + 1 ) {
          piano_roll.pop_front();
        }
      }

      for ( const auto& press : history_queue ) {
        if ( press.key < 21 or press.key > 108 )
          continue;
        if ( press.time > last_note_pred - last_note_duration / 2
             && press.time <= next_note_pred - next_note_duration / 2 ) {
          piano_roll.back()[press.key - 21] = press.velocity > 0 ? true : false;
        }
      }

      if ( piano_roll.size() < 2 ) {
        for ( size_t i = 0; i < 88; i++ ) {
          should_play_next_pred[i] = false;
        }
        last_pred_time = now;
        return;
      }

      vector<array<bool, 88>> old_notes( piano_roll.begin(), piano_roll.end() - 1 );
      nn.train_next_note_values( old_notes, piano_roll.back() );

      vector<array<bool, 88>> notes( piano_roll.begin() + 1, piano_roll.end() );
      // cerr << "\n";
      for ( size_t j = 0; j < notes.size(); j++ ) {
        for ( size_t i = 0; i < 88; i++ ) {
          // cerr << (bool)notes[j][i];
        }
        // cerr << "\n";
      }
      should_play_next_pred = nn.predict_next_note_values( notes );
      for ( size_t i = 0; i < 88; i++ ) {
        // cerr << should_play_next_pred[i];
      }
      // cerr << endl;

      last_pred_time = now;
    },
    [&] { return duration_cast<milliseconds>( steady_clock::now() - last_pred_time ).count() >= 25; } );

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
