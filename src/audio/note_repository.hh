#pragma once

#include "note_files.hh"
#include <vector>

struct sound
{
  uint8_t direction;
  uint8_t note;
  uint8_t velocity;
  unsigned long curr_offset;
};

class NoteRepository
{
  std::vector<NoteFiles> notes {};

public:
  NoteRepository();

  const WavWrapper& get_wav( const uint8_t direction, const uint8_t note, const uint8_t velocity ) const;
};
