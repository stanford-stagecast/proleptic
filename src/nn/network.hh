#pragma once

#include <array>
#include <tuple>

#include "layer.hh"

// recursive definition (base case below)
template<typename T, int i0, int o0, int... o_rest>
class Network
{
public:
  using L_layer0 = Layer<T, i0, o0>;
  using N_rest = Network<T, o0, o_rest...>;

private:
  L_layer0 layer0_ {};
  N_rest rest_ {};

public:
  static constexpr size_t num_layers = N_rest::num_layers + 1;
  static constexpr size_t num_params = L_layer0::num_params + N_rest::num_params;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = N_rest::output_size;
  using type = T;

  template<int T_batch_size>
  using M_input = typename L_layer0::template M_input<T_batch_size>;

  template<int T_batch_size>
  using M_output = typename N_rest::template M_output<T_batch_size>;

  template<int T_batch_size>
  void apply( const M_input<T_batch_size>& input, M_output<T_batch_size>& output )
  {
    static typename L_layer0::template M_output<T_batch_size> output_this_layer;

    layer0_.apply_without_activation( input, output_this_layer );
    layer0_.activate( output_this_layer );
    rest_.apply( output_this_layer, output );
  }

  template<size_t N, int i0_out, int o0_out, int... o_rest_out>
  struct LayerType
  {
    using type = typename N_rest::template LayerType<N - 1, o0_out, o_rest_out...>::type;
  };

  template<int i0_out, int o0_out, int... o_rest_out>
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

  bool operator==( const Network& other ) const { return layer0_ == other.layer0_ and rest_ == other.rest_; }
};

// base case
template<class T, int i0, int o0>
class Network<T, i0, o0>
{
  using L_layer0 = Layer<T, i0, o0>;

  L_layer0 layer0_ {};

public:
  static constexpr size_t num_layers = 1;
  static constexpr size_t num_params = L_layer0::num_params;
  static constexpr size_t input_size = L_layer0::input_size;
  static constexpr size_t output_size = L_layer0::output_size;
  using type = T;

  template<int T_batch_size>
  using M_input = typename L_layer0::template M_input<T_batch_size>;

  template<int T_batch_size>
  using M_output = typename L_layer0::template M_output<T_batch_size>;

  template<int T_batch_size>
  void apply( const M_input<T_batch_size>& input, M_output<T_batch_size>& output )
  {
    layer0_.apply_without_activation( input, output );
  }

  template<size_t N, int i0_out, int o0_out, int... o_rest_out>
  struct LayerType;

  template<int i0_out, int o0_out>
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

  bool operator==( const Network& other ) const { return layer0_ == other.layer0_; }
};
