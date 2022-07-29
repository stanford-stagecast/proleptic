#include "serdes.hh"
#include "dnn_types.hh"

using namespace std;

static constexpr array<char, 8> layer_header_str = { 'l', 'a', 'y', 'e', 'r', ' ', ' ', ' ' };
static constexpr array<char, 8> network_header_str = { 'n', 'e', 't', 'w', 'o', 'r', 'k', ' ' };

static constexpr string_view layer_header_str_view { layer_header_str.data(), layer_header_str.size() };

static constexpr string_view network_header_str_view { network_header_str.data(), network_header_str.size() };

static constexpr array<char, 8> test_input_header_str { 'i', 'n', 'p', 'u', 't', ' ', ' ', ' ' };
static constexpr array<char, 8> test_output_header_str { 'o', 'u', 't', 'p', 'u', 't', ' ', ' ' };
static constexpr array<char, 8> roundtrip_test_end_str { 's', 't', 'o', 'p', 'h', 'e', 'r', 'e' };

static constexpr string_view test_input_header_str_view { test_input_header_str.data(),
                                                          test_input_header_str.size() };

static constexpr string_view test_output_header_str_view { test_output_header_str.data(),
                                                           test_output_header_str.size() };

static constexpr string_view roundtrip_test_end_str_view { roundtrip_test_end_str.data(),
                                                           roundtrip_test_end_str.size() };

namespace LayerSerDes {
template<typename T_layer>
void serialize( const T_layer& layer, Serializer& out )
{
  out.string( layer_header_str_view );
  out.integer( T_layer::input_size );
  out.integer( T_layer::output_size );

  for ( unsigned int i = 0; i < layer.weights().size(); ++i ) {
    out.floating( *( layer.weights().data() + i ) );
  }

  for ( unsigned int i = 0; i < layer.biases().size(); ++i ) {
    out.floating( *( layer.biases().data() + i ) );
  }
}

template<typename T_layer>
void parse( T_layer& layer, Parser& in )
{
  array<char, 8> layer_header;
  string_span layer_header_span { layer_header.data(), layer_header.size() };
  in.string( layer_header_span );
  if ( layer_header != layer_header_str ) {
    throw runtime_error( "missing layer header" );
  }

  size_t input_size, output_size;
  in.integer( input_size );
  in.integer( output_size );

  if ( input_size != T_layer::input_size or output_size != T_layer::output_size ) {
    throw runtime_error( "input or output size mismatch" );
  }

  for ( unsigned int i = 0; i < layer.weights().size(); ++i ) {
    in.floating( *( layer.weights().data() + i ) );
  }

  for ( unsigned int i = 0; i < layer.biases().size(); ++i ) {
    in.floating( *( layer.biases().data() + i ) );
  }
}
}

namespace NetworkSerDes {
template<typename T_network>
void serialize_internal( const T_network& network, Serializer& out )
{
  if constexpr ( T_network::is_last_layer ) {
    return;
  } else {
    NetworkSerDes::serialize( network.rest(), out );
  }
}

template<typename T_network>
void parse_internal( T_network& network, Parser& in )
{
  LayerSerDes::parse( network.first_layer(), in );

  if constexpr ( T_network::is_last_layer ) {
    return;
  } else {
    NetworkSerDes::parse( network.rest(), in );
  }
}

template<typename T_network>
void serialize( const T_network& network, Serializer& out )
{
  out.string( network_header_str_view );
  LayerSerDes::serialize( network.first_layer(), out );

  serialize_internal( network, out );

  /* Consistency check for serialization and parsing 
     (1) generate 16 random inputs (type T_network::M_input)
     (2) apply each one through the network to produce an output
     (3) serialize each input (maybe with an "input   " header)
         and its corresponding output (maybe with an "output  " header) */

  // initialize some random input
  typename T_network::template M_input<16> my_input;
  my_input.Random();

  // initialize output container
  typename T_network::template Activations<16> activations;
  network.apply( my_input, activations );

  // serialize input header
  out.string( test_input_header_str_view );

  // serialize input
  for ( unsigned int i = 0; i < my_input.size(); ++i ) {
    out.floating( *( my_input.data() + i ) );
  }

  // serialize output header
  out.string( test_output_header_str_view );

  // serialize output
  for ( unsigned int i = 0; i < activations.output().size(); ++i ) {
    out.floating( *( activations.output().data() + i ) );
  }

  // serialize end of test
  out.string( roundtrip_test_end_str_view );
}

template<typename T_network>
void parse( T_network& network, Parser& in )
{
  array<char, 8> network_header;
  string_span network_header_span { network_header.data(), network_header.size() };
  in.string( network_header_span );
  if ( network_header != network_header_str ) {
    throw runtime_error( "missing network header" );
  }

  parse_internal( network, in );

  /* Consistency check for serialization and parsing 
     (1) parse each of the 16 random inputs (each starting with the
         "input   " header)
     (2) parse each of the 16 corresponding outputs (each starting
         with the "output  " header)
     (3) apply the parsed network to each of the inputs
     (4) confirm that the computed output matches the parsed expectation
     (by "matched" maybe we mean it's within .01% or something -- doesn't
     have to be perfectly exact in the == sense between two floats) */

  // input header demarcates input
  array<char, 8> test_input_header;
  string_span test_input_header_span { test_input_header.data(), test_input_header.size() };

  // verify input header
  in.string( test_input_header_span );
  if ( test_input_header != test_input_header_str ) {
    throw runtime_error( "missing test input header " + string( test_input_header_span ) );
  }

  // read in input
  typename T_network::template M_input<16> my_input;

  for ( unsigned int i = 0; i < my_input.size(); ++i ) {
    in.floating( *( my_input.data() + i ) );
  }

  // output header demarcates output
  array<char, 8> test_output_header;
  string_span test_output_header_span { test_output_header.data(), test_output_header.size() };

  // verify output header
  in.string( test_output_header_span );
  if ( test_output_header != test_output_header_str ) {
    throw runtime_error( "missing test output header" );
  }

  // read in output
  typename T_network::template Activations<16> activations;

  for ( unsigned int i = 0; i < activations.output().size(); ++i ) {
    in.floating( *( activations.output().data() + i ) );
  }

  // read in end of test
  array<char, 8> test_end_point;
  string_span test_end_point_span { test_end_point.data(), test_end_point.size() };

  // verify end of test
  in.string( test_end_point_span );
  if ( test_end_point != roundtrip_test_end_str ) {
    throw runtime_error( "mismatched roundtrip test ends" );
  }
}
}

template void NetworkSerDes::serialize<DNN>( const DNN&, Serializer& );
template void NetworkSerDes::parse<DNN>( DNN&, Parser& );
