#include "note_repository.hh"
#include "wav_wrapper.hh"

#include <cmath>
#include <iostream>

using namespace std;

constexpr float LOW_XFOUT_LOVEL = 8;   // Equivalent to MED_XFIN_LOVEL
constexpr float LOW_XFOUT_HIVEL = 59;  // Equivalent to MED_XFIN_HIVEL
constexpr float HIGH_XFIN_LOVEL = 67;  // Equivalent to MED_XFOUT_LOVEL
constexpr float HIGH_XFIN_HIVEL = 119; // Equivalent to MED_XFOUT_HIVEL

NoteRepository::NoteRepository( const string& sample_directory )
{
  add_notes( sample_directory, "A0" );
  add_notes( sample_directory, "U-A0" );
  add_notes( sample_directory, "D-C1" );
  add_notes( sample_directory, "C1" );
  add_notes( sample_directory, "U-C1" );
  add_notes( sample_directory, "D-D#1" );
  add_notes( sample_directory, "D#1" );
  add_notes( sample_directory, "U-D#1" );
  add_notes( sample_directory, "D-F#1" );
  add_notes( sample_directory, "F#1" );
  add_notes( sample_directory, "U-F#1" );
  add_notes( sample_directory, "D-A1" );
  add_notes( sample_directory, "A1" );
  add_notes( sample_directory, "U-A1" );
  add_notes( sample_directory, "D-C2" );
  add_notes( sample_directory, "C2" );
  add_notes( sample_directory, "U-C2" );
  add_notes( sample_directory, "D-D#2" );
  add_notes( sample_directory, "D#2" );
  add_notes( sample_directory, "U-D#2" );
  add_notes( sample_directory, "D-F#2" );
  add_notes( sample_directory, "F#2" );
  add_notes( sample_directory, "U-F#2" );
  add_notes( sample_directory, "D-A2" );
  add_notes( sample_directory, "A2" );
  add_notes( sample_directory, "U-A2" );
  add_notes( sample_directory, "D-C3" );
  add_notes( sample_directory, "C3" );
  add_notes( sample_directory, "U-C3" );
  add_notes( sample_directory, "D-D#3" );
  add_notes( sample_directory, "D#3" );
  add_notes( sample_directory, "U-D#3" );
  add_notes( sample_directory, "D-F#3" );
  add_notes( sample_directory, "F#3" );
  add_notes( sample_directory, "U-F#3" );
  add_notes( sample_directory, "D-A3" );
  add_notes( sample_directory, "A3" );
  add_notes( sample_directory, "U-A3" );
  add_notes( sample_directory, "D-C4" );
  add_notes( sample_directory, "C4" );
  add_notes( sample_directory, "U-C4" );
  add_notes( sample_directory, "D-D#4" );
  add_notes( sample_directory, "D#4" );
  add_notes( sample_directory, "U-D#4" );
  add_notes( sample_directory, "D-F#4" );
  add_notes( sample_directory, "F#4" );
  add_notes( sample_directory, "U-F#4" );
  add_notes( sample_directory, "D-A4" );
  add_notes( sample_directory, "A4" );
  add_notes( sample_directory, "U-A4" );
  add_notes( sample_directory, "D-C5" );
  add_notes( sample_directory, "C5" );
  add_notes( sample_directory, "U-C5" );
  add_notes( sample_directory, "D-D#5" );
  add_notes( sample_directory, "D#5" );
  add_notes( sample_directory, "U-D#5" );
  add_notes( sample_directory, "D-F#5" );
  add_notes( sample_directory, "F#5" );
  add_notes( sample_directory, "U-F#5" );
  add_notes( sample_directory, "D-A5" );
  add_notes( sample_directory, "A5" );
  add_notes( sample_directory, "U-A5" );
  add_notes( sample_directory, "D-C6" );
  add_notes( sample_directory, "C6" );
  add_notes( sample_directory, "U-C6" );
  add_notes( sample_directory, "D-D#6" );
  add_notes( sample_directory, "D#6" );
  add_notes( sample_directory, "U-D#6" );
  // keys below here do not have dampers
  add_notes( sample_directory, "D-F#6", false );
  add_notes( sample_directory, "F#6", false );
  add_notes( sample_directory, "U-F#6", false );
  add_notes( sample_directory, "D-A6", false );
  add_notes( sample_directory, "A6", false );
  add_notes( sample_directory, "U-A6", false );
  add_notes( sample_directory, "D-C7", false );
  add_notes( sample_directory, "C7", false );
  add_notes( sample_directory, "U-C7", false );
  add_notes( sample_directory, "D-D#7", false );
  add_notes( sample_directory, "D#7", false );
  add_notes( sample_directory, "U-D#7", false );
  add_notes( sample_directory, "D-F#7", false );
  add_notes( sample_directory, "F#7", false );
  add_notes( sample_directory, "U-F#7", false );
  add_notes( sample_directory, "D-A7", false );
  add_notes( sample_directory, "A7", false );
  add_notes( sample_directory, "U-A7", false );
  add_notes( sample_directory, "D-C8", false );
  add_notes( sample_directory, "C8", false );

  cerr << "Added " << notes.size() << " notes\n";
}

const std::vector<wav_frame_t> NoteRepository::get_wav( const bool direction,
                                                        const size_t note,
                                                        const uint8_t velocity ) const
{
  if ( direction ) {
    if ( velocity <= LOW_XFOUT_LOVEL ) {
      const WavWrapper& wav = notes.at( note ).getSlow();
      std::vector<wav_frame_t> samples { wav.size() };
      for ( size_t i = 0; i < wav.size(); i++ ) {
        std::pair<float, float> new_samp = wav.view( i );
        samples[i] = { new_samp.first, new_samp.second };
      }
      return samples;
    } else if ( velocity <= LOW_XFOUT_HIVEL ) {
      const WavWrapper& wav = notes.at( note ).getSlow();
      const WavWrapper& wav_med = notes.at( note ).getMed();
      std::vector<wav_frame_t> samples { wav_med.size() };
      size_t i;
      for ( i = 0; i < wav.size(); i++ ) {
        std::pair<float, float> new_samp = wav.view( i );
        new_samp.first *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );
        new_samp.second *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );

        std::pair<float, float> med_samp = wav_med.view( i );
        new_samp.first
          += med_samp.first * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        new_samp.second
          += med_samp.second * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        samples[i] = { new_samp.first, new_samp.second };
      }

      while ( i < wav_med.size() ) {
        std::pair<float, float> new_samp = wav_med.view( i );
        new_samp.first
          = new_samp.first * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        new_samp.second
          = new_samp.second * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        samples[i] = { new_samp.first, new_samp.second };
        i++;
      }

      return samples;
    } else if ( velocity <= HIGH_XFIN_LOVEL ) {
      const WavWrapper& wav = notes.at( note ).getMed();
      std::vector<wav_frame_t> samples { wav.size() };
      for ( size_t i = 0; i < wav.size(); i++ ) {
        std::pair<float, float> new_samp = wav.view( i );
        samples[i] = { new_samp.first, new_samp.second };
      }
      return samples;
    } else if ( velocity <= HIGH_XFIN_HIVEL ) {
      const WavWrapper& wav = notes.at( note ).getMed();
      const WavWrapper& wav_fast = notes.at( note ).getFast();
      std::vector<wav_frame_t> samples { wav_fast.size() };
      size_t i;
      for ( i = 0; i < wav.size(); i++ ) {
        std::pair<float, float> new_samp = wav.view( i );
        new_samp.first *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );
        new_samp.second *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );

        std::pair<float, float> fast_samp = wav_fast.view( i );
        new_samp.first
          += fast_samp.first * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        new_samp.second
          += fast_samp.second * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        samples[i] = { new_samp.first, new_samp.second };
      }

      while ( i < wav_fast.size() ) {
        std::pair<float, float> new_samp = wav_fast.view( i );
        new_samp.first
          = new_samp.first * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        new_samp.second
          = new_samp.second * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
        samples[i] = { new_samp.first, new_samp.second };
        i++;
      }

      return samples;
    } else {
      const WavWrapper& wav = notes.at( note ).getFast();
      std::vector<wav_frame_t> samples { wav.size() };
      for ( size_t i = 0; i < wav.size(); i++ ) {
        std::pair<float, float> new_samp = wav.view( i );
        samples[i] = { new_samp.first, new_samp.second };
      }
      return samples;
    }
  }

  const WavWrapper& wav = notes.at( note ).getRel();
  std::vector<wav_frame_t> samples { wav.size() };
  for ( size_t i = 0; i < wav.size(); i++ ) {
    std::pair<float, float> new_samp = wav.view( i );
    samples[i] = { new_samp.first, new_samp.second };
  }
  return samples;
}

const wav_frame_t NoteRepository::get_sample( const bool direction,
                                              const size_t note,
                                              const uint8_t velocity,
                                              const unsigned long offset ) const
{
  if ( direction ) {
    if ( velocity <= LOW_XFOUT_LOVEL ) {
      return notes.at( note ).getSlow().view( offset );
    } else if ( velocity <= LOW_XFOUT_HIVEL ) {
      std::pair<float, float> new_samp = notes.at( note ).getSlow().view( offset );
      new_samp.first *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );
      new_samp.second *= ( LOW_XFOUT_HIVEL - velocity ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL );

      std::pair<float, float> med_samp = notes.at( note ).getMed().view( offset );
      new_samp.first += med_samp.first * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );
      new_samp.second
        += med_samp.second * ( ( velocity - LOW_XFOUT_LOVEL ) / ( LOW_XFOUT_HIVEL - LOW_XFOUT_LOVEL ) );

      return new_samp;
    } else if ( velocity <= HIGH_XFIN_LOVEL ) {
      return notes.at( note ).getMed().view( offset );
    } else if ( velocity <= HIGH_XFIN_HIVEL ) {
      std::pair<float, float> new_samp = notes.at( note ).getMed().view( offset );
      new_samp.first *= ( HIGH_XFIN_HIVEL - velocity ) / ( HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL );
      new_samp.second *= ( HIGH_XFIN_HIVEL - velocity ) / ( HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL );

      std::pair<float, float> fast_samp = notes.at( note ).getFast().view( offset );
      new_samp.first
        += fast_samp.first * ( ( velocity - HIGH_XFIN_LOVEL ) / ( HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL ) );
      new_samp.second
        += fast_samp.second * ( ( velocity - HIGH_XFIN_LOVEL ) / ( HIGH_XFIN_HIVEL - HIGH_XFIN_LOVEL ) );

      return new_samp;
    } else {
      return notes.at( note ).getFast().view( offset );
    }
  }

  return notes.at( note ).getRel().view( offset );
}

bool NoteRepository::note_finished( const bool direction,
                                    const size_t note,
                                    const uint8_t velocity,
                                    const unsigned long offset ) const
{
  if ( direction ) {
    if ( velocity <= LOW_XFOUT_LOVEL ) {
      return notes.at( note ).getSlow().at_end( offset );
    } else if ( velocity <= LOW_XFOUT_HIVEL ) {
      return notes.at( note ).getMed().at_end( offset );
    } else if ( velocity <= HIGH_XFIN_LOVEL ) {
      return notes.at( note ).getMed().at_end( offset );
    } else if ( velocity <= HIGH_XFIN_HIVEL ) {
      return notes.at( note ).getFast().at_end( offset );
    } else {
      return notes.at( note ).getFast().at_end( offset );
    }
  }

  return notes.at( note ).getRel().at_end( offset );
}

void NoteRepository::add_notes( const string& sample_directory, const string& name, const bool has_damper )
{
  unsigned int release_sample_num = notes.size() + 1;
  notes.emplace_back( sample_directory, name, release_sample_num, has_damper );
}
