#include "nngraph.hh"
#include "cdf.hh"
#include "dnn_types.hh"
#include "grapher.hh"

#include <limits>
#include <stdexcept>

using namespace std;

AutoCDF::AutoCDF( const string_view title, const string_view xlabel )
  : title_( title )
  , xlabel_( xlabel )
{
}

std::string AutoCDF::graph( const unsigned int width, const unsigned int height )
{
  make_cdf( values_, num_cdf_points, cdf_ );

  if ( cdf_.empty() ) {
    throw runtime_error( "empty CDF" );
  }

  xrange_ = { min( xrange_.first, cdf_.front().first ), max( xrange_.second, cdf_.back().first ) };
  yrange_ = { min( yrange_.first, cdf_.front().second ), max( yrange_.second, cdf_.back().second ) };

  Graph graph { { width, height }, xrange_, yrange_, title_, xlabel_, "cumulative fraction" };
  graph.draw_line( cdf_ );
  graph.finish();
  return move( graph.mut_svg() );
}

template<template<typename> typename Getter, class T>
void extract( const T& obj, const size_t layer_no, vector<float>& vec )
{
  if ( layer_no > 0 ) {
    if constexpr ( T::is_last_layer ) {
      throw runtime_error( "layer_no out of range" );
    } else {
      return extract<Getter>( obj.rest(), layer_no - 1, vec );
    }
  }

  const auto& value_matrix = Getter<T>::get( obj );

  for ( unsigned int i = 0; i < value_matrix.size(); ++i ) {
    const float value = *( value_matrix.data() + i );
    vec.emplace_back( value );
  }
}

template<class T>
struct GetWeights
{
  static constexpr auto& get( const T& net ) { return net.first_layer().weights(); }
};

template<class T>
struct GetBiases
{
  static constexpr auto& get( const T& net ) { return net.first_layer().biases(); }
};

template<class T>
struct GetActivations
{
  static constexpr auto& get( const T& act ) { return act.first_layer(); }
};

template<class Network>
NetworkGraph<Network>::NetworkGraph()
  : weights_()
  , biases_()
  , weight_svgs_( Network::num_layers )
  , bias_svgs_( Network::num_layers )
{
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    weights_.emplace_back( "CDF of weights @ layer " + to_string( i ), "weight" );
    biases_.emplace_back( "CDF of biases @ layer " + to_string( i ), "bias" );
  }
}

template<class Network>
void NetworkGraph<Network>::initialize( const Network& net )
{
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    auto& weight_cdf = weights_.at( i );
    weight_cdf.values().clear();
    extract<GetWeights>( net, i, weight_cdf.values() );
    weight_svgs_.at( i ) = weight_cdf.graph( 640, 480 );

    auto& bias_cdf = biases_.at( i );
    bias_cdf.values().clear();
    extract<GetBiases>( net, i, bias_cdf.values() );
    bias_svgs_.at( i ) = bias_cdf.graph( 640, 480 );
  }
}

template<class Network>
string NetworkGraph<Network>::graph()
{
  string svg;
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    svg.append( weight_svgs_.at( i ) );
    svg.append( bias_svgs_.at( i ) );
    svg.append( "<p>" );
  }
  return svg;
}

template class NetworkGraph<DNN>;
