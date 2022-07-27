#pragma once

#include <Eigen/Dense>

static constexpr float leaky_constant = 0.01;

template<typename T, int T_input_size, int T_output_size>
class Layer
{
  static_assert( T_input_size > 0 );
  static_assert( T_output_size > 0 );

public:
  using type = T;

  using M_weights = Eigen::Matrix<T, T_input_size, T_output_size>;
  using M_biases = Eigen::Matrix<T, 1, T_output_size>;

  static constexpr size_t num_params = ( T_input_size + 1 ) * T_output_size;
  static constexpr size_t input_size = T_input_size;
  static constexpr size_t output_size = T_output_size;

  const M_weights& weights() const { return weights_; }
  const M_biases& biases() const { return biases_; }

  M_weights& weights() { return weights_; }
  M_biases& biases() { return biases_; }

  template<int T_batch_size>
  using M_input = Eigen::Matrix<T, T_batch_size, T_input_size>;

  template<int T_batch_size>
  using M_output = Eigen::Matrix<T, T_batch_size, T_output_size>;

  template<int T_batch_size>
  void apply_without_activation( const M_input<T_batch_size>& input,
                                 M_output<T_batch_size>& unactivated_output ) const
  {
    static_assert( T_batch_size > 0 );
    unactivated_output = ( input * weights_ ).rowwise() + biases_;
  }

  template<int T_batch_size>
  void activate( M_output<T_batch_size>& output ) const
  {
    static_assert( T_batch_size > 0 );
    // apply "leaky" rectifier (leaky ReLU)
    output = output.unaryExpr( []( const auto val ) { return val > 0 ? val : leaky_constant * val; } );
  }

  bool operator==( const Layer& other ) const = default;

private:
  M_weights weights_ {};
  M_biases biases_ {};
};
