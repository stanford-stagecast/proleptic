#include "midi_recording.hh"

using namespace std;

void program_body( const string& midi_recording_path )
{
  MidiRecording my_recording = MidiRecording( midi_recording_path );
  vector<MidiEvent> my_events = my_recording.events();
  for ( MidiEvent event : my_events )
    cout << event.time.value() << " " << event.event_type.value() << " " << event.key.value() << " "
         << event.velocity.value() << endl;
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc < 1 || argc > 2 ) {
    cerr << "Usage: " << argv[0] << " [midi_recording_path]\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
