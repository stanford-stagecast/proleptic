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
  vector<float> samples_tmp( NUM_CHANNELS * num_frames_in_input );
  const auto retval = handle_.read( samples_tmp.data(), NUM_CHANNELS * num_frames_in_input );

  if ( retval != NUM_CHANNELS * num_frames_in_input ) {
    throw runtime_error( "unexpected read of " + to_string( retval ) + " samples" );
  }

  /* verify EOF */
  int16_t dummy;
  if ( 0 != handle_.read( &dummy, 1 ) ) {
    throw runtime_error( "unexpected extra data in WAV file" );
  }

  to_stereo( samples_tmp, samples_ );

  /* scale to avoid clipping */
  for ( auto& x : samples_ ) {
    x.first *= 0.2;
    x.second *= 0.2;
  }
}

void WavWrapper::to_stereo( const vector<float>& raw, vector<wav_frame_t>& stereo )
{
  if ( raw.size() % NUM_CHANNELS ) {
    throw runtime_error( "size not divisible by NUM_CHANNELS" );
  }
  stereo.resize( raw.size() / NUM_CHANNELS );
  for ( size_t i = 0; i < stereo.size(); i++ ) {
    stereo.at( i ) = { raw.at( NUM_CHANNELS * i ), raw.at( NUM_CHANNELS * i + 1 ) };
  }
}

void WavWrapper::bend_pitch( const double pitch_bend_ratio )
{
  vector<float> new_samples( samples_.size() * 2 ); /* hopefully enough samples */

  SRC_DATA resample_info;
  resample_info.data_in = &samples_.at( 0 ).first;
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

  to_stereo( new_samples, samples_ );
}
