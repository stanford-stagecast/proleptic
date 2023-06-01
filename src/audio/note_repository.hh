#pragma once

#include "note_files.hh"
#include <vector>

class NoteRepository
{
  std::vector<NoteFiles> notes {};

  void add_notes( const std::string& sample_directory, const std::string& name, const bool has_damper = true );

public:
  struct WavCombination
  {
    const std::vector<wav_frame_t>* a {};
    const std::vector<wav_frame_t>* b {};
    float a_weight {}, b_weight {};
  };

  NoteRepository( const std::string& sample_directory );

  const std::vector<wav_frame_t> get_wav( const bool direction, const size_t note, const uint8_t velocity ) const;

  WavCombination get_keydown_combination( const size_t note, const uint8_t velocity ) const;

  bool note_finished( const bool direction,
                      const size_t note,
                      const uint8_t velocity,
                      const unsigned long offset ) const;
};
