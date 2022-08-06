#pragma once

#include <Eigen/Dense>

// These functions model the application of a network of fully-connected
// layers with "leaky ReLU" activation.

static constexpr float leaky_constant = 0.01;

template<class T_layer>
struct LayerInference
{
  // Types of the input and output matrices (templated by batch size)
  template<int T_batch_size>
  using M_input = Eigen::Matrix<typename T_layer::type, T_batch_size, T_layer::input_size>;

  template<int T_batch_size>
  using M_output = Eigen::Matrix<typename T_layer::type, T_batch_size, T_layer::output_size>;

  // Apply the linear part of a fully-connected layer (matrix multiplication)
  template<int T_batch_size>
  static void apply_fully_connected_layer( const T_layer& layer,
                                           const M_input<T_batch_size>& input,
                                           M_output<T_batch_size>& unactivated_output )
  {
    static_assert( T_batch_size > 0 );
    unactivated_output = ( input * layer.weights() ).rowwise() + layer.biases();
  }

  // Apply the nonlinear part of the layer ("leaky ReLU")
  template<int T_batch_size>
  static void apply_leaky_relu( M_output<T_batch_size>& output )
  {
    static_assert( T_batch_size > 0 );
    output = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
  }
};

// Apply the neural network to a given input, writing the activations as output.
// This applies the current layer and then recurses to the rest of the layers
// if necessary.
template<int T_batch_size, class NetworkType>
void apply( const NetworkType& network,
            const typename NetworkType::template M_input<T_batch_size>& input,
            typename NetworkType::template Activations<T_batch_size>& activations )
{
  using Infer = LayerInference<typename NetworkType::L_layer0>;

  Infer::apply_fully_connected_layer( network.first_layer(), input, activations.first_layer() );

  if constexpr ( not NetworkType::is_last_layer ) {
    Infer::apply_leaky_relu( activations.first_layer() );
    apply<T_batch_size>( network.rest(), activations.first_layer(), activations.rest() );
  }
}
