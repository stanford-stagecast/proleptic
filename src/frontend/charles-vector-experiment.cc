#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#include "timer.hh"

using namespace std;

struct MidiEvent
{
  unsigned short type, note, velocity; // actual midi data
};

class Chunk
{
  std::vector<MidiEvent> events_;

public:
  void add( const MidiEvent& new_event ) { events_.push_back( new_event ); }

  void clear() { events_.clear(); }

  void print() const
  {
    if ( events_.empty() ) {
      cout << "(empty chunk)\n";
      return;
    }

    for ( const auto& ev : events_ ) {
      cout << "[" << ev.note << "] [" << ev.type << "] [" << ev.velocity << "]\n";
    }
  }
};

static constexpr uint64_t chunk_duration_ms = 5;

void program_body( const string& midi_filename )
{
  /* speed up C++ I/O by decoupling from C standard I/O */
  ios::sync_with_stdio( false );

  /* read MIDI file */
  ifstream midi_data { midi_filename }; // opens midi_filename given
  midi_data.unsetf( ios::dec );
  midi_data.unsetf( ios::oct );
  midi_data.unsetf( ios::hex );

  uint64_t end_of_chunk = chunk_duration_ms;

  // create vector to store 5ms chunks
  std::vector<Chunk> chunks;
  chunks.emplace_back(); // put current chunk on the vector

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event

    while ( timestamp >= end_of_chunk ) {
      chunks.emplace_back();
      end_of_chunk += chunk_duration_ms;
    }

    chunks.back().add( ev );
  }
  cout << "*************" << endl;

  for ( const auto& chunk : chunks ) {
    cout << "Here is a chunk: ";
    chunk.print();
    cout << "\n";
  }
}

void usage_message( const string_view argv0 )
{
  cerr << "Usage: " << argv0 << " midi_filename\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      usage_message( argv[0] );
      return EXIT_FAILURE;
    }

    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
