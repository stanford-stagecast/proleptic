#include "sampler.hh"
#include "dnn_types.hh"
#include "network.hh"
#include "nnops.hh"

using namespace std;

template<int T_batch_size, class NetworkType, typename OutputType>
void Sampler<T_batch_size, NetworkType, OutputType>::sample( const size_t num_batches,
                                                             NetworkType& neuralnetwork,
                                                             const InputGenerator& input_generator,
                                                             const OutputTransformer& output_transformer,
                                                             Output& outputs )
{
  outputs.clear();
  outputs.resize( num_batches * T_batch_size );

  typename NetworkType::template M_input<T_batch_size> input_batch;

  typename NetworkType::template M_input<1> single_input;
  typename NetworkType::template M_output<1> single_output;

  typename NetworkType::template Activations<T_batch_size> activations;

  for ( size_t batch = 0; batch < num_batches; ++batch ) {
    // step 1: generate one batch of input
    for ( size_t elem = 0; elem < T_batch_size; ++elem ) {
      input_generator( single_input, single_output );
      input_batch.row( elem ) = single_input;
      // store target output
      output_transformer( single_output, outputs.at( batch * T_batch_size + elem ).first );
    }

    // step 2: apply the network to the batch
    apply<T_batch_size>( neuralnetwork, input_batch, activations );

    // step 3: transform the actual outputs
    for ( size_t elem = 0; elem < T_batch_size; ++elem ) {
      output_transformer( activations.output().row( elem ), outputs.at( batch * T_batch_size + elem ).second );
    }
  }
}

template struct Sampler<BATCH_SIZE, DNN, float>;
