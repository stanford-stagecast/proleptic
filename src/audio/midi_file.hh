#pragma once
#include "exception.hh"
#include "file_descriptor.hh"
#include "piano_roll.hh"

#include <algorithm>
#include <array>
#include <assert.h>
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
    } else if ( event_type & 0x80 ) {
      // MIDI
      key = read_one<uint8_t>( filename, fd );
      velocity = read_one<uint8_t>( filename, fd );
      last_status = event_type;
      channel = event_type & 0xf;
      event_type = static_cast<EventType>( event_type & 0xf0 );
    } else if ( last_status.has_value() ) {
      key = event_type;
      event_type = *last_status;
      velocity = read_one<uint8_t>( filename, fd );
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

    size_t ts_now = 0;
    std::vector<std::vector<MidiEvent>::const_iterator> track_iters {};
    std::vector<std::vector<MidiEvent>::const_iterator> track_ends {};
    std::vector<size_t> timestamps {};

    for ( const auto& track : tracks_ ) {
      track_iters.push_back( track.events().begin() );
      track_ends.push_back( track.events().end() );
      timestamps.push_back( track.events().front().ticks );
    }

    while ( not track_iters.empty() ) {
      size_t min_idx = 0;
      for ( size_t i = 0; i < timestamps.size(); i++ ) {
        if ( timestamps[i] < timestamps[min_idx] )
          min_idx = i;
      }
      MidiEvent event = *track_iters[min_idx];
      if ( not starting_tempo_.has_value() and event.event_type == MidiEvent::MetaEvent
           and event.meta_event_type == MidiEvent::Tempo ) {
        starting_tempo_ = event.tempo;
      }
      event.ticks = timestamps[min_idx] - ts_now;
      events_.push_back( event );
      ts_now = timestamps[min_idx];
      ++track_iters[min_idx];
      if ( track_iters[min_idx] == track_ends[min_idx] ) {
        track_iters.erase( track_iters.begin() + min_idx );
        track_ends.erase( track_ends.begin() + min_idx );
        timestamps.erase( timestamps.begin() + min_idx );
      } else {
        timestamps[min_idx] += track_iters[min_idx]->ticks;
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
      MidiEvent::EventType type = event.event_type;
      if ( type == MidiEvent::NoteOn and event.velocity > 0 ) {
        piano_roll_.back()[*event.key] = PianoRollEvent::NoteDown;
        have_any_notes = true;
      } else if ( type == MidiEvent::NoteOff or ( type == MidiEvent::NoteOn and event.velocity == 0 ) ) {
        piano_roll_.back()[*event.key] = PianoRollEvent::NoteUp;
        have_any_notes = true;
      }
      if ( ticks_in_current_timeslot >= ticks_per_piano_roll ) {
        size_t rolls = ticks_in_current_timeslot / ticks_per_piano_roll;
        ticks_in_current_timeslot %= ticks_per_piano_roll;
        for ( size_t i = 0; i < rolls; i++ ) {
          piano_roll_.push_back( piano_roll_.back() );
        }
      }
      ++iter;
    }
  }

  size_t ticks_per_beat() const { return header_.ticks_per_beat; };
  const std::vector<MidiTrack>& tracks() const { return tracks_; }
  const std::vector<MidiEvent>& events() const { return events_; }
  std::optional<uint32_t> starting_tempo() const { return starting_tempo_; };
  const PianoRoll<128>& piano_roll() const { return piano_roll_; };
};
