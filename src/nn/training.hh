#pragma once

#include <Eigen/Dense>
#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
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
  std::unique_ptr<Infer> infer = std::make_unique<Infer>();
  std::unique_ptr<BackProp> backprop = std::make_unique<BackProp>();
  std::unique_ptr<GradientDescent> gradient_descent = std::make_unique<GradientDescent>();

  void train( Network& nn,
              const Input& input,
              const std::function<PdLossWrtOutputs( const Output )>& pd_loss_wrt_outputs,
              float learning_rate )
  {
    infer->apply( nn, input );

    Output& output = infer->output();
    PdLossWrtOutputs pd = pd_loss_wrt_outputs( output );

    backprop->differentiate( nn, input, *infer, pd );

    gradient_descent->update( nn, *backprop, learning_rate );
  }

  float train_with_backoff( Network& nn,
                            const Input& input,
                            const std::function<PdLossWrtOutputs( const Output )>& pd_loss_wrt_outputs,
                            float max_learning_rate )
  {
    std::unique_ptr<Network> nn_backup = std::make_unique<Network>( nn );
    float learning_rate = max_learning_rate;
    size_t i = 0;
    infer->apply( nn, input );
    Output initial_gradient = pd_loss_wrt_outputs( infer->output() );
    Output gradient;
    do {
      nn = *nn_backup;
      if ( i++ > 5 ) {
        return 0.0;
      }
      train( nn, input, pd_loss_wrt_outputs, learning_rate );
      infer->apply( nn, input );
      learning_rate /= 10;
      gradient = pd_loss_wrt_outputs( infer->output() );
    } while ( initial_gradient.dot( gradient ) < 0 );
    return learning_rate * 10;
  }
};
