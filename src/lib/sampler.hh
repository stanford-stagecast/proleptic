#pragma once

#include <functional>

template<class NetworkType>
struct Sampler
{
  using OutputVector = std::vector<std::pair<typename NetworkType::M_output, typename NetworkType::M_output>>;

  static void sample(
    const size_t N,
    NetworkType& neuralnetwork,
    const std::function<void( typename NetworkType::M_input&, typename NetworkType::M_output& )>& input_generator,
    OutputVector& outputs );

  // The input_generator writes a sample input and its corresponding target output.
  // The sampler returns a vector of pairs of outputs. The first member of the pair is the target output,
  // and the second is the actual output.
};
