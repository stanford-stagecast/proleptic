#pragma once

#include <Eigen/Dense>
#include <concepts>
#include <type_traits>

#include "inference.hh"

template<LayerT Layer, int batch_size>
struct LayerBackPropagation
{
  using Inference = LayerInference<Layer, batch_size>;

  using Error = typename Inference::Output;
  using WeightTimesErrorNextLayer = typename Inference::Output;
  using WeightTimesError = typename Inference::Input;

  void differentiate( const Layer& layer,
                      const typename Inference::Input& input,
                      const Inference& inference,
                      const WeightTimesErrorNextLayer& wte_next_layer )
  {
    typename Inference::Output pd_activation_wrt_unactivated_output
      = inference.output.unaryExpr( []( const auto val ) { return val > 0 ? 1 : leaky_constant; } );
    Error error = wte_next_layer.cwiseProduct( pd_activation_wrt_unactivated_output );
    weight_times_error.noalias() = error * layer.weights.transpose();

    bias_gradient.noalias() = error.colwise().sum();
    weight_gradient.noalias() = input.transpose() * error;
  }

  // State for this layer's backpropagation
  typename Layer::Weights weight_gradient {};
  typename Layer::Biases bias_gradient {};
  WeightTimesError weight_times_error {};
};

template<NetworkT Network, int batch_size, bool is_last_T>
struct NetworkBackPropagationHelper;

template<NetworkT Network, int batch_size>
using NetworkBackPropagation = NetworkBackPropagationHelper<Network, batch_size, Network::is_last>;

// Here is the recursive case: backpropagation for a non-terminal Network.
template<NetworkT Network, int batch_size>
struct NetworkBackPropagationHelper<Network, batch_size, false>
{
  static_assert( not Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = false;

  using NetworkInfer = NetworkInference<Network, batch_size>;
  using LayerBackProp = LayerBackPropagation<typename Network::Layer0, batch_size>;
  using RestBackProp = NetworkBackPropagation<typename Network::Rest, batch_size>;
  using Input = typename LayerBackProp::Inference::Input;

  // Backpropagation state
  LayerBackProp first {};
  RestBackProp rest {};

  void differentiate( const Network& network,
                      const Input& input,
                      const NetworkInfer& inference,
                      const typename NetworkInfer::Output& pd_loss_wrt_outputs )
  {
    rest.differentiate( network.rest, inference.first.output, inference.rest, pd_loss_wrt_outputs );

    first.differentiate( network.first, input, inference.first, rest.first.weight_times_error );
  }
};

// Here is the base case: backpropagation for a single-layer Network.
template<NetworkT Network, int batch_size>
struct NetworkBackPropagationHelper<Network, batch_size, true>
{
  static_assert( Network::is_last );

  // Helpful boolean to indicate if this is the last layer
  static constexpr bool is_last = true;

  using NetworkInfer = NetworkInference<Network, batch_size>;
  using LayerBackProp = LayerBackPropagation<typename Network::Layer0, batch_size>;
  using Input = typename LayerBackProp::Inference::Input;

  // Backpropagation state
  LayerBackProp first {};

  void differentiate( const Network& network,
                      const Input& input,
                      const NetworkInfer& inference,
                      const typename NetworkInfer::Output& pd_loss_wrt_outputs )
  {
    first.differentiate( network.first, input, inference.first, pd_loss_wrt_outputs );
  }
};
