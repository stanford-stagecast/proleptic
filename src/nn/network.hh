#pragma once

#include <array>
#include <tuple>

#include "layer.hh"

// recursive definition (base case below)
template<typename T, size_t T_batch_size, size_t i0, size_t o0, size_t... o_rest>
class Network
{
public:
  using L_layer0 = Layer<T, T_batch_size, i0, o0>;
  using N_rest = Network<T, T_batch_size, o0, o_rest...>;

private:
  L_layer0 layer0_ {};
  N_rest rest_ {};

  using M_output_this_layer = typename L_layer0::M_output;

  M_output_this_layer output_this_layer {};

public:
  static constexpr size_t num_layers = N_rest::num_layers + 1;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = N_rest::output_size;

  using M_input = typename L_layer0::M_input;
  using M_output = typename N_rest::M_output;

  void apply( const M_input& input, M_output& output )
  {
    layer0_.apply_without_activation( input, output_this_layer );
    layer0_.activate( output_this_layer );
    rest_.apply( output_this_layer, output );
  }

  template<size_t N, size_t i0_out, size_t o0_out, size_t... o_rest_out>
  struct LayerType
  {
    using type = typename N_rest::LayerType<N - 1, o0_out, o_rest_out...>::type;
  };

  template<size_t i0_out, size_t o0_out, size_t... o_rest_out>
  struct LayerType<0, i0_out, o0_out, o_rest_out...>
  {
    using type = L_layer0;
  };

  template<size_t layer_num>
  const typename LayerType<layer_num, i0, o0, o_rest...>::type& get_layer() const
  {
    if constexpr ( layer_num > 0 ) {
      return rest_.template get_layer<layer_num - 1>();
    } else {
      return layer0_;
    }
  }

  template<size_t layer_num>
  typename LayerType<layer_num, i0, o0, o_rest...>::type& get_layer()
  {
    if constexpr ( layer_num > 0 ) {
      return rest_.template get_layer<layer_num - 1>();
    } else {
      return layer0_;
    }
  }

  static constexpr bool is_last_layer = false;
  const N_rest& rest() const { return rest_; }
  N_rest& rest() { return rest_; }
};

// base case
template<class T, size_t T_batch_size, size_t i0, size_t o0>
class Network<T, T_batch_size, i0, o0>
{
  using L_layer0 = Layer<T, T_batch_size, i0, o0>;

  L_layer0 layer0_ {};

public:
  static constexpr size_t num_layers = 1;
  static constexpr size_t input_size = L_layer0::input_size;
  static constexpr size_t output_size = L_layer0::output_size;

  using M_input = typename L_layer0::M_input;
  using M_output = typename L_layer0::M_output;

  void apply( const M_input& input, M_output& output ) { layer0_.apply_without_activation( input, output ); }

  template<size_t N, size_t i0_out, size_t o0_out, size_t... o_rest_out>
  struct LayerType;

  template<size_t i0_out, size_t o0_out>
  struct LayerType<0, i0_out, o0_out>
  {
    using type = L_layer0;
  };

  template<size_t layer_num>
  typename LayerType<layer_num, i0, o0>::type& get_layer()
  {
    static_assert( layer_num == 0 );
    return layer0_;
  }

  template<size_t layer_num>
  const typename LayerType<layer_num, i0, o0>::type& get_layer() const
  {
    static_assert( layer_num == 0 );
    return layer0_;
  }

  static constexpr bool is_last_layer = true;
};
