#pragma once

#include "network.hh"

using DNN = Network<float, 16, 16, 1, 30, 2560, 1>;

using DNN_timestamp = Network<float, 16, 256, 256, 256, 2>;
using DNN_piano_roll_prediction = Network<float, 64, 256, 1>;

static constexpr size_t BATCH_SIZE = 64;
