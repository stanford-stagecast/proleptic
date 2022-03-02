#pragma once

#include <sndfile.hh>
#include <vector>

using wav_frame_t = std::pair<float, float>;

using namespace std;

/* wrap WAV file with error/validity checks */
class WavWrapper
{
  SndfileHandle handle_;
  vector<float> samples_;

public:
  WavWrapper( const string& filename );

  wav_frame_t view( size_t offset ) const;

  bool at_end( size_t offset ) const;
};
