#include "midi_processor.hh"
#include "eventloop.hh"
#include "exception.hh"
#include <fcntl.h>
#include <iostream>

using namespace std;
using namespace chrono;

void MidiProcessor::read_from_fd( FileDescriptor& fd )
{
  unprocessed_midi_bytes_.push_from_fd( fd );

  pop_active_sense_bytes();
  last_event_time_ = steady_clock::now();
}

float MidiProcessor::pop_event()
{
  while ( unprocessed_midi_bytes_.readable_region().size() >= 3 ) {
    unprocessed_midi_bytes_.pop( 3 );
    pop_active_sense_bytes();
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now()
                                                                - original_time_ )
           .count()
         / 1000.0;
}

void MidiProcessor::pop_active_sense_bytes()
{
  while ( unprocessed_midi_bytes_.readable_region().size()
          and uint8_t( unprocessed_midi_bytes_.readable_region().at( 0 ) ) == 0xfe ) {
    unprocessed_midi_bytes_.pop( 1 );
  }
}

/* create input vector from midi data */
std::queue<float> MidiProcessor::nn_midi_input( const string& midi_filename )
{
  FileDescriptor piano { CheckSystemCall( midi_filename, open( midi_filename.c_str(), O_RDONLY ) ) };
  reset_time();
  auto event_loop = make_shared<EventLoop>();
  size_t num_notes = 0;

  queue<float> ret_queue;

  /* rule #1: read events from MIDI piano */
  event_loop->add_rule( "read MIDI data", piano, Direction::In, [&] { read_from_fd( piano ); } );

  /* rule #2: add MIDI data to matrix */
  event_loop->add_rule(
    "synthesizer processes data",
    [&] {
      while ( has_event() ) {
        uint8_t event_type = get_event_type();
        float time_val = pop_event() / 1000.0;
        if ( event_type == 144 ) {
          ret_queue.push( time_val );
          cout << "time val: " << time_val << "\n";
          num_notes++;
        }
      }
    },
    /* when should this rule run? */
    [&] { return has_event(); } );

  while ( event_loop->wait_next_event( 5 ) != EventLoop::Result::Exit ) {
    if ( num_notes >= 16 )
      break;
  }

  return ret_queue;
}

bool MidiProcessor::data_timeout() const
{
  const auto now = steady_clock::now();

  return duration_cast<milliseconds>( now - last_event_time_ ).count() > 1000;
}
