#pragma once

#include "note_files.hh"
#include <vector>

class NoteRepository
{
  std::vector<NoteFiles> notes {};

public:
  NoteRepository();

  const WavWrapper& get_wav( const uint8_t direction, const uint8_t note, const uint8_t velocity ) const;
};
