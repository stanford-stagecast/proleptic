#pragma once

#include <algorithm>
#include <array>
#include <assert.h>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

struct MidiEvent
{
  enum EventType
  {
    NoteOff = 0x80,
    NoteOn = 0x90,
    PolyphonicKeyPressure = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    ChannelPressure = 0xD0,
    PitchWheelChange = 0xE0,
    SystemExclusive = 0xF0,
    MetaEvent = 0xFF,
  };

  std::optional<EventType> event_type = std::nullopt;
  std::optional<unsigned int> time = std::nullopt;
  std::optional<unsigned int> key = std::nullopt;
  std::optional<unsigned int> velocity = std::nullopt;

  template<typename Out>
  void split( const std::string& s, char delim, Out result )
  {
    std::istringstream iss( s );
    std::string item;
    while ( std::getline( iss, item, delim ) ) {
      *result++ = item;
    }
  }

  std::vector<std::string> split( std::string& s, char delim )
  {
    std::vector<std::string> elems;
    split( s, delim, std::back_inserter( elems ) );
    return elems;
  }

  unsigned int hex_string_to_unsigned_int( std::string event_info )
  {
    unsigned int event_info_int;
    std::stringstream ss;
    ss << std::hex << event_info;
    ss >> event_info_int;
    return event_info_int;
  }

  MidiEvent( std::string event_info_line )
  {
    std::vector<std::string> event_info = split( event_info_line, ' ' );
    time = std::stoi( event_info[0] );
    event_type = static_cast<EventType>( hex_string_to_unsigned_int( event_info[1] ) );
    key = hex_string_to_unsigned_int( event_info[2] );
    velocity = hex_string_to_unsigned_int( event_info[3] );
  }
};

class MidiRecording
{
private:
  std::vector<MidiEvent> events_ {};

public:
  const std::string name;

  MidiRecording( const std::string filename )
    : name( filename )
  {
    using namespace std;

    ifstream midi_recording( filename.c_str() );
    string current_line;
    // read the midi recording file line by line
    while ( getline( midi_recording, current_line ) ) {
      // generate MidiEvent based on the current line
      if ( current_line.size() > 0 ) {
        MidiEvent event( current_line );
        events_.push_back( event );
      }
    }
    // close the midi recording file
    midi_recording.close();
  }

  const std::vector<MidiEvent>& events() const { return events_; }
};
