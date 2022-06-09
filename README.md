Pancake: an amendable piano synthesizer

## How to Run:
1. Clone repo
2. In repo directory, make an empty build directory
3. Enter build directory and run `cmake ..`
4. Run `make`
5. Run `./src/frontend/synthesizer-test [device_prefix] [midi_device] [sample_directory]`
    If you're working on the snr-piano machine:
    - device_prefix: Scarlett
    - midi_device: /dev/midi1
    - sample_directory: 

## File Overview
- `synthesizer-test`: Entry point to the program. It runs an event loop which reads in new midi data, initiates the processing of midi data into audio, and sends the generated audio to the playback device.
- `midi_processor`: Class that reads data in from the midi device into a buffer. Midi data consists of event type, event note, and event velocity, where an "event" is something like the press of a key, the release of a key, a change in position of pedal, etc. `midi-processor` provides functions to access the oldest unprocessed event.
- `synthesizer`: Class handles the conversion of midi events into audio data.
- `note_repository`: Class that manages a list of `NoteFiles`.
- `note_files`: Class that holds all of the WAV files for a single note. (Each note has a low velocity, medium velocity, and high velocity audio file, as well as a release audio file.)


[![Compiles](https://github.com/stanford-stagecast/pancake/workflows/Compile/badge.svg?event=push)](https://github.com/stanford-stagecast/pancake/actions)
