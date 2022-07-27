#pragma once

#include <Eigen/Dense>
#include <functional>

template<int T_batch_size, class NetworkType, typename OutputType>
struct Sampler
{
  using Output = std::vector<std::pair<OutputType, OutputType>>;

  using InputGenerator = std::function<void( typename NetworkType::template M_input<1>&,
                                             typename NetworkType::template M_output<1>& )>;

  using OutputTransformer = std::function<void( const typename NetworkType::template M_output<1>&, OutputType& )>;

  static void sample( const size_t num_batches,
                      NetworkType& neuralnetwork,
                      const InputGenerator& input_generator,
                      const OutputTransformer& output_transformer,
                      Output& outputs );
};

// The input_generator writes a sample input and its corresponding target output.

// The output_transformer post-processes each output, e.g. to turn it into something graphable (like a float).

// The sampler returns a vector of pairs of outputs. The first member of each pair is the target output,
// and the second is the actual output.
