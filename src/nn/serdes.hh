#pragma once

#include "parser.hh"

namespace LayerSerDes {
template<typename LayerT>
void serialize( const LayerT& layer, Serializer& out );
template<typename LayerT>
void parse( LayerT& layer, Parser& in );
}

namespace NetworkSerDes {
template<typename NetworkT>
void serialize( const NetworkT& network, Serializer& out );
template<typename NetworkT>
void parse( NetworkT& layer, Parser& in );

template<typename NetworkT>
void serialize_internal( const NetworkT& network, Serializer& out );
template<typename NetworkT>
void parse_internal( NetworkT& layer, Parser& in );
}

using NetworkSerDes::parse;
using NetworkSerDes::serialize;
