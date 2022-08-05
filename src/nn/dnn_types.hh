#pragma once

#include "network.hh"

using DNN = Network<float, 16, 16, 1, 30, 2560, 1>;

static constexpr size_t BATCH_SIZE = 64;
