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
  void apply_fully_connected_layer( const Layer& layer, const Input& input )
  {
    static_assert( batch_size > 0 );
    output = ( input * layer.weights ).rowwise() + layer.biases; // unactivated
  }

  // Activations (outputs) from the layer
  Output output {};
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

// Here is the recursive case: inference for a non-terminal Network.
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
  LayerInfer first {};
  RestInfer rest {};

  // Accessors
  const Output& output() const { return rest.output(); }
  Output& output() { return rest.output(); }

  // Apply the neural network to a given input, writing the activations as output.
  // This applies the current layer and then recurses to the rest.
  void apply( const Network& network, const Input& input )
  {
    first.apply_fully_connected_layer( network.first, input );
    apply_leaky_relu( first.output );
    rest.apply( network.rest, first.output );
  }
};

// Here is the base case: inference for a single-layer Network.
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
  LayerInfer first {};

  // Accessors
  const Output& output() const { return first.output; }
  Output& output() { return first.output; }

  // Apply the neural network to a given input, writing the activations as output.
  // This applies the current (last) layer only, with no activation function.
  void apply( const Network& network, const Input& input )
  {
    first.apply_fully_connected_layer( network.first, input );
  }
};

template<class ProposedLayerInference>
concept LayerInferenceT = requires( ProposedLayerInference c )
{
  []<LayerT Layer, int batch_size>( LayerInference<Layer, batch_size>& ) {}( c );
};

template<class ProposedNetworkInference>
concept NetworkInferenceT = requires( ProposedNetworkInference c )
{
  []<NetworkT Network, int batch_size>( NetworkInference<Network, batch_size>& ) {}( c );
};
