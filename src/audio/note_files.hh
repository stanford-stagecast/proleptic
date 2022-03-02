#pragma once

#include "wav_wrapper.hh"

class NoteFiles
{
  WavWrapper slow;
  WavWrapper med;
  WavWrapper fast;
  WavWrapper rel;

public:
  NoteFiles( const string& note, size_t rel_num );

  WavWrapper& getSlow() { return slow; };
  WavWrapper& getMed() { return med; };
  WavWrapper& getFast() { return fast; };
  WavWrapper& getRel() { return rel; };
  WavWrapper& getFileFromVel( uint8_t dir, uint8_t vel );
};
