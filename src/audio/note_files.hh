#pragma once

#include "wav_wrapper.hh"

class NoteFiles
{
  WavWrapper slow;
  WavWrapper med;
  WavWrapper fast;
  WavWrapper rel;

  std::string slow_name;
  std::string med_name;
  std::string fast_name;

  bool has_damper_;

public:
  NoteFiles( const std::string& sample_directory,
             const std::string& note,
             const size_t key_num,
             const bool has_damper );

  const WavWrapper& getSlow() const { return slow; };
  const WavWrapper& getMed() const { return med; };
  const WavWrapper& getFast() const { return fast; };
  const WavWrapper& getRel() const { return rel; };

  bool has_damper() const { return has_damper_; }

  void bend_pitch( const double pitch_bend_ratio );
};
