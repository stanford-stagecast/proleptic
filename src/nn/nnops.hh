#pragma once

#include <Eigen/Dense>

// These functions model the application of a network of fully-connected
// layers with "leaky ReLU" activation.

static constexpr float leaky_constant = 0.01;

// Apply the linear part of a fully-connected layer (matrix multiplication)
template<class LayerType, int T_batch_size>
void apply_fully_connected( const LayerType& layer,
                            const typename LayerType::template M_input<T_batch_size>& input,
                            typename LayerType::template M_output<T_batch_size>& unactivated_output )
{
  static_assert( T_batch_size > 0 );
  unactivated_output = ( input * layer.weights() ).rowwise() + layer.biases();
}

// Apply the nonlinear part of the layer ("leaky ReLU")
template<class LayerType, int T_batch_size>
void activate( typename LayerType::template M_output<T_batch_size>& output )
{
  static_assert( T_batch_size > 0 );
  output = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
}
