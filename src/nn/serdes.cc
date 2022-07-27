#include "serdes.hh"
#include "dnn_types.hh"

using namespace std;

static constexpr array<char, 8> layer_header_str = { 'l', 'a', 'y', 'e', 'r', ' ', ' ', ' ' };
static constexpr array<char, 8> network_header_str = { 'n', 'e', 't', 'w', 'o', 'r', 'k', ' ' };

static constexpr string_view layer_header_str_view { layer_header_str.data(), layer_header_str.size() };

static constexpr string_view network_header_str_view { network_header_str.data(), network_header_str.size() };

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
  LayerSerDes::parse( network.template get_layer<0>(), in );

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
  LayerSerDes::serialize( network.template get_layer<0>(), out );

  serialize_internal( network, out );

  /* todo:
     (1) generate 16 random inputs (type T_network::M_input)
     (2) apply each one through the network to produce an output
     (3) serialize each input (maybe with an "input   " header)
         and its corresponding output (maybe with an "output  " header) */
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

  /* todo:
     (1) parse each of the 16 random inputs (each starting with the
         "input   " header)
     (2) parse each of the 16 corresponding outputs (each starting
         with the "output  " header)
     (3) apply the parsed network to each of the inputs
     (4) confirm that the computed output matches the parsed expectation
     (by "matched" maybe we mean it's within .01% or something -- doesn't
     have to be perfectly exact in the == sense between two floats) */
}
}

template void NetworkSerDes::serialize<DNN>( const DNN&, Serializer& );
template void NetworkSerDes::parse<DNN>( DNN&, Parser& );