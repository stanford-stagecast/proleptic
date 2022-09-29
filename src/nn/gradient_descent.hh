#pragma once

#include <Eigen/Dense>
#include <concepts>
#include <type_traits>

#include "backprop.hh"

template<LayerT Layer, int batch_size>
struct LayerGradientDescent
{
  using Backprop = LayerBackPropagation<Layer, batch_size>;

  static void update( Layer& layer, const Backprop& backprop, float learning_rate )
  {
    layer.weights.noalias() -= learning_rate * backprop.weight_gradient;
    layer.biases.noalias() -= learning_rate * backprop.bias_gradient;
  }
};

template<NetworkT Network, int batch_size, bool is_last_T>
struct NetworkGradientDescentHelper;

template<NetworkT Network, int batch_size>
using NetworkGradientDescent = NetworkGradientDescentHelper<Network, batch_size, Network::is_last>;

// Here is the recursive case: gradient descent for a non-terminal Network.
template<NetworkT Network, int batch_size>
struct NetworkGradientDescentHelper<Network, batch_size, false>
{
  static_assert( not Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = false;

  using NetworkBackProp = NetworkBackPropagation<Network, batch_size>;
  using LayerDescent = LayerGradientDescent<typename Network::Layer0, batch_size>;
  using RestDescent = NetworkGradientDescent<typename Network::Rest, batch_size>;

  // Backpropagation state
  LayerDescent first {};
  RestDescent rest {};

  void update( Network& network, const NetworkBackProp& backprop, float learning_rate )
  {
    first.update( network.first, backprop.first, learning_rate );
    rest.update( network.rest, backprop.rest, learning_rate );
  }
};

// Here is the base case: gradient descent for a single-layer Network.
template<NetworkT Network, int batch_size>
struct NetworkGradientDescentHelper<Network, batch_size, true>
{
  static_assert( Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = true;

  using NetworkBackProp = NetworkBackPropagation<Network, batch_size>;
  using LayerDescent = LayerGradientDescent<typename Network::Layer0, batch_size>;

  // Backpropagation state
  LayerDescent first {};

  void update( Network& network, const NetworkBackProp& backprop, float learning_rate )
  {
    first.update( network.first, backprop.first, learning_rate );
  }
};
