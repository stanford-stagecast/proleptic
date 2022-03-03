#pragma once

#include "wav_wrapper.hh"

class NoteFiles
{
  WavWrapper slow;
  WavWrapper med;
  WavWrapper fast;
  WavWrapper rel;

public:
  NoteFiles( const string& note, const size_t key_num );

  const WavWrapper& getSlow() const { return slow; };
  const WavWrapper& getMed() const { return med; };
  const WavWrapper& getFast() const { return fast; };
  const WavWrapper& getRel() const { return rel; };
  const WavWrapper& getFileFromVel( uint8_t dir, uint8_t vel ) const;
};
