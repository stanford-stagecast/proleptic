#pragma once

#include <concepts>

#include "layer.hh"

// The Network models a sequence of neural-network layers.
// It transforms a vector of size "i0" into a vector whose size is the output size
// of the last layer. The Network is defined recursively: each Network contains
// one layer (layer0), and then either (a) the "rest" of the Network (recursive case)
// or (b) nothing else (the base case).

// Here is the recursive case. "i0" is the first layer's input size,
// and "o0" is its output size (also the input size of the next layer).
// The rest of the input/output sizes come afterward; each layer's output size
// is the input size of the next layer.
template<std::floating_point T, int i0, int o0, int... o_rest>
struct Network
{
  // Type of the entries, the first layer, and the rest of the network
  using type = T;
  using Layer0 = Layer<T, i0, o0>;
  using Rest = Network<T, o0, o_rest...>;

  // What the network contains: one layer, plus the rest of the network
  Layer0 first {};
  Rest rest {};

  // Helpful constants
  static constexpr size_t num_layers = Rest::num_layers + 1;
  static constexpr size_t num_params = Layer0::num_params + Rest::num_params;
  static constexpr size_t input_size = Layer0::input_size;
  static constexpr size_t output_size = Rest::output_size;
  static constexpr bool is_last = false;

  // Comparison
  bool operator==( const Network& other ) const = default;
};

// Here is the base case. This is similar to the recursive case, but has no "rest" member.
// It only contains one layer, defined by one input size and one output size.
template<std::floating_point T, int i0, int o0>
struct Network<T, i0, o0>
{
  // Type of the entries and sole layer
  using type = T;
  using Layer0 = Layer<T, i0, o0>;

  // What the network contains: one layer
  Layer0 first {};

  // Helpful constants
  static constexpr size_t num_layers = 1;
  static constexpr size_t num_params = Layer0::num_params;
  static constexpr size_t input_size = Layer0::input_size;
  static constexpr size_t output_size = Layer0::output_size;
  static constexpr bool is_last = true;

  // Comparison
  bool operator==( const Network& other ) const = default;
};

template<class ProposedNetwork>
concept NetworkT = requires( ProposedNetwork c )
{
  []<typename T, int i0, int o0, int... o_rest>( Network<T, i0, o0, o_rest...>& ) {}( c );
};
;
