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

static int numerical_diff_violations = 0;
static int numerical_diff_total_count = 0;

class numerical_diff_failed : public runtime_error
{
public:
  numerical_diff_failed( string test_name )
    : runtime_error(
      ( "Test '" + test_name + "' failed: back-propagation does not generate expected gradient values." ).c_str() )
  {}
};

struct RandomState
{
  default_random_engine prng { get_random_engine() };
  normal_distribution<double> parameter_distribution { 0.0, 1.0 };

  double sample() { return parameter_distribution( prng ); }
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
void compute_numerical_gradients_layer( Layer& layer,
                                        auto input,
                                        Network& network,
                                        NetworkInference& infer,
                                        LayerBackPropagation& backprop,
                                        auto original_inference_result,
                                        double step_size,
                                        double gradient_error_percent_threshold,
                                        double gradient_error_absolute_threshold )
{
  double current_weight_gradient, current_bias_gradient, error_sum;

  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) += step_size;
    numerical_diff_total_count += 1;

    // run a new inference
    infer.apply( network, input );
    current_weight_gradient = *( backprop.weight_gradient.data() + i );

    // check the change in inference value
    auto new_inference_result = infer.output();
    auto errors = new_inference_result - original_inference_result;
    error_sum = errors.sum();

    if ( error_sum * current_weight_gradient < 0
         || ( abs( error_sum / step_size - current_weight_gradient ) / abs( current_weight_gradient )
                > gradient_error_percent_threshold
              && abs( error_sum / step_size - current_weight_gradient ) > gradient_error_absolute_threshold ) ) {
      numerical_diff_violations += 1;
    }

    // reset weight value
    *( layer.weights.data() + i ) -= step_size;
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) += step_size;
    numerical_diff_total_count += 1;

    // run a new inference
    infer.apply( network, input );
    current_bias_gradient = *( backprop.bias_gradient.data() + i );

    // check the change in inference value
    auto new_inference_result = infer.output();
    auto errors = new_inference_result - original_inference_result;
    error_sum = errors.sum();

    if ( error_sum * current_bias_gradient < 0
         || ( abs( error_sum / step_size - current_bias_gradient ) / abs( current_bias_gradient )
                > gradient_error_percent_threshold
              && abs( error_sum / step_size - current_bias_gradient ) > gradient_error_absolute_threshold ) ) {
      numerical_diff_violations += 1;
    }

    // reset bias value
    *( layer.biases.data() + i ) -= step_size;
  }

  // we need the original output of this layer as the input to the next layer
  infer.apply( network, input );
}

template<NetworkT Network, NetworkInferenceT NetworkInference, NetworkBackPropagationT NetworkBackPropagation>
void compute_numerical_gradients_network( Network& network,
                                          auto input,
                                          NetworkInference& infer,
                                          NetworkBackPropagation& backprop,
                                          auto original_inference_result,
                                          double step_size,
                                          double gradient_error_percent_threshold,
                                          double gradient_error_absolute_threshold )
{
  compute_numerical_gradients_layer( network.first,
                                     input,
                                     network,
                                     infer,
                                     backprop.first,
                                     original_inference_result,
                                     step_size,
                                     gradient_error_percent_threshold,
                                     gradient_error_absolute_threshold );

  if constexpr ( not Network::is_last ) {
    compute_numerical_gradients_network( network.rest,
                                         infer.first.output,
                                         infer.rest,
                                         backprop.rest,
                                         original_inference_result,
                                         step_size,
                                         gradient_error_percent_threshold,
                                         gradient_error_absolute_threshold );
  }
}

template<NetworkT Network, NetworkInferenceT Infer, NetworkBackPropagationT Backprop>
void run_test( RandomState& rng,
               const string& title,
               double step_size,
               double gradient_error_percent_threshold,
               double gradient_error_absolute_threshold,
               double pass_percent_threshold )
{
  Network nn;
  randomize_network( nn, rng );
  Infer infer;

  using Input = typename Infer::Input;
  using Output = typename Infer::Output;

  Input input = Input::Random();

  // run initial inference
  infer.apply( nn, input );
  // double original_inference_result = infer.output()( 0, 0 );
  Output original_inference_result = infer.output();

  // define pd_loss_wrt_outputs
  Output pd_loss_wrt_outputs = Output::Ones();

  // run backward propagation to get our computed gradients
  Backprop backprop;
  backprop.differentiate( nn, input, infer, pd_loss_wrt_outputs );

  // use numerical differentiation to get numerical gradients
  // this also compares computed and numerical gradients to test our implementation
  numerical_diff_violations = 0;
  numerical_diff_total_count = 0;
  compute_numerical_gradients_network( nn,
                                       input,
                                       infer,
                                       backprop,
                                       original_inference_result,
                                       step_size,
                                       gradient_error_percent_threshold,
                                       gradient_error_absolute_threshold );

  // the test will pass if the percentage of violation does not exceed the threshold
  double pass_rate = 1.0 - (double)numerical_diff_violations / (double)numerical_diff_total_count;
  cout << "\tTest name: " << title << endl;
  cout << "\tPass rate: " << pass_rate * 100 << "%" << endl;
  if ( pass_rate < pass_percent_threshold ) {
    throw numerical_diff_failed( title );
  }

  cout << "\tTest passed." << endl;
}

void one_layer_network_test( RandomState& rng )
{
  using OneLayerNetwork = Network<double, 1, 1>;
  using Infer = NetworkInference<OneLayerNetwork, 1>;
  using Backprop = NetworkBackPropagation<OneLayerNetwork, 1>;

  // run the test
  run_test<OneLayerNetwork, Infer, Backprop>( rng, "one_layer_network_test", 10e-7, 0.1, 0.01, 0.9 );
}

void multi_layer_network_test( RandomState& rng )
{
  using MultiLayerNetwork = Network<double, 1, 16, 16, 16, 16, 16, 1>;
  using Infer = NetworkInference<MultiLayerNetwork, 1>;
  using Backprop = NetworkBackPropagation<MultiLayerNetwork, 1>;

  // run the test
  run_test<MultiLayerNetwork, Infer, Backprop>( rng, "multi_layer_network_test", 10e-7, 0.05, 0.01, 0.9 );
}

void multi_output_network_test( RandomState& rng )
{
  using MultiOutputNetwork = Network<double, 2, 16, 16, 16, 16, 3>;
  using Infer = NetworkInference<MultiOutputNetwork, 1>;
  using Backprop = NetworkBackPropagation<MultiOutputNetwork, 1>;

  // run the test
  run_test<MultiOutputNetwork, Infer, Backprop>( rng, "multi_output_network_test", 10e-7, 0.05, 0.01, 0.9 );
}

void program_body_new()
{
  ios::sync_with_stdio( false );

  // set random seed for test stability
  RandomState rng;
  rng.prng.seed( 20221007 );

  one_layer_network_test( rng );
  multi_layer_network_test( rng );
  multi_output_network_test( rng );
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
    program_body_new();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
