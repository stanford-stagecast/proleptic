#include "wav_wrapper.hh"
#include "exception.hh"

#include <iostream>
#include <samplerate.h>

#include <fstream>

constexpr unsigned int SAMPLE_RATE = 48000; /* Hz */
constexpr unsigned int NUM_CHANNELS = 2;

#include <algorithm>

using namespace std;

WavWrapper::WavWrapper( const string& filename )
{
  SndfileHandle handle_ { filename };

  const unsigned int num_frames_in_input = handle_.frames();
  samples_.resize( NUM_CHANNELS * num_frames_in_input );

  if ( handle_.error() ) {
    throw runtime_error( filename + ": " + handle_.strError() );
  }

  if ( handle_.format() != ( SF_FORMAT_WAV | SF_FORMAT_PCM_24 ) ) {
    throw runtime_error( filename + ": not a 24-bit WAV file" );
  }

  if ( handle_.samplerate() != SAMPLE_RATE ) {
    throw runtime_error( filename + " sample rate is " + to_string( handle_.samplerate() ) + ", not "
                         + to_string( SAMPLE_RATE ) );
  }

  if ( handle_.channels() != NUM_CHANNELS ) {
    throw runtime_error( filename + " channel # is " + to_string( handle_.channels() ) + ", not "
                         + to_string( NUM_CHANNELS ) );
  }

  /* read file into memory */
  const auto retval = handle_.read( samples_.data(), NUM_CHANNELS * num_frames_in_input );

  if ( retval != NUM_CHANNELS * num_frames_in_input ) {
    throw runtime_error( "unexpected read of " + to_string( retval ) + " samples" );
  }


  /* verify EOF */
  int16_t dummy;
  if ( 0 != handle_.read( &dummy, 1 ) ) {
    throw runtime_error( "unexpected extra data in WAV file" );
  }
}

bool WavWrapper::at_end( size_t offset ) const
{
  if ( offset >= samples_.size() / 2 ) {
    return true;
  }

  return false;
}

wav_frame_t WavWrapper::view( size_t offset ) const
{
  if ( at_end( offset ) ) {
    return { 0, 0 };
  }
  std::pair<float, float> sample = { samples_.at( offset * 2 ), samples_.at( ( offset * 2 ) + 1 ) };

  return sample;
}

void WavWrapper::bend_pitch( const double pitch_bend_ratio )
{
  vector<float> new_samples( samples_.size() * 2 ); /* hopefully enough samples */

  SRC_DATA resample_info;
  resample_info.data_in = samples_.data();
  resample_info.data_out = new_samples.data();
  resample_info.input_frames = samples_.size() / NUM_CHANNELS;
  resample_info.output_frames = new_samples.size() / NUM_CHANNELS;
  resample_info.src_ratio = pitch_bend_ratio;

  const int ret = src_simple( &resample_info, SRC_SINC_FASTEST, NUM_CHANNELS );
  if ( ret ) {
    throw runtime_error( "libsamplerate src_simple exception: "s + src_strerror( ret ) );
    /* XXX should use an error category */
  }

  new_samples.resize( NUM_CHANNELS * resample_info.output_frames_gen );

  samples_.swap( new_samples );

}
