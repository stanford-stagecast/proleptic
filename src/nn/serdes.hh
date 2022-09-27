#pragma once

#include "layer.hh"
#include "network.hh"
#include "parser.hh"

namespace LayerSerDes {
template<LayerT LayerT>
void serialize( const LayerT& layer, Serializer& out );
template<LayerT LayerT>
void parse( LayerT& layer, Parser& in );
}

namespace NetworkSerDes {
template<NetworkT NetworkT>
void serialize( const NetworkT& network, Serializer& out );
template<NetworkT NetworkT>
void parse( NetworkT& layer, Parser& in );

template<NetworkT NetworkT>
void serialize_internal( const NetworkT& network, Serializer& out );
template<NetworkT NetworkT>
void parse_internal( NetworkT& layer, Parser& in );
}

using NetworkSerDes::parse;
using NetworkSerDes::serialize;
