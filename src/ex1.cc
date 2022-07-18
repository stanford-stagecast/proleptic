#include <iostream>

#include "network.hh"
#include "pp.hh"

using namespace std;

int main()
{
  using DNN = Network<float, 1, 2, 2, 1>;
  DNN mynetwork;

  DNN::A_storage s1, s2, s3;

  DNN::M_input& input = *reinterpret_cast<DNN::M_input*>( s1.data() );

  input( 0, 0 ) = 8;
  input( 0, 1 ) = -16;

  auto& layer1 = mynetwork.layer0;
  auto& layer2 = mynetwork.next.layer0;

  layer1.weights().setOnes();
  layer1.biases().setOnes();
  layer2.weights().setOnes();
  layer2.biases().setOnes();

  const DNN::M_output& output = mynetwork.apply( s1, s2, s3 );

  pp( input );
  pp( layer1.weights() );
  pp( layer1.biases() );
  pp( layer2.weights() );
  pp( layer2.biases() );
  pp( output );
}

// the i,j element of the resulting matrix is the sum of row i of the left matrix times column j of the right matrix
// need same number of rows of the first matrix as columns of the second matrix
