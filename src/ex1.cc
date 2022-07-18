#include <iostream>
#include <string>

#include "network.hh"
#include "pp.hh"

using namespace std;

void program_body( const string& iterations )
{
  const unsigned int num_iterations = stoi( iterations );

  using DNN = Network<float, 1, 16, 128, 128, 128, 128, 128, 128, 128, 128, 1>;
  DNN mynetwork;

  DNN::A_storage s1, s2, s3;

  DNN::M_input& input = *reinterpret_cast<DNN::M_input*>( s1.data() );

  input( 0, 0 ) = 15;

  for ( unsigned int i = 0; i < num_iterations; ++i ) {
    mynetwork.apply( s1, s2, s3 );
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
