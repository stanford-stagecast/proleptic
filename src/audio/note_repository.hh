#pragma once

#include "note_files.hh"
#include <vector>

class NoteRepository
{
  std::vector<NoteFiles> notes {};

  void add_notes( const std::string& name, const unsigned int num_notes, const bool has_damper = true );

public:
  NoteRepository();

  const wav_frame_t get_sample( const bool direction,
                                const size_t note,
                                const uint8_t velocity,
                                const unsigned long offset ) const;

  bool note_finished( const bool direction,
                      const size_t note,
                      const uint8_t velocity,
                      const unsigned long offset ) const;
};
