#pragma once

#include "audio_buffer.hh"

#include <sndfile.hh>
#include <string>

class WavWriter
{
  SndfileHandle handle_;

public:
  WavWriter( const std::string& path, const int sample_rate );

  // void write( const ChannelPair& buffer, const size_t range_end );

  void write_one( std::pair<float, float> sample );
};
