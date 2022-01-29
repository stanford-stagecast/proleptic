#include "wav_wrapper.hh"
#include "exception.hh"

#include <iostream>

const unsigned int SAMPLE_RATE = 48000; /* Hz */
const unsigned int NUM_CHANNELS = 2;

using namespace std;


WavWrapper::WavWrapper( const string & filename )
: handle_( filename ),
    samples_( 1 )
{
    const unsigned int num_frames_in_input = handle_.frames(); 
    samples_.resize( NUM_CHANNELS * num_frames_in_input );

    if ( handle_.error() ) {
        throw runtime_error( filename + ": " + handle_.strError() );
    }

    if ( handle_.format() != (SF_FORMAT_WAV | SF_FORMAT_PCM_24) ) {
        throw runtime_error( filename + ": not a 24-bit WAV file" );
    }

    if ( handle_.samplerate() != SAMPLE_RATE ) {
        throw runtime_error( filename + " sample rate is " + to_string( handle_.samplerate() ) + ", not " + to_string( SAMPLE_RATE ) );
    }

    if ( handle_.channels() != NUM_CHANNELS ) {
        throw runtime_error( filename + " channel # is " + to_string( handle_.channels() ) + ", not " + to_string( NUM_CHANNELS ) );
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

wav_frame_t WavWrapper::view( const size_t offset )
{
    if ( offset > samples_.size()/2 ) {
        throw out_of_range( "offset > samples_.size()/2" );
    }


    return {samples_.at(offset*2),  samples_.at((offset*2) + 1)};
}
