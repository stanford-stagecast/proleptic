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
  size_t curr_offset;

public:
  WavWrapper( const string& filename );

  wav_frame_t view();

  void reset() { curr_offset = 0; };

  bool at_end();
};