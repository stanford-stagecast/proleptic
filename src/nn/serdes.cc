#include "serdes.hh"
#include "dnn_types.hh"
#include "inference.hh"

using namespace std;

static constexpr array<char, 8> layer_header_str = { 'l', 'a', 'y', 'e', 'r', ' ', ' ', ' ' };
static constexpr array<char, 8> network_header_str = { 'n', 'e', 't', 'w', 'o', 'r', 'k', ' ' };

static constexpr string_view layer_header_str_view { layer_header_str.data(), layer_header_str.size() };

static constexpr string_view network_header_str_view { network_header_str.data(), network_header_str.size() };

static constexpr array<char, 8> test_input_header_str { 'i', 'n', 'p', 'u', 't', ' ', ' ', ' ' };
static constexpr array<char, 8> test_output_header_str { 'o', 'u', 't', 'p', 'u', 't', ' ', ' ' };
static constexpr array<char, 8> roundtrip_test_end_str { 'f', 'i', 'n', 'i', 's', 'h', 'e', 'd' };

static constexpr string_view test_input_header_str_view { test_input_header_str.data(),
                                                          test_input_header_str.size() };

static constexpr string_view test_output_header_str_view { test_output_header_str.data(),
                                                           test_output_header_str.size() };

static constexpr string_view roundtrip_test_end_str_view { roundtrip_test_end_str.data(),
                                                           roundtrip_test_end_str.size() };

static constexpr unsigned int num_examples = 16;

namespace LayerSerDes {
template<LayerT Layer>
void serialize( const Layer& layer, Serializer& out )
{
  out.string( layer_header_str_view );
  out.integer( Layer::input_size );
  out.integer( Layer::output_size );

  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    out.floating( *( layer.weights.data() + i ) );
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    out.floating( *( layer.biases.data() + i ) );
  }
}

template<LayerT Layer>
void parse( Layer& layer, Parser& in )
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

  if ( input_size != Layer::input_size or output_size != Layer::output_size ) {
    throw runtime_error( "input or output size mismatch" );
  }

  for ( unsigned int i = 0; i < layer.weights.size(); ++i ) {
    in.floating( *( layer.weights.data() + i ) );
  }

  for ( unsigned int i = 0; i < layer.biases.size(); ++i ) {
    in.floating( *( layer.biases.data() + i ) );
  }
}
}

namespace NetworkSerDes {
template<NetworkT Network>
void serialize_internal( const Network& network, Serializer& out )
{
  if constexpr ( not Network::is_last ) {
    NetworkSerDes::serialize( network.rest, out );
  }
}

template<NetworkT Network>
void parse_internal( Network& network, Parser& in )
{
  LayerSerDes::parse( network.first, in );

  if constexpr ( not Network::is_last ) {
    NetworkSerDes::parse( network.rest, in );
  }
}

template<NetworkT Network>
void serialize( const Network& network, Serializer& out )
{
  out.string( network_header_str_view );
  LayerSerDes::serialize( network.first, out );

  serialize_internal( network, out );

  /* Consistency check for serialization and parsing
     (1) generate 16 random inputs
     (2) apply each one through the network to produce an output
     (3) serialize each input (maybe with an "input   " header)
         and its corresponding output (maybe with an "output  " header) */

  using Infer = NetworkInference<Network, num_examples>;

  // initialize some random input
  typename Infer::Input my_input;
  my_input.Random();

  // initialize output container
  Infer inference;
  inference.apply( network, my_input );

  // serialize input header
  out.string( test_input_header_str_view );

  // serialize input
  for ( unsigned int i = 0; i < my_input.size(); ++i ) {
    out.floating( *( my_input.data() + i ) );
  }

  // serialize output header
  out.string( test_output_header_str_view );

  // serialize output
  for ( unsigned int i = 0; i < inference.output().size(); ++i ) {
    out.floating( *( inference.output().data() + i ) );
  }

  // serialize end of test
  out.string( roundtrip_test_end_str_view );
}

template<NetworkT Network>
void parse( Network& network, Parser& in )
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

  using Infer = NetworkInference<Network, num_examples>;

  // input header demarcates input
  array<char, 8> test_input_header;
  string_span test_input_header_span { test_input_header.data(), test_input_header.size() };

  // verify input header
  in.string( test_input_header_span );
  if ( test_input_header != test_input_header_str ) {
    throw runtime_error( "missing test input header " + string( test_input_header_span ) );
  }

  // read in input
  typename Infer::Input my_input;

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
  typename Infer::Output expected_output;
  for ( unsigned int i = 0; i < expected_output.size(); ++i ) {
    in.floating( *( expected_output.data() + i ) );
  }

  // read in end of test
  array<char, 8> test_end_point;
  string_span test_end_point_span { test_end_point.data(), test_end_point.size() };

  // verify end of test
  in.string( test_end_point_span );
  if ( test_end_point != roundtrip_test_end_str ) {
    throw runtime_error( "mismatched roundtrip test ends" );
  }

  // confirm that the network's output matches the serialized expectation
  Infer inference;
  inference.apply( network, my_input );

  if ( ( inference.output() - expected_output ).cwiseAbs().maxCoeff() > 1e-5 ) {
    throw runtime_error( "DNN roundtrip failure" );
  }
}
}

template void NetworkSerDes::serialize<DNN>( const DNN&, Serializer& );
template void NetworkSerDes::parse<DNN>( DNN&, Parser& );

template void NetworkSerDes::serialize<DNN_timestamp>( const DNN_timestamp&, Serializer& );
template void NetworkSerDes::parse<DNN_timestamp>( DNN_timestamp&, Parser& );

template void NetworkSerDes::serialize<DNN_tempo>( const DNN_tempo&, Serializer& );
template void NetworkSerDes::parse<DNN_tempo>( DNN_tempo&, Parser& );

template void NetworkSerDes::serialize<DNN_period>( const DNN_period&, Serializer& );
template void NetworkSerDes::parse<DNN_period>( DNN_period&, Parser& );

template void NetworkSerDes::serialize<DNN_piano_roll_rhythm_prediction>( const DNN_piano_roll_rhythm_prediction&,
                                                                          Serializer& );
template void NetworkSerDes::parse<DNN_piano_roll_rhythm_prediction>( DNN_piano_roll_rhythm_prediction&, Parser& );

template void NetworkSerDes::serialize<DNN_piano_roll_octave_prediction>( const DNN_piano_roll_octave_prediction&,
                                                                          Serializer& );
template void NetworkSerDes::parse<DNN_piano_roll_octave_prediction>( DNN_piano_roll_octave_prediction&, Parser& );
