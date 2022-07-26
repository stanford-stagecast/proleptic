#pragma once

#include "parser.hh"

namespace LayerSerDes {
template<typename T_layer>
void serialize( const T_layer& layer, Serializer& out );
template<typename T_layer>
void parse( T_layer& layer, Parser& in );
}

namespace NetworkSerDes {
template<typename T_network>
void serialize( const T_network& network, Serializer& out );
template<typename T_network>
void parse( T_network& layer, Parser& in );
}

using NetworkSerDes::parse;
using NetworkSerDes::serialize;
