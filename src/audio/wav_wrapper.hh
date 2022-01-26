#pragma once

#include <vector>
#include <sndfile.hh>



using wav_frame_t = std::basic_string_view<int16_t>;

using namespace std;

/* wrap WAV file with error/validity checks */
class WavWrapper
{
    SndfileHandle handle_;
    vector<int16_t> samples_;

public:
    WavWrapper( const string & filename );

    wav_frame_t view( const size_t offset );

};