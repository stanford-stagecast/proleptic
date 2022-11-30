#pragma once
#include "exception.hh"
#include "file_descriptor.hh"
#include "piano_roll.hh"

#include <algorithm>
#include <array>
#include <assert.h>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <optional>
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

  enum MetaEventType
  {
    SequenceNumber = 0,
    TextEvent,
    CopyrightNotice,
    TrackName,
    InstrumentName,
    LyricText,
    MarkerText,
    CuePoint,
    Channel = 0x20,
    EndOfTrack = 0x2F,
    Tempo = 0x51,
    SmpteOffset = 0x54,
    TimeSignature = 0x58,
    KeySignature = 0x59,
    Sequencer = 0x7F,
  };

  uint32_t ticks;
  EventType event_type;
  std::optional<MetaEventType> meta_event_type = std::nullopt;
  std::optional<uint8_t> channel = std::nullopt;
  std::optional<uint8_t> key = std::nullopt;
  std::optional<uint8_t> velocity = std::nullopt;
  std::optional<uint32_t> tempo = std::nullopt;
  std::vector<uint8_t> data {};

  template<class T>
  T read_one( const std::string_view filename, const int fd )
  {
    char value[sizeof( T )];
    static_assert( std::is_fundamental<T>::value, "can only read primitive types" );
    size_t bytes = CheckSystemCall( filename, read( fd, &value, sizeof( T ) ) );
    if ( bytes != sizeof( T ) ) {
      throw unix_error( "read the wrong number of bytes" );
    }
    std::reverse( value, value + sizeof( T ) );
    return *reinterpret_cast<T*>( value );
  }

  template<class T, size_t N>
  std::array<T, N> read_several( const std::string_view filename, const int fd )
  {
    std::array<T, N> values {};
    for ( size_t i = 0; i < N; i++ ) {
      values[i] = read_one<T>( filename, fd );
    }
    return values;
  }

  uint32_t read_variable_length_int( const std::string_view filename, const int fd )
  {
    uint32_t value = 0;
    bool high_bit;
    do {
      value <<= 7;
      uint8_t next_value = read_one<uint8_t>( filename, fd );
      high_bit = next_value & 0x80;
      value |= next_value & 0x7F;
    } while ( high_bit );
    return value;
  }

  MidiEvent( const std::string_view filename, int fd )
    : ticks( read_variable_length_int( filename, fd ) )
    , event_type( static_cast<EventType>( read_one<uint8_t>( filename, fd ) ) )
  {
    static std::optional<EventType> last_status = std::nullopt;
    if ( event_type == 0xFF ) {
      meta_event_type = static_cast<MetaEventType>( read_one<uint8_t>( filename, fd ) );
      uint8_t length;
      switch ( *meta_event_type ) {
        case SequenceNumber:
          length = 4;
          break;
        case Channel:
          length = 2;
          break;
        case EndOfTrack:
          length = 1;
          break;
        case Tempo:
          tempo = read_one<uint32_t>( filename, fd ) & 0xffffff;
          length = 0;
          break;
        case SmpteOffset:
          length = 6;
          break;
        case TimeSignature:
          length = 5;
          break;
        case KeySignature:
          length = 3;
          break;
        default:
          length = read_variable_length_int( filename, fd );
      }
      for ( size_t i = 0; i < length; i++ ) {
        data.push_back( read_one<uint8_t>( filename, fd ) );
      }
      last_status = std::nullopt;
    } else if ( ( event_type & 0xF0 ) == 0xF0 ) {
      // SysEx
      uint8_t length = read_variable_length_int( filename, fd );
      for ( size_t i = 0; i < length; i++ ) {
        data.push_back( read_one<uint8_t>( filename, fd ) );
      }
      last_status = std::nullopt;
    } else if ( event_type & 0x80 or last_status.has_value() ) {
      // MIDI
      uint8_t arg0;
      if ( event_type & 0x80 ) {
        arg0 = read_one<uint8_t>( filename, fd );
      } else {
        arg0 = event_type;
        event_type = *last_status;
      }
      uint8_t masked = event_type & 0xf0;
      if ( masked == EventType::NoteOn or masked == EventType::NoteOff
           or masked == EventType::PolyphonicKeyPressure ) {
        key = arg0;
        velocity = read_one<uint8_t>( filename, fd );
      } else if ( masked == EventType::ControlChange ) {
        read_one<uint8_t>( filename, fd );
      } else if ( masked == EventType::ProgramChange ) {
      } else {
        throw std::runtime_error( "Unhandled MIDI event type" );
      }
      last_status = event_type;
      channel = event_type & 0xf;
      event_type = static_cast<EventType>( event_type & 0xf0 );
    } else {
      throw std::runtime_error( "MIDI file out of sync" );
    }
  }
};

class MidiTrack
{
private:
  template<class T>
  T read_one( const std::string_view filename, const int fd )
  {
    char value[sizeof( T )];
    static_assert( std::is_fundamental<T>::value, "can only read primitive types" );
    size_t bytes = CheckSystemCall( filename, read( fd, &value, sizeof( T ) ) );
    if ( bytes != sizeof( T ) ) {
      throw unix_error( "read the wrong number of bytes" );
    }
    std::reverse( value, value + sizeof( T ) );
    return *reinterpret_cast<T*>( value );
  }

  template<class T, size_t N>
  std::array<T, N> read_several( const std::string_view filename, const int fd )
  {
    std::array<T, N> values {};
    for ( size_t i = 0; i < N; i++ ) {
      values[i] = read_one<T>( filename, fd );
    }
    return values;
  }

  std::vector<MidiEvent> events_ {};
  std::optional<uint32_t> starting_tempo_ = std::nullopt;

public:
  MidiTrack( const std::string_view filename, int fd )
  {
    char magic[4];
    memcpy( &magic, read_several<char, 4>( filename, fd ).data(), 4 );
    if ( memcmp( magic, "MTrk", 4 ) ) {
      throw std::runtime_error( "Invalid magic number in MIDI track" );
    }
    uint32_t nbytes = read_one<uint32_t>( filename, fd );
    off_t start_offset = CheckSystemCall( filename, lseek( fd, 0, SEEK_CUR ) );
    off_t current_offset = CheckSystemCall( filename, lseek( fd, 0, SEEK_CUR ) );
    while ( current_offset < start_offset + nbytes ) {
      MidiEvent event( filename, fd );
      if ( ( not starting_tempo_.has_value() ) and event.event_type == MidiEvent::MetaEvent
           and event.meta_event_type == MidiEvent::Tempo ) {
        starting_tempo_ = event.tempo;
      }
      events_.push_back( event );
      current_offset = CheckSystemCall( filename, lseek( fd, 0, SEEK_CUR ) );
    }
  }

  const std::vector<MidiEvent>& events() const { return events_; }
  std::optional<uint32_t> starting_tempo() const { return starting_tempo_; };
};

class MidiFile
{
private:
  struct MidiHeader
  {
    char magic[4];
    uint32_t header_length;
    uint16_t format;
    uint16_t track_count;
    int16_t ticks_per_beat;
  };
  MidiHeader header_ {};
  std::vector<MidiEvent> events_ {};
  std::vector<MidiTrack> tracks_ {};
  PianoRoll<128> piano_roll_ {};
  std::vector<float> time_in_beats_ {};
  std::vector<uint32_t> periods_ {};
  std::optional<uint32_t> starting_tempo_ = std::nullopt;

  template<class T>
  T read_one( const std::string_view filename, const int fd )
  {
    char value[sizeof( T )];
    static_assert( std::is_fundamental<T>::value, "can only read primitive types" );
    size_t bytes = CheckSystemCall( filename, read( fd, &value, sizeof( T ) ) );
    if ( bytes != sizeof( T ) ) {
      throw unix_error( "read the wrong number of bytes" );
    }
    std::reverse( value, value + sizeof( T ) );
    return *reinterpret_cast<T*>( value );
  }

  template<class T, size_t N>
  std::array<T, N> read_several( const std::string_view filename, const int fd )
  {
    std::array<T, N> values {};
    for ( size_t i = 0; i < N; i++ ) {
      values[i] = read_one<T>( filename, fd );
    }
    return values;
  }

public:
  const std::string name;

  MidiFile( const std::string filename )
    : name( filename )
  {
    using namespace std;
    int fd = CheckSystemCall( filename, open( filename.c_str(), O_RDONLY ) );
    memcpy( header_.magic, read_several<char, 4>( filename, fd ).data(), 4 );
    header_.header_length = read_one<uint32_t>( filename, fd );
    header_.format = read_one<uint16_t>( filename, fd );
    header_.track_count = read_one<uint16_t>( filename, fd );
    header_.ticks_per_beat = read_one<int16_t>( filename, fd );

    if ( memcmp( header_.magic, "MThd", 4 ) ) {
      throw runtime_error( "Invalid magic number in MIDI header" );
    }

    if ( header_.ticks_per_beat <= 0 ) {
      throw runtime_error( "Invalid ticks per beat in MIDI header" );
    }

    off_t current = CheckSystemCall( filename, lseek( fd, 0, SEEK_CUR ) );
    off_t end = CheckSystemCall( filename, lseek( fd, 0, SEEK_END ) );
    CheckSystemCall( filename, lseek( fd, current, SEEK_SET ) );

    while ( current < end ) {
      tracks_.push_back( MidiTrack( filename, fd ) );
      current = CheckSystemCall( filename, lseek( fd, 0, SEEK_CUR ) );
    }

    // the MIDI file separates events into tracks, but we want to reintegrate
    // all of them into a single stream
    std::vector<std::deque<MidiEvent>> tracks {};

    for ( const auto& track : tracks_ ) {
      std::deque events( track.events().begin(), track.events().end() );
      tracks.push_back( events );
    }

    // Keep advancing time to the next event we can see in any channel
    float current_time = 0;
    uint32_t current_tempo = 0;
    while ( not tracks.empty() ) {
      size_t min_idx = 0;
      for ( size_t i = 0; i < tracks.size(); i++ ) {
        if ( tracks.at( i ).front().ticks < tracks.at( min_idx ).front().ticks ) {
          min_idx = i;
        }
      }
      current_time += tracks.at( min_idx ).front().ticks / (float)header_.ticks_per_beat;
      std::deque<MidiEvent>& track = tracks.at( min_idx );
      MidiEvent& event = track.front();
      if ( event.event_type == 0x80 or event.event_type == 0x90 ) {}
      size_t ticks = event.ticks;
      events_.push_back( event );

      // tempo change
      if ( event.event_type == 0xFF and event.meta_event_type.value() == 0x51 ) {
        current_tempo = event.tempo.value();
      }
      // NoteOn
      if ( event.event_type == 0x90 and event.velocity != 0 ) {
        // push current time (in beats) into time_in_beats_ queue
        // push current period (in microseconds per quarter note) into periods_ queue
        if ( time_in_beats_.size() == 0 or time_in_beats_.back() != current_time ) {
          time_in_beats_.push_back( current_time );
          periods_.push_back( current_tempo );
          cout << current_time << "\t\t\t" << current_tempo << "\t\t" << unsigned( event.velocity.value() ) << endl;
        }
      }

      for ( std::deque<MidiEvent>& t : tracks ) {
        t.front().ticks -= ticks;
      }
      track.pop_front();

      if ( track.empty() ) {
        tracks.erase( tracks.begin() + min_idx );
      }
    }

    piano_roll_.push_back( {} );
    for ( size_t i = 0; i < piano_roll_.back().size(); i++ ) {
      piano_roll_.back()[i] = PianoRollEvent::Unknown;
    }
    size_t ticks_per_piano_roll = header_.ticks_per_beat / 4;
    size_t ticks_in_current_timeslot = ticks_per_piano_roll / 2;
    bool have_any_notes = false;

    auto iter = events_.begin();

    while ( iter < events_.end() ) {
      const MidiEvent& event = *iter;
      if ( have_any_notes )
        ticks_in_current_timeslot += event.ticks;
      if ( ticks_in_current_timeslot >= ticks_per_piano_roll ) {
        size_t rolls = ticks_in_current_timeslot / ticks_per_piano_roll;
        ticks_in_current_timeslot %= ticks_per_piano_roll;
        for ( size_t i = 0; i < rolls; i++ ) {
          piano_roll_.push_back( piano_roll_.back() );
        }
      }
      MidiEvent::EventType type = event.event_type;
      if ( type == MidiEvent::NoteOn and event.velocity > 0 ) {
        piano_roll_.back()[*event.key] = PianoRollEvent::NoteDown;
        have_any_notes = true;
      } else if ( type == MidiEvent::NoteOff or ( type == MidiEvent::NoteOn and event.velocity == 0 ) ) {
        piano_roll_.back()[*event.key] = PianoRollEvent::NoteUp;
        have_any_notes = true;
      }
      ++iter;
    }
  }

  size_t ticks_per_beat() const { return header_.ticks_per_beat; };
  const std::vector<MidiTrack>& tracks() const { return tracks_; }
  const std::vector<MidiEvent>& events() const { return events_; }
  std::optional<uint32_t> starting_tempo() const { return starting_tempo_; };
  const PianoRoll<128>& piano_roll() const { return piano_roll_; };
  const std::vector<float>& time_in_beats() const { return time_in_beats_; };
  const std::vector<uint32_t>& periods() const { return periods_; };
};
