#pragma once

#include "network.hh"

using DNN = Network<float, 16, 16, 1, 30, 2560, 1>;

using DNN_timestamp = Network<float, 16, 256, 256, 256, 2>;

static constexpr size_t BATCH_SIZE = 64;
