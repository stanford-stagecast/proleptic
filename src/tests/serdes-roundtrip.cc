#include <charconv>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "dnn_types.hh"
#include "network.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"

using namespace std;

struct RandomState
{
  default_random_engine prng { get_random_engine() };
  normal_distribution<float> parameter_distribution { 0, 10 };

  float sample() { return parameter_distribution( prng ); }
};

template<LayerT LayerT>
void randomize_layer( LayerT& layer, RandomState& rng )
{
  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    *( layer.weights.data() + i ) = rng.sample();
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    *( layer.biases.data() + i ) = rng.sample();
  }
}

template<NetworkT NetworkT>
void randomize_network( NetworkT& network, RandomState& rng )
{
  randomize_layer( network.first, rng );

  if constexpr ( not NetworkT::is_last ) {
    randomize_network( network.rest, rng );
  }
}

constexpr size_t iteration_count = 256;

void program_body()
{
  ios::sync_with_stdio( false );

  RandomState rng;

  for ( size_t i = 0; i < iteration_count; ++i ) {
    auto mynetwork_ptr = make_unique<DNN>();
    DNN& mynetwork = *mynetwork_ptr;
    randomize_network( mynetwork, rng );

    string serialized_nn;
    {
      serialized_nn.resize( 1048576 );
      Serializer serializer( string_span::from_view( serialized_nn ) );
      serialize( mynetwork, serializer );
      serialized_nn.resize( serializer.bytes_written() );
    }

    DNN mynetwork2;
    {
      Parser parser { serialized_nn };
      parse( mynetwork2, parser );
    }

    if ( not( mynetwork == mynetwork2 ) ) {
      throw runtime_error( "failed to roundtrip serialization-deserialization" );
    }
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
