#pragma once

#include <Eigen/Dense>

// The Layer models a neural-network layer that transforms a vector
// of size "input_size" into a vector of size "output_size".
// The operations are external to the layer and are implemented in "inference.hh".

template<typename T, int input_size_T, int output_size_T>
struct Layer
{
  // Helpful constants
  static constexpr size_t input_size = input_size_T, output_size = output_size_T;
  static constexpr size_t num_params = ( input_size + 1 ) * output_size;

  // Layer must have a positive input and output size
  static_assert( input_size > 0 and output_size > 0 );

  // The type of entry (e.g. float or double) and of the weight and bias matrices
  using type = T;
  using Weights = Eigen::Matrix<T, input_size, output_size>;
  using Biases = Eigen::Matrix<T, 1, output_size>;

  // What the layer contains: weights and biases.
  Weights weights {};
  Biases biases {};

  // Comparison
  bool operator==( const Layer& other ) const = default;
};
