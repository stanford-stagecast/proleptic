#pragma once

#include "spans.hh"
#include "file_descriptor.hh"
#include <queue>

struct key_press {
  uint8_t direction;
  uint8_t note;
  uint8_t velocity;
};

using namespace std;

/* wrap MIDI file input */
class MidiProcessor
{
  string data;
  string_span raw_input_;
  queue<uint8_t> leftover_data;
  queue<key_press> key_presses;

private:
  void pushPress( );

public:
    
  MidiProcessor(string data_str);

  string_span getDataBuffer() { return raw_input_; };

  void processIncomingMIDI( ssize_t bytes_read );

  size_t pressesSize() { return key_presses.size(); };

  void popPress();

};