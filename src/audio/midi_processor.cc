#include "midi_processor.hh"
#include <iostream>


MidiProcessor::MidiProcessor(std::string data_str)
:   data( data_str ), 
    raw_input_( string_span::from_view( data ) ),
    leftover_data(),
    key_presses() {
        
    }


uint8_t MidiProcessor::popPress() {
  key_press curr_press = key_presses.front();
  key_presses.pop();
  return curr_press.note;
}

void MidiProcessor::pushPress( ) {
    key_press new_press;
    if (static_cast<unsigned int>( leftover_data.front() ) == 144) {
      new_press.direction = leftover_data.front();
      leftover_data.pop();
      new_press.note = leftover_data.front();
      leftover_data.pop();
      new_press.velocity = leftover_data.front();
      leftover_data.pop();
      key_presses.push(new_press);
    } else {
      leftover_data.pop();
      leftover_data.pop();
      leftover_data.pop();
    }
    
}

void MidiProcessor::processIncomingMIDI( ssize_t bytes_read ) {
    string_view data_read_this_time { data.data(), (long unsigned int) bytes_read };

    
    for (size_t i = 0; i < data_read_this_time.size(); i++) {
      uint8_t byte = data_read_this_time[i];
      
      if ( byte != 0xfe ) {
        leftover_data.push(byte);
        if (leftover_data.size() == 3) {
          pushPress();
        }
        cerr << "Piano gave us: " << static_cast<unsigned int>( byte ) << "\n";
      }
    }

}