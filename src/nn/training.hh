#pragma once

#include <Eigen/Dense>
#include <concepts>
#include <functional>
#include <type_traits>

#include "gradient_descent.hh"
#include "inference.hh"
#include "network.hh"

template<NetworkT Network, int batch_size>
struct NetworkTraining
{
  using Infer = NetworkInference<Network, batch_size>;
  using BackProp = NetworkBackPropagation<Network, batch_size>;
  using GradientDescent = NetworkGradientDescent<Network, batch_size>;

  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  using PdLossWrtOutputs = typename Infer::Output;

  void train( Network& nn,
              const Input& input,
              const std::function<PdLossWrtOutputs( const Output )>& pd_loss_wrt_outputs,
              float learning_rate )
  {
    Infer infer;
    infer.apply( nn, input );

    Output output = infer.output();
    PdLossWrtOutputs pd = pd_loss_wrt_outputs( output );

    BackProp backprop;
    backprop.differentiate( nn, input, infer, pd );

    GradientDescent gradient_descent;
    gradient_descent.update( nn, backprop, learning_rate );
  }
};
