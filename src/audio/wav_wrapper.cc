#include "wav_wrapper.hh"
#include "exception.hh"


const unsigned int SAMPLE_RATE = 48000; /* Hz */
const unsigned int NUM_CHANNELS = 2;
const unsigned int NUM_SAMPLES_IN_OPUS_FRAME = 960;
const unsigned int EXPECTED_LOOKAHEAD = 312; /* 6.5 ms * 48 kHz */
const unsigned int EXTRA_FRAMES_PREPENDED = 10;
const unsigned int OVERLAP_SAMPLES_PREPENDED = (EXTRA_FRAMES_PREPENDED + 1) * NUM_SAMPLES_IN_OPUS_FRAME - EXPECTED_LOOKAHEAD;
const unsigned int NUM_SAMPLES_IN_OUTPUT = 230400; /* 48kHz * 4.8s */
const unsigned int NUM_SAMPLES_IN_INPUT = OVERLAP_SAMPLES_PREPENDED + NUM_SAMPLES_IN_OUTPUT;

using namespace std;


WavWrapper::WavWrapper( const string & filename )
: handle_( filename ),
    samples_( NUM_CHANNELS * ( NUM_SAMPLES_IN_INPUT + EXPECTED_LOOKAHEAD ) )
{
    if ( handle_.error() ) {
        throw runtime_error( filename + ": " + handle_.strError() );
    }

    if ( handle_.format() != (SF_FORMAT_WAV | SF_FORMAT_PCM_16) ) {
        throw runtime_error( filename + ": not a 16-bit PCM WAV file" );
    }

    if ( handle_.samplerate() != SAMPLE_RATE ) {
        throw runtime_error( filename + " sample rate is " + to_string( handle_.samplerate() ) + ", not " + to_string( SAMPLE_RATE ) );
    }

    if ( handle_.channels() != NUM_CHANNELS ) {
        throw runtime_error( filename + " channel # is " + to_string( handle_.channels() ) + ", not " + to_string( NUM_CHANNELS ) );
    }

    if ( handle_.frames() != NUM_SAMPLES_IN_INPUT ) {
        throw runtime_error( filename + " length is " + to_string( handle_.frames() ) + ", not " + to_string( NUM_SAMPLES_IN_INPUT ) + " samples" );
    }

    /* read file into memory */
    const auto retval = handle_.read( samples_.data(), NUM_CHANNELS * NUM_SAMPLES_IN_INPUT );
    if ( retval != NUM_CHANNELS * NUM_SAMPLES_IN_INPUT ) {
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
    if ( offset > samples_.size() ) {
        throw out_of_range( "offset > samples_.size()" );
    }

    const size_t member_length = NUM_CHANNELS * NUM_SAMPLES_IN_OPUS_FRAME;

    if ( offset + member_length > samples_.size() ) {
        throw out_of_range( "offset + len > samples_.size()" );
    }

    /* second bounds check */
    int16_t first_sample __attribute((unused)) = samples_.at( offset );
    int16_t last_sample __attribute((unused)) = samples_.at( offset + member_length - 1 );

    return { samples_.data() + offset, member_length };
}
