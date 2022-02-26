#pragma once

#include "wav_wrapper.hh"
#include <vector>

// Current note_id structure: direction_velocity_note
using note_id = uint32_t;

struct sound
{
  uint8_t direction;
  uint8_t note;
  uint8_t velocity;
  unsigned long curr_offset;
};

class NoteRepository
{

  std::vector<WavWrapper> wav_files {};

  note_id get_note_idx( sound curr_sound );

public:
  NoteRepository();

  bool reached_end( sound curr_sound );
  wav_frame_t get_sample( sound curr_sound );
};