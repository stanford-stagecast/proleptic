#include "sampler.hh"
#include "dnn_types.hh"
#include "network.hh"

using namespace std;

template<class NetworkType>
void Sampler<NetworkType>::sample(
  const size_t N,
  NetworkType& neuralnetwork,
  const function<void( typename NetworkType::M_input&, typename NetworkType::M_output& )>& input_generator,
  OutputVector& outputs )
{
  outputs.clear();
  for ( size_t i = 0; i < N; ++i ) {
    outputs.emplace_back();
    typename NetworkType::M_input sample;
    input_generator( sample, outputs.back().first );
    neuralnetwork.apply( sample, outputs.back().second );
  }
}

template struct Sampler<DNN>;
