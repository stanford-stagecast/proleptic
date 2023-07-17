#include <array>
#include <cstdint>
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

static constexpr uint8_t PIANO_OFFSET = 21;
static constexpr uint8_t NUM_KEYS = 88;
static constexpr uint64_t chunk_duration_ms = 5;
static constexpr uint8_t KEYDOWN_TYPE = 0x90;

struct TimingStats
{
  uint64_t max_ns {};   // max time taken to process an event
  uint64_t total_ns {}; // total processing time (to calc avg time)
  uint64_t count {};    // total events processed

  optional<uint64_t> start_time {};

  void start_timer()
  {
    if ( start_time.has_value() ) {
      throw runtime_error( "timer started when already running" );
    }
    start_time = Timer::timestamp_ns(); // start the timer
  }

  void stop_timer()
  {
    if ( not start_time.has_value() ) {
      throw runtime_error( "timer stopped when not running" );
    }
    const uint64_t time_elapsed = Timer::timestamp_ns() - start_time.value();
    max_ns = max( time_elapsed, max_ns );
    total_ns += time_elapsed;
    count++;
    start_time.reset();
  }
};

class MatchFinder
{
  array<vector<unsigned short>, NUM_KEYS> storage_;
  unsigned short prev_note;
  bool first_note = true;

  TimingStats timing_stats_;

public:
  void process_events( const vector<MidiEvent>& events )
  {
    for ( const auto& ev : events ) {
      if ( ev.type != KEYDOWN_TYPE ) { // only keydowns represent new notes
        continue;
      }

      timing_stats_.start_timer();

      if ( !first_note ) {
        vector<unsigned short>& curr_note
          = storage_[prev_note - PIANO_OFFSET]; // curr_note follows stored prev_note
        if ( std::find( curr_note.begin(), curr_note.end(), ev.note )
             == curr_note.end() ) {       // if curr_note not already listed,
          curr_note.push_back( ev.note ); // add it
        }
      } else {
        first_note = false; // first note can't possibly follow a note
      }
      prev_note = ev.note;

      timing_stats_.stop_timer();
    }
  };

  void print_stats() const
  {
    cout << "Timing stats:\n";
    cout << "  total events: " << timing_stats_.count << "\n";
    cout << "  max time:     ";
    Timer::pp_ns( cout, timing_stats_.max_ns );
    cout << "\n";
    cout << "  average time: ";
    Timer::pp_ns( cout, timing_stats_.total_ns / timing_stats_.count );
    cout << "\n\n";
  }

  void print_predict() const
  {
    cout << "Enter a note from 21(A0) to 108(C8): ";
    unsigned short val;
    cin >> val;
    for ( const auto& note : storage_[val - PIANO_OFFSET] ) {
      cout << note << " ";
    }
    cout << "\n";
  }
};

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

  while ( not midi_data.eof() ) { // until file reaches end
    if ( not midi_data.good() ) {
      throw runtime_error( midi_filename + " could not be parsed" );
    }

    MidiEvent ev;
    uint64_t timestamp;
    midi_data >> timestamp >> ev.type >> ev.note >> ev.velocity; // reads in data to event

    while ( timestamp >= end_of_chunk ) {
      match_finder.process_events( events_in_chunk );
      events_in_chunk.clear();
      end_of_chunk += chunk_duration_ms;
    }
    events_in_chunk.push_back( std::move( ev ) );
  }
  match_finder.print_stats();
  match_finder.print_predict();
  /*
   * TO DO:
   * Each time that MatchFinder::process_events is called for a KeyDown, it should find ALL times
   * that the same key was KeyDowned in the past, and for all of THOSE times where there was any
   * subsequent KeyDown, it should make a list of all the UNIQUE keys that came immediately after.
   * This could be an empty list (if it's the first time this key has been seen in a KeyDown),
   * or it could be a list of up to 88 entries (it could have preceded every other key going down
   * at some point in the past).
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