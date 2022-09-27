#pragma once

#include <Eigen/Dense>
#include <concepts>
#include <type_traits>

#include "layer.hh"
#include "network.hh"

// These functions model the application of a network of fully-connected
// layers with "leaky ReLU" activation.

static constexpr float leaky_constant = 0.01;

template<LayerT Layer, int batch_size>
struct LayerInference
{
  // Types of the input and output matrices
  using Input = Eigen::Matrix<typename Layer::type, batch_size, Layer::input_size>;
  using Output = Eigen::Matrix<typename Layer::type, batch_size, Layer::output_size>;

  // Apply the linear part of a fully-connected layer (matrix multiplication)
  static void apply_fully_connected_layer( const Layer& layer, const Input& input, Output& unactivated_output )
  {
    static_assert( batch_size > 0 );
    unactivated_output = ( input * layer.weights ).rowwise() + layer.biases;
  }
};

// Apply the nonlinear part of the layer ("leaky ReLU")
template<class MatrixT>
static void apply_leaky_relu( MatrixT& output )
{
  output.noalias() = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
}

template<NetworkT Network, int batch_size, bool is_last_T>
struct NetworkInferenceHelper;

template<NetworkT Network, int batch_size>
using NetworkInference = NetworkInferenceHelper<Network, batch_size, Network::is_last>;

template<NetworkT Network, int batch_size>
struct NetworkInferenceHelper<Network, batch_size, false>
{
  static_assert( not Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = false;

  using LayerInfer = LayerInference<typename Network::Layer0, batch_size>;
  using RestInfer = NetworkInference<typename Network::Rest, batch_size>;

  // Types of the input and output matrices
  using Input = typename LayerInfer::Input;
  using Layer0Output = typename LayerInfer::Output;
  using Output = typename RestInfer::Output;

  // Activations (outputs)
  Layer0Output first {};
  RestInfer rest {};

  // Accessors
  const Output& output() const { return rest.output(); }
  Output& output() { return rest.output(); }

  // Apply the neural network to a given input, writing the activations as output.
  // This applies the current layer and then recurses to the rest.
  void apply( const Network& network, const Input& input )
  {
    LayerInfer::apply_fully_connected_layer( network.first, input, first );
    apply_leaky_relu( first );
    rest.apply( network.rest, first );
  }
};

template<NetworkT Network, int batch_size>
struct NetworkInferenceHelper<Network, batch_size, true>
{
  static_assert( Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = true;

  using LayerInfer = LayerInference<typename Network::Layer0, batch_size>;

  // Types of the input and output matrices
  using Input = typename LayerInfer::Input;
  using Output = typename LayerInfer::Output;

  // Activations (outputs)
  Output first {};

  // Accessors
  const Output& output() const { return first; }
  Output& output() { return first; }

  // Apply the neural network to a given input, writing the activations as output.
  // This applies the current (last) layer only, with no activation function.
  void apply( const Network& network, const Input& input )
  {
    LayerInfer::apply_fully_connected_layer( network.first, input, first );
  }
};
