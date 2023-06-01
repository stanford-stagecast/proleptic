#pragma once

#include <sndfile.hh>
#include <vector>

using wav_frame_t = std::pair<float, float>;

/* wrap WAV file with error/validity checks */
class WavWrapper
{
  std::vector<wav_frame_t> samples_ {};

  static void to_stereo( const std::vector<float>& raw, std::vector<wav_frame_t>& stereo );

public:
  WavWrapper( const std::string& filename );

  void resize( size_t N ) { samples_.resize( N ); }

  size_t size() const { return samples_.size(); };

  const std::vector<wav_frame_t>& samples() const { return samples_; }

  const wav_frame_t view( size_t offset ) const
  {
    return at_end( offset ) ? wav_frame_t {} : samples_.at( offset );
  }
  bool at_end( size_t offset ) const { return offset >= size(); }

  void bend_pitch( const double pitch_bend_ratio );

  /* can't copy or assign */
  WavWrapper( const WavWrapper& other ) = delete;
  WavWrapper& operator=( const WavWrapper& other ) = delete;

  /* ... but can move-construct */
  WavWrapper( WavWrapper&& other ) = default;
};
