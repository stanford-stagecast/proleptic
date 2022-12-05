#include <alsa/asoundlib.h>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "midi_processor.hh"
#include "simplenn.hh"
#include "stats_printer.hh"

using namespace std;
using namespace chrono;

const int input_size = 4;

static constexpr unsigned int audio_horizon = 16; /* samples */
static constexpr float max_amplitude = 0.9;
static constexpr float note_decay_rate = 0.9998;

// Reject timestamp deltas smaller than timestamp_delta_threshold
// For now, the threshold = length of 1/32 note under tempo 180 assuming time
// signature is 4/4
static constexpr float timestamp_delta_threshold = 60.0 / 180 / 8;

float find_min_error_term_index( auto vec )
{
  float min = accumulate( get<1>( vec[0] ).begin(), get<1>( vec[0] ).end(), 0.0 );
  int min_index = 0;
  for ( int i = 0; i < (int)vec.size(); ++i ) {
    vector<float> ind_errors = get<1>( vec[i] );
    float error_sum = accumulate( ind_errors.begin(), ind_errors.end(), 0.0 );
    if ( error_sum < min ) {
      min = error_sum;
      min_index = i;
    }
  }
  return min_index;
}

bool check_timestamp_delta( steady_clock::time_point time_val, deque<steady_clock::time_point> press_queue )
{
  // if there is no element in press_queue, proceed with True
  if ( press_queue.size() == 0 )
    return false;

  // compute the timestamp delta between the latest two timestamps
  steady_clock::time_point last_time_val = press_queue.back();
  float timestamp_delta = ( duration_cast<milliseconds>( time_val - last_time_val ) ).count() / 1000.f;

  // see if duration is too small, if so, reject the current time_val
  if ( timestamp_delta < timestamp_delta_threshold )
    return true;
  else
    return false;
}

tuple<array<float, input_size>, int> get_absolute_timing( deque<steady_clock::time_point> times,
                                                          steady_clock::time_point begin_time )
{
  array<float, input_size> ret_mat {};
  int effective_len = input_size;

  vector<float> timestamps;
  for ( const auto& time : times ) {
    timestamps.push_back( duration_cast<milliseconds>( time - begin_time ).count() / 1000.f );
  }

  for ( size_t i = 0; i < input_size; i++ ) {
    if ( i >= timestamps.size() ) {
      ret_mat[i] = 0;
      effective_len--;
    } else {
      ret_mat[i] = timestamps[i];
    }
  }

  return make_tuple( ret_mat, effective_len );
}

float metronome_wave_val( float period, float phase, float current_time )
{
  return cos( ( 2 * M_PI / period ) * ( current_time + phase ) );
}

float metronome_wave_error( float metronome_wave_value )
{
  vector<float> errors;
  errors.push_back( 0.25 * abs( metronome_wave_value - 1 ) ); // full beat
  errors.push_back( 0.5 * abs( metronome_wave_value + 1 ) );  // half beat
  errors.push_back( abs( metronome_wave_value ) );            // quarter beat
  return *min_element( errors.begin(), errors.end() );
}

float find_next_beat_val( float period, float offset, float current_time )
{
  int factor = (int)( ( current_time + offset ) / period );
  float upper_multiple = ( (float)factor + 1 ) * period;
  return upper_multiple - offset;
}

float find_lastest_timestamp( array<float, input_size> timestamps )
{
  for ( int i = input_size - 1; i >= 0; --i ) {
    if ( timestamps[i] > 0.0 )
      return timestamps[i];
  }
  return 0.0;
}

tuple<float, float> update_period_and_phase( float current_period,
                                             float current_phase,
                                             array<float, input_size> past_timestamps,
                                             int past_timestamps_eff_len,
                                             auto pairs_adjustments )
{
  // initialize the vector for storing errors
  vector<tuple<tuple<float, float>, vector<float>>> errors;
  for ( int i = 0; i < (int)pairs_adjustments.size(); ++i ) {
    errors.push_back( make_tuple( pairs_adjustments[i], vector<float>() ) );
  }

  // compute the errors based on the current metronome wave
  for ( int i = 0; i < (int)errors.size(); ++i ) {
    // if the vector of errors is not full, just push the new individual error term
    int size_error_vec = get<1>( errors[i] ).size();
    float period_adjustment = get<0>( get<0>( errors[i] ) );
    float phase_adjustment = get<1>( get<0>( errors[i] ) );
    if ( size_error_vec < past_timestamps_eff_len ) {
      for ( int j = size_error_vec; j < past_timestamps_eff_len; ++j )
        get<1>( errors[i] )
          .push_back( metronome_wave_error( metronome_wave_val(
            current_period + period_adjustment, current_phase + phase_adjustment, past_timestamps[j] ) ) );
    }
    // otherwise, pop the first error term, and then push the new individual error term
    else {
      get<1>( errors[i] )
        .push_back( metronome_wave_error( metronome_wave_val(
          current_period + period_adjustment, current_phase + phase_adjustment, past_timestamps.back() ) ) );
      get<1>( errors[i] ).erase( get<1>( errors[i] ).begin() );
    }
  }

  // update period and phase
  int min_error_index = find_min_error_term_index( errors );
  float period_change = get<0>( get<0>( errors[min_error_index] ) );
  float phase_change = get<1>( get<0>( errors[min_error_index] ) );
  current_period += period_change;
  current_phase += phase_change;
  cerr << "current period = " << current_period << endl;
  cerr << "current phase  = " << current_phase << endl;

  return make_tuple( current_period, current_phase );
}

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
  size_t num_notes = 0;

  /* get ready to play an audio signal */
  ChannelPair audio_signal { 16384 };  // the output signal
  size_t next_sample_to_calculate = 0; // what's the next sample # to be written to the output signal?

  /* current amplitude of the sine waves */
  float amp_left = 0, amp_right = 0;

  steady_clock::time_point next_note_pred { steady_clock::now() };
  steady_clock::time_point last_pred_time { steady_clock::now() };
  steady_clock::time_point curr_time { steady_clock::now() };
  float time_since_pred_note = 0;
  array<float, input_size> past_timestamps {};
  int past_timestamps_eff_len;
  float latest_timestamp = 0.0;
  size_t counter = 0;

  // begin_time: used for obtaining the absolute timings
  steady_clock::time_point begin_time { steady_clock::now() };
  steady_clock::time_point checkpoint_time { steady_clock::now() };

  // initialize a sine wave for the metronome based on estimated tempo
  float tempo = 90.0;
  float current_period = 60.0 / tempo;
  float current_phase = 0.0;
  float current_time_from_start;

  // beat-tracking parameters
  float current_beat_val = 0.0;

  // period & phase adjustment parameters
  float phase_step = 0.01;
  float period_step = 0.01;
  int len_period_adjustments = 3;
  int len_phase_adjustments = 7;

  // initialize the vector for storing phase adjustments
  vector<float> phase_adjustments { 0.0 };
  float phase_step_multiple = 1.0;
  while ( (int)phase_adjustments.size() < len_phase_adjustments ) {
    phase_adjustments.push_back( phase_step_multiple * phase_step );
    phase_adjustments.push_back( ( -1 ) * phase_step_multiple * phase_step );
    phase_step_multiple += 1.0;
  }

  // initialize the vector for storing period adjustments
  vector<float> period_adjustments { 0.0 };
  float period_step_multiple = 1.0;
  while ( (int)period_adjustments.size() < len_period_adjustments ) {
    period_adjustments.push_back( period_step_multiple * period_step );
    period_adjustments.push_back( ( -1 ) * period_step_multiple * period_step );
    period_step_multiple += 1.0;
  }

  // generate pairs of potential adjustments
  vector<tuple<float, float>> pairs_adjustments;
  for ( float period_ad : period_adjustments ) {
    for ( float phase_ad : phase_adjustments ) {
      pairs_adjustments.push_back( make_tuple( period_ad, phase_ad ) );
    }
  }

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
          { amp_left * sin( 2 * M_PI * 440 * time ), amp_right * sin( 2 * M_PI * 440 * 1.5 * time ) } );
        amp_left *= note_decay_rate;
        // amp_right = some equation based on note_decay_rate and next_note_pred
        if ( next_note_pred <= curr_time ) {
          time_since_pred_note
            = ( config.sample_rate * duration_cast<microseconds>( curr_time - next_note_pred ).count() ) / 1000000;
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
          // add current key-down to press_queue
          if ( !check_timestamp_delta( time_val, press_queue ) ) {
            press_queue.push_back( time_val );
            num_notes++;
          }
          // pop if queue is full
          if ( num_notes > input_size ) {
            press_queue.pop_front();
            num_notes--;
          }
        }
        midi.pop_event();
      }
    },
    [&] { return midi.has_event(); } );

  /* rule #5: update metronome wave and get beat prediction */
  event_loop->add_rule(
    "update metronome wave and get beat prediction",
    [&] {
      auto past_timestamps_info = get_absolute_timing( press_queue, begin_time );
      past_timestamps = get<0>( past_timestamps_info );
      past_timestamps_eff_len = get<1>( past_timestamps_info );
      last_pred_time = steady_clock::now();

      cerr << "past_timestamps = ";
      for ( int i = 0; i < input_size; i++ ) {
        cerr << past_timestamps[i] << " ";
      }
      cerr << endl;

      // update period or phase only when there are new incoming timestamps
      if ( find_lastest_timestamp( past_timestamps ) != latest_timestamp ) {
        auto updated_period_and_phase = update_period_and_phase(
          current_period, current_phase, past_timestamps, past_timestamps_eff_len, pairs_adjustments );
        current_period = get<0>( updated_period_and_phase );
        current_phase = get<1>( updated_period_and_phase );
      }

      // update latest_timestamp and proceed to determine position of the next beat
      latest_timestamp = find_lastest_timestamp( past_timestamps );

      // determine when the next beat is
      checkpoint_time = steady_clock::now();
      current_time_from_start = ( duration_cast<milliseconds>( checkpoint_time - begin_time ) ).count() / 1000.f;
      float next_beat_val = find_next_beat_val( current_period, current_phase, current_time_from_start );

      // make sure that we do not render double beats that are very close to each other
      if ( abs( next_beat_val - current_beat_val ) > 0
           and abs( next_beat_val - current_beat_val ) < current_period * 0.5 ) {
        next_beat_val = current_beat_val;
      }
      current_beat_val = next_beat_val;

      // compute time_to_next
      float time_to_next = current_time_from_start - next_beat_val;
      cout << latest_timestamp << "\t" << next_beat_val << "\t" << current_period << "\t" << current_phase << endl;

      // render metromone audio only when the latest beat has not been rendered
      if ( next_note_pred < steady_clock::now() ) {
        counter++;
        next_note_pred = checkpoint_time - round<milliseconds>( duration<float> { time_to_next } );
      }
    },
    [&] { return duration_cast<milliseconds>( steady_clock::now() - last_pred_time ).count() >= 50; } );

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
