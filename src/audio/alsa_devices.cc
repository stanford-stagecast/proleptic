#include <algorithm>
#include <alsa/asoundlib.h>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <tuple>

#include "alsa_devices.hh"
#include "exception.hh"
#include "timestamp.hh"

using namespace std;
using namespace std::chrono;

class alsa_error_category : public error_category
{
public:
  const char* name() const noexcept override { return "alsa_error_category"; }
  string message( const int return_value ) const noexcept override { return snd_strerror( return_value ); }
};

class alsa_error : public system_error
{
  string what_;

public:
  alsa_error( const string& context, const int err )
    : system_error( err, alsa_error_category() )
    , what_( context + ": " + system_error::what() )
  {}

  const char* what() const noexcept override { return what_.c_str(); }
};

int alsa_check( const char* context, const int return_value )
{
  if ( return_value >= 0 ) {
    return return_value;
  }
  throw alsa_error( context, return_value );
}

int alsa_check( const string& context, const int return_value )
{
  return alsa_check( context.c_str(), return_value );
}

#define alsa_check_easy( expr ) alsa_check( #expr, expr )

vector<ALSADevices::Device> ALSADevices::list()
{
  int card = -1;

  struct ALSA_hint_deleter
  {
    void operator()( void** x ) const { snd_device_name_free_hint( x ); }
  };

  struct free_deleter
  {
    void operator()( void* x ) const { free( x ); }
  };

  vector<Device> ret;

  while ( true ) {
    alsa_check_easy( snd_card_next( &card ) );
    if ( card < 0 ) {
      break;
    }

    ret.push_back( { "Audio" + to_string( card ), {} } );

    unique_ptr<void*, ALSA_hint_deleter> hints { [&] {
      void** hints_tmp;
      alsa_check_easy( snd_device_name_hint( card, "pcm", &hints_tmp ) );
      return hints_tmp;
    }() };

    for ( auto current_hint = hints.get(); *current_hint; ++current_hint ) {
      const unique_ptr<char, free_deleter> name { snd_device_name_get_hint( *current_hint, "NAME" ) };
      const unique_ptr<char, free_deleter> desc { snd_device_name_get_hint( *current_hint, "DESC" ) };

      if ( name and desc ) {
        const string_view name_str { name.get() }, desc_str { desc.get() };
        if ( name_str.substr( 0, 3 ) == "hw:"sv ) {
          const auto first_line = desc_str.substr( 0, desc_str.find_first_of( '\n' ) );
          ret.back().interfaces.emplace_back( name_str, first_line );
        }
      }
    }
  }

  return ret;
}

pair<string, string> ALSADevices::find_device( const vector<string_view> descriptions )
{
  ALSADevices devices;

  for ( const auto& dev : devices.list() ) {
    for ( const auto& interface : dev.interfaces ) {
      for ( const auto& description : descriptions ) {
        if ( interface.second.substr( 0, description.length() ) == description ) {
          cerr << "Found audio device: " << description << ": " << dev.name << "/" << interface.first << endl;
          return { dev.name, interface.first };
        }
      }
    }
  }

  throw runtime_error( "Audio device not found" );
}

AudioInterface::AudioInterface( const string_view interface_name,
                                const string_view annotation,
                                const snd_pcm_stream_t stream )
  : interface_name_( interface_name )
  , annotation_( annotation )
  , pcm_( nullptr )
  , fd_()
{
  const string diagnostic = "snd_pcm_open(" + name() + ")";
  alsa_check( diagnostic, snd_pcm_open( &pcm_, interface_name_.c_str(), stream, SND_PCM_NONBLOCK ) );
  notnull( diagnostic, pcm_ );

  check_state( SND_PCM_STATE_OPEN );

  fd_.emplace( dup_internal_fd() );
}

PCMFD AudioInterface::dup_internal_fd()
{
  const int count = alsa_check_easy( snd_pcm_poll_descriptors_count( pcm_ ) );
  if ( count < 1 or count > 2 ) {
    throw runtime_error( "unexpected fd count: " + to_string( count ) );
  }

  // XXX rely on "hw" driver behavior where PCM fd is always first

  pollfd pollfds[2];
  alsa_check_easy( snd_pcm_poll_descriptors( pcm_, pollfds, count ) );
  return PCMFD { CheckSystemCall( "dup AudioInterface fd", dup( pollfds[0].fd ) ) };
}

void AudioInterface::initialize()
{
  check_state( SND_PCM_STATE_OPEN );

  snd_pcm_uframes_t buffer_size;

  /* set desired hardware parameters */
  {
    struct params_deleter
    {
      void operator()( snd_pcm_hw_params_t* x ) const { snd_pcm_hw_params_free( x ); }
    };
    unique_ptr<snd_pcm_hw_params_t, params_deleter> params { [] {
      snd_pcm_hw_params_t* x = nullptr;
      snd_pcm_hw_params_malloc( &x );
      return notnull( "snd_pcm_hw_params_malloc", x );
    }() };

    alsa_check_easy( snd_pcm_hw_params_any( pcm_, params.get() ) );

    alsa_check_easy( snd_pcm_hw_params_set_rate_resample( pcm_, params.get(), false ) );
    alsa_check_easy( snd_pcm_hw_params_set_access( pcm_, params.get(), SND_PCM_ACCESS_MMAP_INTERLEAVED ) );
    alsa_check_easy( snd_pcm_hw_params_set_format( pcm_, params.get(), SND_PCM_FORMAT_S32_LE ) );
    alsa_check_easy( snd_pcm_hw_params_set_channels( pcm_, params.get(), 2 ) );
    alsa_check_easy( snd_pcm_hw_params_set_rate( pcm_, params.get(), config_.sample_rate, 0 ) );
    alsa_check_easy( snd_pcm_hw_params_set_period_size( pcm_, params.get(), config_.period_size, 0 ) );
    alsa_check_easy( snd_pcm_hw_params_set_buffer_size( pcm_, params.get(), config_.buffer_size ) );

    /* apply hardware parameters */
    alsa_check_easy( snd_pcm_hw_params( pcm_, params.get() ) );

    /* save buffer size */
    alsa_check_easy( snd_pcm_hw_params_get_buffer_size( params.get(), &buffer_size ) );
  }

  check_state( SND_PCM_STATE_PREPARED );

  /* set desired software parameters */
  {
    struct params_deleter
    {
      void operator()( snd_pcm_sw_params_t* x ) const { snd_pcm_sw_params_free( x ); }
    };
    unique_ptr<snd_pcm_sw_params_t, params_deleter> params { [] {
      snd_pcm_sw_params_t* x = nullptr;
      snd_pcm_sw_params_malloc( &x );
      return notnull( "snd_pcm_sw_params_malloc", x );
    }() };

    alsa_check_easy( snd_pcm_sw_params_current( pcm_, params.get() ) );

    alsa_check_easy( snd_pcm_sw_params_set_avail_min( pcm_, params.get(), config_.avail_minimum ) );
    alsa_check_easy( snd_pcm_sw_params_set_period_event( pcm_, params.get(), true ) );
    alsa_check_easy(
      snd_pcm_sw_params_set_start_threshold( pcm_, params.get(), numeric_limits<snd_pcm_uframes_t>::max() ) );
    alsa_check_easy( snd_pcm_sw_params_set_stop_threshold( pcm_, params.get(), buffer_size - 1 ) );

    alsa_check_easy( snd_pcm_sw_params( pcm_, params.get() ) );

    snd_pcm_uframes_t thresh;
    alsa_check_easy( snd_pcm_sw_params_get_stop_threshold( params.get(), &thresh ) );
  }

  check_state( SND_PCM_STATE_PREPARED );
}

bool AudioInterface::update()
{
  const auto ret = snd_pcm_avail_delay( pcm_, &avail_, &delay_ );

  if ( ret < 0 ) {
    //    cerr << name() << ": " << snd_strerror( ret ) << "\n";
    return true;
  }

  if ( avail_ < 0 or delay_ < 0 ) {
    throw runtime_error( "avail < 0 or delay < 0" );
  }

  if ( state() == SND_PCM_STATE_RUNNING ) {
    statistics_.min_delay = min( statistics_.min_delay, delay() );
  }

  return false;
}

void AudioInterface::recover()
{
  statistics_.recoveries++;
  statistics_.last_recovery = cursor();
  drop();
  prepare();
}

string AudioInterface::name() const
{
  return annotation_ + "[" + interface_name_ + "]";
}

AudioInterface::~AudioInterface()
{
  try {
    alsa_check( "snd_pcm_close(" + name() + ")", snd_pcm_close( pcm_ ) );
  } catch ( const exception& e ) {
    cerr << "Exception in destructor: " << e.what() << endl;
  }
}

snd_pcm_state_t AudioInterface::state() const
{
  return snd_pcm_state( pcm_ );
}

void AudioInterface::check_state( const snd_pcm_state_t expected_state )
{
  const auto actual_state = state();
  if ( expected_state != actual_state ) {
    snd_pcm_sframes_t avail, delay;
    snd_pcm_avail_delay( pcm_, &avail, &delay );
    throw runtime_error( name() + ": expected state " + snd_pcm_state_name( expected_state ) + " but state is "
                         + snd_pcm_state_name( actual_state ) + " with avail=" + to_string( avail )
                         + " and delay=" + to_string( delay ) );
  }
}

void AudioInterface::start()
{
  check_state( SND_PCM_STATE_PREPARED );
  alsa_check( "snd_pcm_start(" + name() + ")", snd_pcm_start( pcm_ ) );
  check_state( SND_PCM_STATE_RUNNING );
}

void AudioInterface::drop()
{
  alsa_check( "snd_pcm_drop(" + name() + ")", snd_pcm_drop( pcm_ ) );
  check_state( SND_PCM_STATE_SETUP );
}

void AudioInterface::prepare()
{
  check_state( SND_PCM_STATE_SETUP );
  alsa_check( "snd_pcm_prepare(" + name() + ")", snd_pcm_prepare( pcm_ ) );
  check_state( SND_PCM_STATE_PREPARED );
}

inline float sample_to_float( const int32_t sample )
{
  if ( sample & 0xff ) {
    throw runtime_error( "invalid sample: " + to_string( sample ) );
  }
  constexpr float maxval = uint64_t( 1 ) << 31;
  const float ret = sample / maxval;
  if ( ret > 1.0 or ret < -1.0 ) {
    throw runtime_error( "invalid sample: " + to_string( sample ) );
  }
  return ret;
}

inline int32_t float_to_sample( const float sample_f )
{
  constexpr float maxval = uint64_t( 1 ) << 31;
  return lrint( clamp( sample_f, -1.0f, 1.0f ) * maxval );
}

void AudioInterface::play( const size_t play_until_sample, const ChannelPair& playback_input )
{
  if ( update() ) {
    recover();
    fd_.value().register_write();
    return;
  }

  if ( play_until_sample <= cursor() ) {
    /* nothing to play */
    return;
  }

  const unsigned int samples_available_to_play = play_until_sample - cursor();

  Buffer write_buf { *this, samples_available_to_play };

  const unsigned int frames_to_play = write_buf.frame_count();

  if ( frames_to_play == 0 ) {
    throw runtime_error( "AudioInterface::play(): no available buffer space" );
  }

  for ( unsigned int i = 0; i < frames_to_play; i++ ) {
    /* play from input buffer + captured sample */
    const auto playback_sample = playback_input.safe_get( cursor_ );

    write_buf.sample( false, i ) = float_to_sample( playback_sample.first );
    write_buf.sample( true, i ) = float_to_sample( playback_sample.second );

    cursor_++;
  }

  write_buf.commit();

  if ( delay() + frames_to_play >= config_.start_threshold and state() == SND_PCM_STATE_PREPARED ) {
    start();
  }

  fd_.value().register_write();
}

AudioInterface::Buffer::Buffer( AudioInterface& interface, const unsigned int frames_requested )
  : pcm_( interface.pcm_ )
  , areas_( nullptr )
  , frame_count_( 0 )
  , offset_( 0 )
{
  /* XXX channel count must be 2 */
  snd_pcm_uframes_t frames_returned = frames_requested;
  alsa_check_easy( snd_pcm_mmap_begin( pcm_, &areas_, &offset_, &frames_returned ) );

  if ( frames_returned == 0 ) {
    throw runtime_error( interface.name() + ": could not get " + to_string( frames_requested ) + " frames, got "
                         + to_string( frames_returned ) + " instead" );
  }

  frame_count_ = frames_returned;

  if ( areas_[0].addr != areas_[1].addr ) {
    throw runtime_error( "non-interleaved areas returned" );
  }

  if ( areas_[0].first != 0 or areas_[1].first != 32 or areas_[0].step != 64 or areas_[1].step != 64 ) {
    throw runtime_error( "unexpected format or stride returned" );
  }
}

void AudioInterface::Buffer::commit( const unsigned int num_frames )
{
  if ( num_frames > frame_count_ ) {
    throw runtime_error( "Buffer::commit(): num_frames > frame_count_" );
  }

  if ( areas_ ) {
    if ( int( num_frames ) != alsa_check_easy( snd_pcm_mmap_commit( pcm_, offset_, num_frames ) ) ) {
      throw runtime_error( "short commit" );
    }

    areas_ = nullptr;
  }
}

AudioInterface::Buffer::~Buffer()
{
  if ( areas_ ) {
    cerr << "Warning, AudioInterface Buffer not committed before destruction.\n";

    try {
      commit();
    } catch ( const exception& e ) {
      cerr << "AudioInterface::Buffer destructor: " << e.what() << "\n";
    }
  }
}

void AudioInterface::summary( ostream& out ) const
{
  out << name() << " statistics: ";

  out << " wakeups=" << statistics().total_wakeups;

  out << " cursor=";
  pp_samples( out, cursor() );

  if ( statistics().min_delay < std::numeric_limits<unsigned int>::max() ) {
    out << " min_delay=" << statistics().min_delay << " samples";
  }

  if ( statistics().recoveries ) {
    out << " recoveries=" << statistics().recoveries;
  }

  if ( statistics().last_recovery ) {
    out << " last recovery=";
    pp_samples( out, cursor() - statistics().last_recovery );
  }

  out << "\n";
}
