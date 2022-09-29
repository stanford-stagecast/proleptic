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

#include <algorithm>
#include <functional>
#include <iostream>

using namespace std;

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
Output quadratic_loss( Output truth, Output predicted )
{
  Output difference = predicted - truth;
  return 0.5f * difference.cwiseProduct( difference );
}

template<class Output>
Output pd_quadratic_loss_wrt_outputs( Output truth, Output predicted )
{
  return predicted - truth;
}

template<NetworkT Network>
struct GradientDescentTest
{
  using Infer = NetworkInference<Network, 1>;
  using BackProp = NetworkBackPropagation<Network, 1>;
  using GradientDescent = NetworkGradientDescent<Network, 1>;
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

  float train( const vector<Datum>& data )
  {
    float total_loss = 0.f;
    for ( Datum datum : data ) {
      Input x = datum.first;
      Output y = datum.second;

      Infer infer;
      infer.apply( nn, x );
      Output y_hat = infer.output();

      total_loss += loss( y, y_hat ).mean();

      BackProp backprop;
      backprop.differentiate( nn, x, infer, pd_loss_wrt_outputs( y, y_hat ) );

      GradientDescent gradient_descent;
      gradient_descent.update( nn, backprop, learning_rate );
    }

    return total_loss / data.size();
  }
};

void one_layer_network_test( RandomState& rng )
{
  using OneLayerNetwork = Network<float, 1, 1>;
  OneLayerNetwork nn;

  randomize_network( nn, rng );

  using Matrix11 = Eigen::Matrix<float, 1, 1>;
  GradientDescentTest<OneLayerNetwork> test {
    nn,
    quadratic_loss<Matrix11>,
    pd_quadratic_loss_wrt_outputs<Matrix11>,
    0.01,
  };

  using Dataset = vector<pair<Matrix11, Matrix11>>;
  Dataset all_data;
  for ( int i = 0; i < 10000; i++ ) {
    float x = ( ( rand() % 1000 ) - 500 ) / 50.f;
    float y = 3.f * x + 1.f;
    Matrix11 X( x );
    Matrix11 Y( y );
    all_data.push_back( pair( X, Y ) );
  }

  int split = all_data.size() * 3 / 4;
  Dataset training_data( all_data.begin(), all_data.begin() + split );
  Dataset test_data( all_data.begin() + split, all_data.end() );

  cout << endl << "One layer network [3x + 1]:" << endl;
  cout << fixed << setprecision( 10 );
  cout << "\tLoss before: " << test.test( test_data ) << endl;
  cout << "\tLoss during: " << test.train( training_data ) << endl;
  cout << "\tLoss after:  " << test.test( test_data ) << endl;

  auto x = test_data[0].first;
  auto y = test_data[0].second;
  NetworkInference<OneLayerNetwork, 1> infer;
  infer.apply( test.nn, x );
  auto y_hat = infer.output();

  cout << setprecision( 2 );
  cout << "\tExample: 3.00(" << x << ") + 1.00 = " << y << " ?= " << y_hat << " (diff = " << y_hat - y << ")"
       << endl;
}

void multi_layer_network_test( RandomState& rng )
{
  using MultiLayerNetwork = Network<float, 1, 16, 16, 16, 1>;
  MultiLayerNetwork nn;

  randomize_network( nn, rng );

  using Matrix11 = Eigen::Matrix<float, 1, 1>;
  GradientDescentTest<MultiLayerNetwork> test {
    nn,
    quadratic_loss<Matrix11>,
    pd_quadratic_loss_wrt_outputs<Matrix11>,
    0.01,
  };

  using Dataset = vector<pair<Matrix11, Matrix11>>;
  Dataset all_data;
  for ( int i = 0; i < 10000; i++ ) {
    float x = ( rand() % 1000 ) / 1000.f;
    float y = pow( x, 2 );
    Matrix11 X( x );
    Matrix11 Y( y );
    all_data.push_back( pair( X, Y ) );
  }

  int split = all_data.size() * 3 / 4;
  Dataset training_data( all_data.begin(), all_data.begin() + split );
  Dataset test_data( all_data.begin() + split, all_data.end() );

  cout << endl << "Multi-layer network [pow(x, 2)]:" << endl;
  cout << fixed << setprecision( 10 );
  cout << "\tLoss before: " << test.test( test_data ) << endl;
  cout << "\tLoss during: " << test.train( training_data ) << endl;
  cout << "\tLoss after:  " << test.test( test_data ) << endl;

  auto x = test_data[0].first;
  auto y = test_data[0].second;
  NetworkInference<MultiLayerNetwork, 1> infer;
  infer.apply( test.nn, x );
  auto y_hat = infer.output();

  cout << setprecision( 2 );
  cout << "\tExample: pow(" << x << ", 2.00) = " << y << " ?= " << y_hat << " (diff = " << y_hat - y << ")" << endl;
}

void multi_output_network_test( RandomState& rng )
{
  using MultiOutputNetwork = Network<float, 1, 16, 16, 2>;
  MultiOutputNetwork nn;

  randomize_network( nn, rng );

  using Matrix11 = Eigen::Matrix<float, 1, 1>;
  using Matrix12 = Eigen::Matrix<float, 1, 2>;
  GradientDescentTest<MultiOutputNetwork> test {
    nn,
    quadratic_loss<Matrix12>,
    pd_quadratic_loss_wrt_outputs<Matrix12>,
    0.01,
  };

  using Dataset = vector<pair<Matrix11, Matrix12>>;
  Dataset all_data;
  for ( int i = 0; i < 10000; i++ ) {
    float x = ( rand() % 1000 ) / 100.0;
    float y = x > 3.0;
    float z = x < 3.0;
    Matrix11 X( (float)x );
    Matrix12 Y( (float)y, (float)z );
    all_data.push_back( pair( X, Y ) );
  }

  int split = all_data.size() * 3 / 4;
  Dataset training_data( all_data.begin(), all_data.begin() + split );
  Dataset test_data( all_data.begin() + split, all_data.end() );

  cout << endl << "Multi-output network [3.00 < x < 7.00]:" << endl;
  cout << fixed << setprecision( 10 );
  cout << "\tLoss before: " << test.test( test_data ) << endl;
  cout << "\tLoss during: " << test.train( training_data ) << endl;
  cout << "\tLoss after:  " << test.test( test_data ) << endl;

  auto x = test_data[0].first;
  auto y = test_data[0].second;
  NetworkInference<MultiOutputNetwork, 1> infer;
  infer.apply( test.nn, x );
  auto y_hat = infer.output();

  cout << setprecision( 2 );
  cout << "\tExample: 3.00 < " << x << " < 7.00 = [" << y << "] ?= [" << y_hat << "] (diff = [" << y_hat - y << "])"
       << endl;
}

void program_body()
{
  ios::sync_with_stdio( false );
  srand( 1 );

  RandomState rng;
  rng.prng.seed( 1 );

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
