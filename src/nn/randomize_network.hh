#pragma once
#include "network.hh"
#include "random.hh"

struct RandomState
{
  std::default_random_engine prng { get_random_engine() };
  std::normal_distribution<float> parameter_distribution { 0.0, 0.1 };

  float sample() { return parameter_distribution( prng ); }
};

template<LayerT Layer>
static void randomize_layer( Layer& layer, RandomState& rng )
{
  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) = rng.sample();
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) = rng.sample();
  }
}

template<NetworkT Network>
static void randomize_network( Network& network, RandomState& rng )
{
  randomize_layer( network.first, rng );

  if constexpr ( not Network::is_last ) {
    randomize_network( network.rest, rng );
  }
}
