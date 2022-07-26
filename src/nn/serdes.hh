#pragma once

#include "parser.hh"

namespace LayerSerDes {
template<typename T_layer>
void serialize( const T_layer& layer, Serializer& out );
}

namespace NetworkSerDes {
template<typename T_network>
void serialize( const T_network& network, Serializer& out );
}
