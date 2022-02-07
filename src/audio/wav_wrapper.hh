#pragma once

#include <vector>
#include <sndfile.hh>



using wav_frame_t = std::pair<float, float>;

using namespace std;

/* wrap WAV file with error/validity checks */
class WavWrapper
{
    SndfileHandle handle_;
    vector<float> samples_;

public:
    WavWrapper( const string & filename );

    wav_frame_t view( const size_t offset );

    bool at_end( const size_t offset );

};