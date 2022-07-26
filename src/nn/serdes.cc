#include "serdes.hh"
#include "dnn_types.hh"

#include <iostream>

using namespace std;

namespace LayerSerDes {
template<typename T_layer>
void serialize( const T_layer& layer, Serializer& out )
{
  out.string( "layer   " );
  out.integer( T_layer::input_size );
  out.integer( T_layer::output_size );

  for ( unsigned int i = 0; i < layer.weights().size(); ++i ) {
    out.floating( layer.weights().data() + i );
  }

  for ( unsigned int i = 0; i < layer.biases().size(); ++i ) {
    out.floating( layer.biases().data() + i );
  }
}
}

namespace NetworkSerDes {
template<typename T_network>
void serialize( const T_network& network, Serializer& out )
{
  out.string( "network " );
  LayerSerDes::serialize( network.template get_layer<0>(), out );
  if constexpr ( T_network::is_last_layer ) {
    return;
  } else {
    NetworkSerDes::serialize( network.rest(), out );
  }
}
}

template void NetworkSerDes::serialize<DNN>( const DNN&, Serializer& );
