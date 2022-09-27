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

void program_body()
{
  ios::sync_with_stdio( false );

  using TwoLayerNetwork = Network<float, 1, 1, 1>;

  TwoLayerNetwork nn;

  nn.first.weights( 0, 0 ) = 3;
  nn.first.biases( 0, 0 ) = 1;

  nn.rest.first.weights( 0, 0 ) = 7;
  nn.rest.first.biases( 0, 0 ) = 4;

  NetworkInference<TwoLayerNetwork, 1> infer;

  Eigen::Matrix<float, 1, 1> input;
  input( 0, 0 ) = 7;

  infer.apply( nn, input );

  cout << infer.output()( 0, 0 ) << "\n";

  NetworkBackPropagation<TwoLayerNetwork, 1> backprop;

  typename NetworkInference<TwoLayerNetwork, 1>::Output pd_loss_wrt_outputs
    = NetworkInference<TwoLayerNetwork, 1>::Output::Ones();

  backprop.differentiate( nn, input, infer, pd_loss_wrt_outputs );

  cout << backprop.first.weight_gradient( 0, 0 ) << "\n";
  cout << backprop.first.bias_gradient( 0, 0 ) << "\n";
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
