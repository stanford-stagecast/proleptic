#include "backprop.hh"
#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "gradient_descent.hh"
#include "grapher.hh"
#include "inference.hh"
#include "mmap.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"
#include "training.hh"

#include <algorithm>
#include <functional>
#include <iostream>

using namespace std;

class training_failed : public runtime_error
{
public:
  training_failed( string test_name )
    : runtime_error( ( "Test '" + test_name + "' failed: training did not improve accuracy." ).c_str() )
  {}
};

class inference_failed : public runtime_error
{
public:
  inference_failed( string test_name )
    : runtime_error( ( "Test '" + test_name + "' failed: inference did not produce the expected result." ).c_str() )
  {}
};

struct RandomState
{
  default_random_engine prng { get_random_engine() };
  normal_distribution<float> parameter_distribution { 0, 1 };

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

template<class Output>
Output quadratic_loss( const Output& truth, const Output& predicted )
{
  Output difference = predicted - truth;
  return 0.5f * difference.cwiseProduct( difference );
}

template<class Output>
Output pd_quadratic_loss_wrt_outputs( const Output& truth, const Output& predicted )
{
  return predicted - truth;
}

template<NetworkT Network>
struct GradientDescentTest
{
  using Infer = NetworkInference<Network, 1>;
  using BackProp = NetworkBackPropagation<Network, 1>;
  using GradientDescent = NetworkGradientDescent<Network, 1>;
  using Training = NetworkTraining<Network, 1>;
  using Output = typename Infer::Output;
  using Input = typename Infer::Input;
  using Datum = pair<Input, Output>;

  Network nn;

  function<Output( Output, Output )> loss;
  function<Output( Output, Output )> pd_loss_wrt_outputs;
  float learning_rate;

  float test( const vector<Datum>& data )
  {
    float total_loss = 0.f;
    for ( Datum datum : data ) {
      Input x = datum.first;
      Output y = datum.second;

      Infer infer;
      infer.apply( nn, x );
      Output y_hat = infer.output();

      total_loss += loss( y, y_hat ).mean();
    }
    return total_loss / data.size();
  }

  void train( const vector<Datum>& data )
  {
    for ( Datum datum : data ) {
      Input x = datum.first;
      Output y = datum.second;

      Training training;
      training.train(
        nn, x, [&]( auto y_hat ) { return pd_loss_wrt_outputs( y, y_hat ); }, learning_rate );
    }
  }
};

template<NetworkT Network, NetworkInferenceT Inference>
void run_test( RandomState& rng,
               const string& title,
               const function<pair<typename Inference::Input, typename Inference::Output>( int )>& data_generator,
               float fraction_of_data_for_training,
               int num_data_points,
               float learning_rate,
               float max_error_for_pass = 0.25,
               float min_pass_for_success = 0.75 )
{
  Network nn;

  randomize_network( nn, rng );

  using Input = typename Inference::Input;
  using Output = typename Inference::Output;
  using Datum = pair<Input, Output>;
  using Dataset = vector<Datum>;

  GradientDescentTest<Network> test {
    nn,
    quadratic_loss<Output>,
    pd_quadratic_loss_wrt_outputs<Output>,
    learning_rate,
  };

  Dataset all_data;

  for ( int i = 0; i < num_data_points; i++ ) {
    all_data.push_back( data_generator( i ) );
  }
  shuffle( all_data.begin(), all_data.end(), rng.prng );

  int split = all_data.size() * fraction_of_data_for_training;
  Dataset training_data( all_data.begin(), all_data.begin() + split );
  Dataset test_data( all_data.begin() + split, all_data.end() );

  float loss_before = test.test( test_data );
  test.train( test_data );
  float loss_after = test.test( test_data );

  cout << endl;
  cout << "Test: " << title << endl;
  cout << "\tLoss before: " << loss_before << endl;
  cout << "\tLoss after: " << loss_after << endl;

  if ( loss_before <= loss_after ) {
    throw training_failed( title );
  }

  int total_tests = 0;
  int passed_tests = 0;

  for ( Datum test_case : test_data ) {

    Inference infer;
    infer.apply( test.nn, test_case.first );
    Output errors = ( test_case.second - infer.output() ).cwiseQuotient( test_case.second.unaryExpr( []( float x ) {
      return (float)( x == 0 ? 0.01 : x );
    } ) );

    for ( int row = 0; row < errors.rows(); row++ ) {
      float error = errors( row );
      if ( error <= max_error_for_pass ) {
        passed_tests += 1;
      } else {
        cout << "\tTest case: f(" << test_case.first << ") = " << infer.output() << " [expected "
             << test_case.second << "]" << endl;
        cout << "\tError: " << error << endl;
      }
      total_tests += 1;
    }
  }

  float pass_rate = passed_tests / (float)total_tests;
  cout << "\tPass Rate: " << pass_rate * 100 << "%" << endl;
  if ( pass_rate < min_pass_for_success ) {
    throw inference_failed( title );
  }

  // Passing is fairly lenient by default: be within 25% of the right answer at least 75% of the time.

  cout << "\tTest passed." << endl;
}

void one_layer_network_test( RandomState& rng )
{
  using OneLayerNetwork = Network<float, 1, 1>;
  using Infer = NetworkInference<OneLayerNetwork, 1>;
  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  uniform_real_distribution dist( -10.0, 10.0 );
  run_test<OneLayerNetwork, Infer>(
    rng,
    "3x + 1",
    [&dist, &rng]( int i ) {
      (void)i;
      Input in = Input::Constant( dist( rng.prng ) );
      Output out = 3.0 * in + Output::Constant( 1.0 );
      return make_pair( in, out );
    },
    0.75,
    10000,
    0.01 );
}

void multi_layer_network_test( RandomState& rng )
{
  // Since the output values are so high (0..100), this network is inherently less stable; to compensate, we lower
  // the learning rate, which in turn would require more interations to converge.  However, since more iterations
  // takes longer, and this test shouldn't block everything for too long, we also adjust our accuracy bounds to
  // be more tolerant---within 75% of the right answer at least 75% of the time.
  using MultiLayerNetwork = Network<float, 1, 16, 16, 16, 1>;
  using Infer = NetworkInference<MultiLayerNetwork, 1>;
  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  uniform_real_distribution dist( 0.0, 10.0 );
  run_test<MultiLayerNetwork, Infer>(
    rng,
    "x * x",
    [&dist, &rng]( int i ) {
      (void)i;
      Input in = Input::Constant( dist( rng.prng ) );
      Output out = in.cwiseProduct( in );
      return make_pair( in, out );
    },
    0.75,
    1000000,
    0.00001,
    0.75 );
}

void multi_output_network_test( RandomState& rng )
{
  // Since this is a classification problem, the accuracy constraints are loosened such that anything negative
  // counts as "-1" and anything positive counts as "+1".
  using MultiOutputNetwork = Network<float, 1, 16, 32, 16, 2>;
  using Infer = NetworkInference<MultiOutputNetwork, 1>;
  using Input = typename Infer::Input;
  using Output = typename Infer::Output;
  uniform_real_distribution dist( 0.0, 1.0 );
  run_test<MultiOutputNetwork, Infer>(
    rng,
    "3.00 < x < 7.00",
    [&dist, &rng]( int i ) {
      (void)i;
      Input in = Input::Constant( dist( rng.prng ) );
      Output out;
      float t1 = in( 0 ) < 0.3;
      float t2 = in( 0 ) < 0.7;
      t1 = 2 * t1 - 1;
      t2 = 2 * t2 - 1;
      out << t1, t2;
      return make_pair( in, out );
    },
    0.75,
    100000,
    0.001,
    1.0 );
}

void program_body()
{
  ios::sync_with_stdio( false );

  RandomState rng;

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
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
