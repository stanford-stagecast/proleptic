#pragma once

#include "network.hh"

using DNN = Network<float, 16, 16, 1, 30, 2560, 1>;

using DNN_timestamp = Network<float, 16, 256, 256, 256, 2>;
using DNN_tempo = Network<float, 16, 256, 256, 1>;
using DNN_piano_roll_rhythm_prediction = Network<float, 64, 256, 1>;
static constexpr size_t PIANO_ROLL_NUMBER_OF_NOTES = 12;
static constexpr size_t PIANO_ROLL_HISTORY_WINDOW_LENGTH = 64;
using DNN_piano_roll_octave_prediction
  = Network<float, PIANO_ROLL_NUMBER_OF_NOTES * PIANO_ROLL_HISTORY_WINDOW_LENGTH, 128, PIANO_ROLL_NUMBER_OF_NOTES>;

static constexpr size_t BATCH_SIZE = 64;
