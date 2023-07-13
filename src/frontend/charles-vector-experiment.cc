#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>
#include <unordered_map>

#include "timer.hh"

using namespace std;


static constexpr uint8_t NUM_KEYS = 88;

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

  vector<MidiEvent> get_events(){
    return events_;
  }
};

class MatchFinder{

  private:
    std::array<std::unordered_map<int, std::string>, 88> storage_; //store 88 individual keys
  
  public:
    
    void find_next_note(vector<Chunk> chunks){ //go through each chunk, and add the next UNIQUE note
        for (auto& chunk: chunks){
          vector<MidiEvent> current_chunk = chunk.get_events();
          for( auto& ev: current_chunk){ //each event in the current chunk
            //find each unique note and add next note in storage_
            int prev_key;

          }
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

  MatchFinder mf;
  //mf.find_next_note();
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
