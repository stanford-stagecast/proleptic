#pragma once

#include <Eigen/Dense>

static constexpr float leaky_constant = 0.01;

// The Layer class models a fully-connected neural-network layer
// with leaky ReLU activation. The layer transforms a vector
// of size "T_input_size" into a vector of size "T_output_size".
template<typename T, int T_input_size, int T_output_size>
class Layer
{
  static_assert( T_input_size > 0 );
  static_assert( T_output_size > 0 );

public:
  // The type of entry (e.g. float or double)
  using type = T;

  // Types of the weight and bias matrices
  using M_weights = Eigen::Matrix<T, T_input_size, T_output_size>;
  using M_biases = Eigen::Matrix<T, 1, T_output_size>;

  // Helpful constants
  static constexpr size_t num_params = ( T_input_size + 1 ) * T_output_size;
  static constexpr size_t input_size = T_input_size;
  static constexpr size_t output_size = T_output_size;

  // Accessors
  const M_weights& weights() const { return weights_; }
  const M_biases& biases() const { return biases_; }
  M_weights& weights() { return weights_; }
  M_biases& biases() { return biases_; }

  // Types of the input and output matrices (templated by batch size)
  template<int T_batch_size>
  using M_input = Eigen::Matrix<T, T_batch_size, T_input_size>;

  template<int T_batch_size>
  using M_output = Eigen::Matrix<T, T_batch_size, T_output_size>;

  // Apply the linear part of the fully-connected layer (matrix multiplication)
  template<int T_batch_size>
  void apply_without_activation( const M_input<T_batch_size>& input,
                                 M_output<T_batch_size>& unactivated_output ) const
  {
    static_assert( T_batch_size > 0 );
    unactivated_output = ( input * weights_ ).rowwise() + biases_;
  }

  // Apply the nonlinear part of the layer ("leaky ReLU")
  template<int T_batch_size>
  void activate( M_output<T_batch_size>& output ) const
  {
    static_assert( T_batch_size > 0 );
    output = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
  }

  bool operator==( const Layer& other ) const = default;

private:
  // What the layer contains: weights and biases.
  M_weights weights_ {};
  M_biases biases_ {};
};
