#pragma once

#include <array>

#include "layer.hh"

using namespace std;

// recursive definition (base case below)
template<typename T, size_t T_batch_size, size_t i0, size_t o0, size_t... o_rest>
struct Network
{
  using L_layer0 = Layer<T, T_batch_size, i0, o0>;
  using N_rest = Network<T, T_batch_size, o0, o_rest...>;

  L_layer0 layer0 {};
  N_rest rest {};

  static constexpr size_t num_layers = N_rest::num_layers() + 1;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = N_rest::output_size;
  static constexpr size_t storage_required
    = max( max( sizeof( typename L_layer0::M_input ), sizeof( typename L_layer0::M_output ) ),
           N_rest::storage_required );

  using M_input = typename L_layer0::M_input;
  using M_output = typename N_rest::M_output;
  using A_storage = std::array<char, storage_required>;

  template<size_t N>
  M_output& apply( std::array<char, N>& input, std::array<char, N>& output, std::array<char, N>& extra_buf )
  {
    static_assert( N >= storage_required, "output buffer too small" );

    const M_input* input_matrix = reinterpret_cast<M_input*>( input.data() );
    typename L_layer0::M_output* output_matrix = reinterpret_cast<typename L_layer0::M_output*>( output.data() );

    layer0.apply_with_activation( *input_matrix, *output_matrix );

    return rest.apply( output, extra_buf, input );
  }
};

// base case
template<class T, size_t batch_size, size_t i0, size_t o0>
struct Network<T, batch_size, i0, o0>
{
  using L_layer0 = Layer<T, batch_size, i0, o0>;
  using M_input = typename L_layer0::M_input;
  using M_output = typename L_layer0::M_output;

  L_layer0 layer0 {};

  static constexpr size_t num_layers = 1;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = o0;
  static constexpr size_t storage_required = max( sizeof( M_input ), sizeof( M_output ) );

  using A_storage = std::array<char, storage_required>;

  template<size_t N>
  M_output& apply( std::array<char, N>& input, std::array<char, N>& output, std::array<char, N>& )
  {
    static_assert( N >= storage_required, "output buffer too small" );

    const M_input* input_matrix = reinterpret_cast<M_input*>( input.data() );
    M_output* output_matrix = reinterpret_cast<M_output*>( output.data() );

    layer0.apply_without_activation( *input_matrix, *output_matrix );

    return *output_matrix;
  }
};
