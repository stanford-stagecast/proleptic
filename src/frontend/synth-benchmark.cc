#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "synthesizer.hh"
#include "timer.hh"

using namespace std;

void program_body( const string& sample_directory )
{
  optional<Synthesizer> synth;
  {
    GlobalScopeTimer<Timer::Category::InitSynth> timer;
    synth.emplace( sample_directory );
  }

  for ( unsigned int i = 0; i < 64; i++ ) {
    {
      GlobalScopeTimer<Timer::Category::KeyDown> timer;
      synth->add_key_press( i + 22, 80 );
    }

    {
      GlobalScopeTimer<Timer::Category::AdvanceSample> timer;
      synth->advance_sample();
    }
  }

  for ( unsigned int i = 0; i < 64; i++ ) {
    {
      GlobalScopeTimer<Timer::Category::GetWav> timer;
      synth->add_shallow_key_press( i + 22, 1 );
    }

    {
      GlobalScopeTimer<Timer::Category::GetWav> timer;
      synth->add_shallow_key_press( i + 22 + 1, 1 );
    }

    {
      GlobalScopeTimer<Timer::Category::AdvanceSample> timer;
      synth->advance_sample();
    }
  }

  for ( unsigned int i = 0; i < 64; i++ ) {
    {
      GlobalScopeTimer<Timer::Category::KeyUp> timer;
      synth->add_key_release( i + 22, 80 );
    }

    {
      GlobalScopeTimer<Timer::Category::AdvanceSample> timer;
      synth->advance_sample();
    }
  }

  global_timer().summary( cout );
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " sample_directory\n";
    return EXIT_FAILURE;
  }

  program_body( argv[1] );

  return EXIT_SUCCESS;
}
