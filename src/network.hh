#pragma once

#include <array>

#include "layer.hh"

using namespace std;

// recursive definition (base case below)
template<typename T, size_t T_batch_size, size_t i0, size_t o0, size_t... o_rest>
class Network
{
public:
  using L_layer0 = Layer<T, T_batch_size, i0, o0>;
  using N_rest = Network<T, T_batch_size, o0, o_rest...>;

  static constexpr size_t num_layers = N_rest::num_layers + 1;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = N_rest::output_size;

  using M_input = typename L_layer0::M_input;
  using M_output = typename N_rest::M_output;

  void apply( const M_input& input, M_output& output )
  {
    layer0.apply_without_activation( input, unactivated_output_this_layer );
    layer0.activate( unactivated_output_this_layer, activated_output_this_layer );
    rest.apply( activated_output_this_layer, output );
  }

private:
  L_layer0 layer0 {};
  N_rest rest {};

  using M_output_this_layer = typename L_layer0::M_output;

  M_output_this_layer unactivated_output_this_layer {}, activated_output_this_layer {};
};

// base case
template<class T, size_t T_batch_size, size_t i0, size_t o0>
class Network<T, T_batch_size, i0, o0>
{
public:
  using L_layer0 = Layer<T, T_batch_size, i0, o0>;

  static constexpr size_t num_layers = 1;
  static constexpr size_t input_size = L_layer0::input_size;
  static constexpr size_t output_size = L_layer0::output_size;

  using M_input = typename L_layer0::M_input;
  using M_output = typename L_layer0::M_output;

  void apply( const M_input& input, M_output& output ) { layer0.apply_without_activation( input, output ); }

private:
  L_layer0 layer0 {};
};
