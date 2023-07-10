#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unordered_map>
#include <chrono>

#include "exception.hh"
#include "file_descriptor.hh"
#include "midi_processor.hh"
#include "timer.hh"

using namespace std;
using namespace std::chrono;

struct MidiEvent
{
  unsigned short type, note, velocity; // actual midi data
};

class MatchFinder
{
public:
  void process_events( uint64_t starting_ts, uint64_t ending_ts, const vector<MidiEvent>& events )
  {
    // Dummy implementation. Eventually this will need to:
    // (1) store the event in a data structure (maybe many data structures)
    // (2) try to find similar passages in the past
    // (3) time how long all this takes (ideally <1 millisecond of wall-clock time)
    //     [can use the Timer::timestamp_ns() function to measure how long things take IRL]


    
  
    //cout << "Processing chunk from " << starting_ts << ".." << ending_ts << " ms:";

    
    
    for ( const auto& ev : events ) {
      //cout << " [" << ev.type << " " << ev.note << " " << ev.velocity << "]";
    }
    if ( events.empty() ) {
      //cout << " (none)";
    }
    //cout << "\n";
    
  }


};

class Chunk{
  public:
    std::vector<MidiEvent> events_in_chunk;

    void add_event_in_chunk(MidiEvent new_event){
      events_in_chunk.push_back(new_event);
    }

    vector<MidiEvent> get_vector(){
      return events_in_chunk;
    }

    void clear_chunk(){
      events_in_chunk.clear();
    }

    void print_chunk(){
      for(const auto& ev: events_in_chunk){
        cout << "[" << ev.note << "] [" << ev.type << "] [" << ev.velocity << "]" << endl;
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

  vector<MidiEvent> events_in_chunk;
  uint64_t end_of_chunk = chunk_duration_ms;
  MatchFinder match_finder;

  uint64_t total_time;

  //create hashmap to store 5ms chunks
  std::vector<Chunk> chunks;

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event
    Chunk current_chunk;

    clock_t start, end;

    start = clock();
    
    while ( timestamp >= end_of_chunk ) {
      match_finder.process_events( end_of_chunk - chunk_duration_ms, end_of_chunk, events_in_chunk );
      for ( const auto& ev: events_in_chunk){ //for loop adds all event in 5ms window to current chunk
        current_chunk.add_event_in_chunk(ev);
      }
      
      events_in_chunk.clear();
      end_of_chunk += chunk_duration_ms;
    }
  
    events_in_chunk.push_back( move( ev ) );
    

    
    chunks.push_back(current_chunk);
    
    cout << "*************" << endl;
    cout << "Beginning of chunk" << endl;
    current_chunk.print_chunk();
    cout << "end of chunk" << endl;
    
    
    
    current_chunk.clear_chunk();
    end = clock();
    double time_taken_ms = (double(end - start) / double(CLOCKS_PER_SEC)) * 1000;
    cout << "time taken in milliseconds : " << fixed << time_taken_ms << setprecision(5) << endl;
    cout << "*************" << endl;
  }

  /*
  auto avg_time = total_time / chunks.size();
  cout << "Size of chunks vector" << chunks.size() << endl;
  8?
  //cout << "Avg time to store each 5ms chunk: " << avg_time << endl;

  /*
   * TO DO: Have code that runs every 5 ms (instead of sleep until next event, sleep until 5 ms from now.)
   * Upon waking up we look at all the events that occurred since last wakeup (could be empty).
   * Call fn with that set of events (could be empty). Fn will be responsible for:
   * 1. Store data in some data struct
   * 2. Find most similar part of history (if any).
   */
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
