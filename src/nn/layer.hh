#pragma once

#include <Eigen/Dense>

// The Layer class models a neural-network layer that transforms
// a vector of size "T_input_size" into a vector of size "T_output_size".
// The actual operations are external to the layer and are implemented in "nnops.hh".

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

  // Comparison
  bool operator==( const Layer& other ) const = default;

private:
  // What the layer contains: weights and biases.
  M_weights weights_ {};
  M_biases biases_ {};
};
