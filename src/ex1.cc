#include <iostream>
#include <memory>
#include <string>

#include "network.hh"
#include "pp.hh"

using namespace std;

void program_body( const string& iterations )
{
  const unsigned int num_iterations = stoi( iterations );

  using DNN = Network<float, 1, 16, 16, 1, 30, 2560, 1>;

  auto mynetwork_ptr = make_unique<DNN>();
  DNN& mynetwork = *mynetwork_ptr;

  DNN::M_input my_input;
  DNN::M_output my_output;

  cout << "Number of layers: " << mynetwork.num_layers << "\n";

  for ( unsigned int i = 0; i < num_iterations; ++i ) {
    mynetwork.apply( my_input, my_output );
  }
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " iterations\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
