#pragma once

#include <sndfile.hh>
#include <vector>

using wav_frame_t = std::pair<float, float>;

/* wrap WAV file with error/validity checks */
class WavWrapper
{
  std::vector<float> samples_ {};

public:
  WavWrapper( const std::string& filename );

  wav_frame_t view( size_t offset ) const;
  bool at_end( size_t offset ) const;

  void bend_pitch( const double pitch_bend_ratio );

  /* can't copy or assign */
  WavWrapper( const WavWrapper& other ) = delete;
  WavWrapper& operator=( const WavWrapper& other ) = delete;

  /* ... but can move-construct */
  WavWrapper( WavWrapper&& other ) = default;
};
