#pragma once

#include "layer.hh"
#include "network.hh"
#include "parser.hh"

namespace LayerSerDes {
template<LayerT Layer>
void serialize( const Layer& layer, Serializer& out );
template<LayerT Layer>
void parse( Layer& layer, Parser& in );
}

namespace NetworkSerDes {
template<NetworkT Network>
void serialize( const Network& network, Serializer& out );
template<NetworkT Network>
void parse( Network& layer, Parser& in );

template<NetworkT Network>
void serialize_internal( const Network& network, Serializer& out );
template<NetworkT Network>
void parse_internal( Network& layer, Parser& in );
}

using NetworkSerDes::parse;
using NetworkSerDes::serialize;
