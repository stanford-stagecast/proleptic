#pragma once

#include <Eigen/Dense>

// These functions model the application of a network of fully-connected
// layers with "leaky ReLU" activation.

static constexpr float leaky_constant = 0.01;

// Apply the linear part of a fully-connected layer (matrix multiplication)
template<int T_batch_size, class LayerType>
void apply_fully_connected_layer( const LayerType& layer,
                                  const typename LayerType::template M_input<T_batch_size>& input,
                                  typename LayerType::template M_output<T_batch_size>& unactivated_output )
{
  static_assert( T_batch_size > 0 );
  unactivated_output = ( input * layer.weights() ).rowwise() + layer.biases();
}

// Apply the nonlinear part of the layer ("leaky ReLU")
template<int T_batch_size, class OutputType>
void apply_leaky_relu( OutputType& output )
{
  static_assert( T_batch_size > 0 );
  output = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
}

// Apply the neural network to a given input, writing the activations as output.
// This applies the current layer and then recurses to the rest of the layers
// if necessary.
template<int T_batch_size, class NetworkType>
void apply( const NetworkType& network,
            const typename NetworkType::template M_input<T_batch_size>& input,
            typename NetworkType::template Activations<T_batch_size>& activations )
{
  apply_fully_connected_layer<T_batch_size>( network.first_layer(), input, activations.first_layer() );

  if constexpr ( not NetworkType::is_last_layer ) {
    apply_leaky_relu<T_batch_size>( activations.first_layer() );
    apply<T_batch_size>( network.rest(), activations.first_layer(), activations.rest() );
  }
}
