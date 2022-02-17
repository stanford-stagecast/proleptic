#include "midi_processor.hh"

using namespace std;
using namespace chrono;

void MidiProcessor::pop_active_sense_bytes()
{
  while ( unprocessed_midi_bytes_.readable_region().size()
          and uint8_t( unprocessed_midi_bytes_.readable_region().at( 0 ) ) == 0xfe ) {
    unprocessed_midi_bytes_.pop( 1 );
  }
}

void MidiProcessor::read_from_fd( FileDescriptor& fd )
{
  unprocessed_midi_bytes_.push_from_fd( fd );

  pop_active_sense_bytes();

  last_event_time_ = steady_clock::now();
}

void MidiProcessor::pop_event()
{
  while ( unprocessed_midi_bytes_.readable_region().size() >= 3 ) {
    unprocessed_midi_bytes_.pop( 3 );
    pop_active_sense_bytes();
  }
}

bool MidiProcessor::data_timeout() const
{
  const auto now = steady_clock::now();

  return duration_cast<milliseconds>( now - last_event_time_ ).count() > 1000;
}
