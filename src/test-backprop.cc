#include "backprop.hh"
#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "inference.hh"
#include "mmap.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"

#include <iostream>

using namespace std;

struct RandomState
{
  default_random_engine prng { get_random_engine() };
  normal_distribution<float> parameter_distribution { 0, 10 };

  float sample() { return parameter_distribution( prng ); }
};

template<LayerT Layer>
void randomize_layer( Layer& layer, RandomState& rng )
{
  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) = rng.sample();
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) = rng.sample();
  }
}

template<NetworkT Network>
void randomize_network( Network& network, RandomState& rng )
{
  randomize_layer( network.first, rng );

  if constexpr ( not Network::is_last ) {
    randomize_network( network.rest, rng );
  }
}

template<LayerT Layer,
         NetworkT Network,
         NetworkInferenceT NetworkInference,
         LayerBackPropagationT LayerBackPropagation>
bool test_layer( Layer& layer,
                 auto input,
                 Network& network,
                 NetworkInference& infer,
                 LayerBackPropagation& backprop,
                 float original_inference_result )
{
  bool test_pass_flag = true;
  float current_weight_gradient, current_bias_gradient;

  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) += 0.001;

    // do a new inference and check the output
    infer.apply( network, input );
    float new_inference_result = infer.output()( 0, 0 );
    current_weight_gradient = *( backprop.weight_gradient.data() + i );
    if ( abs( new_inference_result - original_inference_result - 0.001 * current_weight_gradient )
         > abs( 0.01 * original_inference_result ) ) {
      test_pass_flag = false;
    }

    *( layer.weights.data() + i ) -= 0.001;
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) += 0.001;

    // do a new inference and check the output
    infer.apply( network, input );
    float new_inference_result = infer.output()( 0, 0 );
    current_bias_gradient = *( backprop.bias_gradient.data() + i );
    if ( abs( new_inference_result - original_inference_result - 0.001 * current_bias_gradient )
         > abs( 0.01 * original_inference_result ) ) {
      test_pass_flag = false;
    }

    *( layer.biases.data() + i ) -= 0.001;
  }

  return test_pass_flag;
}

template<NetworkT Network, NetworkInferenceT NetworkInference, NetworkBackPropagationT NetworkBackPropagation>
bool test_network( Network& network,
                   auto input,
                   NetworkInference& infer,
                   NetworkBackPropagation& backprop,
                   float original_inference_result )
{
  bool test_pass_flag = true;

  test_pass_flag = test_layer( network.first, input, network, infer, backprop.first, original_inference_result );

  if ( !test_pass_flag ) {
    return test_pass_flag;
  }

  if constexpr ( not Network::is_last ) {
    test_pass_flag
      = test_network( network.rest, infer.first.output, infer.rest, backprop.rest, original_inference_result );
  }

  return test_pass_flag;
}

void program_body()
{
  ios::sync_with_stdio( false );

  using TwoLayerNetwork = Network<float, 100, 100, 100, 100, 100, 1>;

  TwoLayerNetwork nn;

  RandomState rng;

  // randomized network
  randomize_network( nn, rng );

  NetworkInference<TwoLayerNetwork, 1> infer;

  // randomized inputs
  Eigen::Matrix<float, 1, 100> input = Eigen::Matrix<float, 1, 100>::Random();

  // inference
  infer.apply( nn, input );
  float original_inference_result = infer.output()( 0, 0 );

  NetworkBackPropagation<TwoLayerNetwork, 1> backprop;

  typename NetworkInference<TwoLayerNetwork, 1>::Output pd_loss_wrt_outputs
    = NetworkInference<TwoLayerNetwork, 1>::Output::Ones();

  backprop.differentiate( nn, input, infer, pd_loss_wrt_outputs );

  // testing through numerical differentiation
  bool test_pass_flag = test_network( nn, input, infer, backprop, original_inference_result );
  if ( test_pass_flag ) {
    cout << "Backprop test passed."
         << "\n";
  } else {
    cout << "Backprop test NOT passed."
         << "\n";
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 1 ) {
    cerr << "Usage: " << argv[0] << "\n";
    return EXIT_FAILURE;
  }

  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
