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

  bool reached_end( sound curr_sound );
  wav_frame_t get_sample( sound curr_sound );
};
