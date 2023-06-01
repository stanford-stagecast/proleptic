#pragma once

#include "wav_wrapper.hh"

class NoteFiles
{
  WavWrapper slow;
  WavWrapper med;
  WavWrapper fast;
  WavWrapper rel;

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

  WavWrapper& getSlow() { return slow; };
  WavWrapper& getMed() { return med; };
  WavWrapper& getFast() { return fast; };
  WavWrapper& getRel() { return rel; };

  bool has_damper() const { return has_damper_; }

  void bend_pitch( const double pitch_bend_ratio );
};
