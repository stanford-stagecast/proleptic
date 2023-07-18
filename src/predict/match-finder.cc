#include <iostream>

#include "match-finder.hh"

using namespace std;

static constexpr uint8_t KEYDOWN_TYPE = 0x90;

void MatchFinder::process_events( const vector<MidiEvent>& events )
{
  for ( const auto& ev : events ) {
    if ( ev.type != KEYDOWN_TYPE ) { // only keydowns represent new notes
      continue;
    }

    timing_stats_.start_timer();

    if ( !first_note ) {
      vector<unsigned short>& curr_note = storage_[prev_note - PIANO_OFFSET]; // curr_note follows stored prev_note
      curr_note.push_back( ev.note );                                         // add it
    } else {
      first_note = false; // first note can't possibly follow a note
    }
    prev_note = ev.note;

    timing_stats_.stop_timer();
  }
}

void MatchFinder::print_stats() const
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
