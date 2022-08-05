#pragma once

#include <array>
#include <tuple>

#include "layer.hh"
#include "nnops.hh"

// The Network class models a sequence of neural-network layers.
// It transforms a vector of size "i0" into a vector whose size is the output size
// of the last layer. The Network is defined recursively: each Network contains
// one layer (layer0), and then either (a) the "rest" of the Network (recursive case)
// or (b) nothing else (the base case).

// Here is the recursive case. "i0" is the first layer's input size,
// and "o0" is its output size (also the input size of the next layer).
// The rest of the input/output sizes come afterward; each layer's output size
// is the input size of the next layer.
template<typename T, int i0, int o0, int... o_rest>
class Network
{
public:
  // Types of the first layer and of the rest of the network
  using L_layer0 = Layer<T, i0, o0>;
  using N_rest = Network<T, o0, o_rest...>;

  // The type of entry (e.g. float or double)
  using type = T;

  // Helpful constants
  static constexpr size_t num_layers = N_rest::num_layers + 1;
  static constexpr size_t num_params = L_layer0::num_params + N_rest::num_params;
  static constexpr size_t input_size = i0;
  static constexpr size_t output_size = N_rest::output_size;

  // Accessors
  const L_layer0& first_layer() const { return layer0_; }
  const N_rest& rest() const { return rest_; }
  L_layer0& first_layer() { return layer0_; }
  N_rest& rest() { return rest_; }

  // Types of the input and output matrices (templated by batch size)
  template<int T_batch_size>
  using M_input = typename L_layer0::template M_input<T_batch_size>;

  template<int T_batch_size>
  using M_output = typename N_rest::template M_output<T_batch_size>;

  // A recursive type that contains the activations of the whole neural network
  template<int T_batch_size>
  class Activations
  {
    // Activations (outputs) of the current layer
    typename L_layer0::template M_output<T_batch_size> layer0_ {};

    // Activations of the rest of the neural network
    typename N_rest::template Activations<T_batch_size> rest_ {};

  public:
    // Accessors
    const decltype( layer0_ )& first_layer() const { return layer0_; }
    decltype( layer0_ )& first_layer() { return layer0_; }

    const decltype( rest_ )& rest() const { return rest_; }
    decltype( rest_ )& rest() { return rest_; }

    const M_output<T_batch_size>& output() const { return rest().output(); }
    M_output<T_batch_size>& output() { return rest().output(); }

    // Helpful boolean to indicate if this is the last layer
    static constexpr bool is_last_layer = false;
  };

  // Apply the neural network to a given input, writing the activations as output.
  // This applies the current layer and then recurses to the rest of the network.
  template<int T_batch_size>
  void apply( const M_input<T_batch_size>& input, Activations<T_batch_size>& activations ) const
  {
    apply_fully_connected<L_layer0, T_batch_size>( layer0_, input, activations.first_layer() );
    activate<L_layer0, T_batch_size>( activations.first_layer() );
    rest_.apply( activations.first_layer(), activations.rest() );
  }

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last_layer = false;

  bool operator==( const Network& other ) const = default;

private:
  // What the network contains: one layer, plus the rest of the network
  L_layer0 layer0_ {};
  N_rest rest_ {};
};

// Here is the base case. This is similar to the recursive case, but has no "rest" member.
// It only contains one layer, defined by one input size and one output size.
template<class T, int i0, int o0>
class Network<T, i0, o0>
{
public:
  // Type of the layer
  using L_layer0 = Layer<T, i0, o0>;

  // The type of entry (e.g. float or double)
  using type = T;

  // Helpful constants
  static constexpr size_t num_layers = 1;
  static constexpr size_t num_params = L_layer0::num_params;
  static constexpr size_t input_size = L_layer0::input_size;
  static constexpr size_t output_size = L_layer0::output_size;

  // Accessors
  const L_layer0& first_layer() const { return layer0_; }
  L_layer0& first_layer() { return layer0_; }

  // Types of the input and output matrices (templated by batch size)
  template<int T_batch_size>
  using M_input = typename L_layer0::template M_input<T_batch_size>;

  template<int T_batch_size>
  using M_output = typename L_layer0::template M_output<T_batch_size>;

  // The base case of a recursive type that contains the activations
  template<int T_batch_size>
  class Activations
  {
    typename L_layer0::template M_output<T_batch_size> layer0_ {};

  public:
    // Accessors
    const M_output<T_batch_size>& output() const { return layer0_; }
    M_output<T_batch_size>& output() { return layer0_; }

    const M_output<T_batch_size>& first_layer() const { return layer0_; }
    M_output<T_batch_size>& first_layer() { return layer0_; }

    // Helpful boolean to indicate if this is the last layer
    static constexpr bool is_last_layer = true;
  };

  // Apply the neural network to a given input, writing the activations as output.
  // This just applies the current layer (the last layer in a network).
  template<int T_batch_size>
  void apply( const M_input<T_batch_size>& input, Activations<T_batch_size>& activations ) const
  {
    apply_fully_connected<L_layer0, T_batch_size>( layer0_, input, activations.first_layer() );
  }

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last_layer = true;

  bool operator==( const Network& other ) const = default;

private:
  // What the network contains: one layer
  L_layer0 layer0_ {};
};
