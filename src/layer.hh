#pragma once

#include <Eigen/Dense>

template<typename T, size_t T_batch_size, size_t T_input_size, size_t T_output_size>
class Layer
{
public:
  using M_input = Eigen::Matrix<T, T_batch_size, T_input_size>;
  using M_output = Eigen::Matrix<T, T_batch_size, T_output_size>;

  using M_weights = Eigen::Matrix<T, T_input_size, T_output_size>;
  using M_biases = Eigen::Matrix<T, 1, T_output_size>;

  static constexpr size_t num_params = ( T_input_size + 1 ) * T_output_size;
  static constexpr size_t batch_size = T_batch_size;
  static constexpr size_t input_size = T_input_size;
  static constexpr size_t output_size = T_output_size;

  const M_weights& weights() const { return weights_; }
  const M_biases& biases() const { return biases_; }

  M_weights& weights() { return weights_; }
  M_biases& biases() { return biases_; }

  void apply_with_activation( const M_input& input, M_output& output ) const
  {
    output = ( ( input * weights_ ).rowwise() + biases_ ).cwiseMax( 0 );
  }

  void apply_without_activation( const M_input& input, M_output& output ) const
  {
    output = ( input * weights_ ).rowwise() + biases_;
  }

private:
  M_weights weights_ {};
  M_biases biases_ {};
};
