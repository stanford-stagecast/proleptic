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

std::string AutoCDF::graph( const unsigned int width, const unsigned int height, vector<float>& values )
{
  make_cdf( values, num_cdf_points, cdf_ );

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
  : weight_values_( Network::num_layers )
  , bias_values_( Network::num_layers )
  , activation_values_( Network::num_layers )
  , weight_svgs_( Network::num_layers )
  , bias_svgs_( Network::num_layers )
{
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    weight_cdfs_.emplace_back( "CDF of weights @ layer " + to_string( i ), "weight" );
    bias_cdfs_.emplace_back( "CDF of biases @ layer " + to_string( i ), "bias" );
    activation_cdfs_.emplace_back( "CDF of activations @ layer " + to_string( i ), "activation" );
  }
}

static void clear_all( vector<vector<float>>& vec )
{
  for ( auto& v : vec ) {
    v.clear();
  }
}

template<class Network>
void NetworkGraph<Network>::initialize( const Network& net )
{
  reset_activations();

  /* precompute weight and bias graphs */
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    weight_values_.at( i ).clear();
    extract<GetWeights>( net, i, weight_values_.at( i ) );
    weight_svgs_.at( i ) = weight_cdfs_.at( i ).graph( 640, 480, weight_values_.at( i ) );

    bias_values_.at( i ).clear();
    extract<GetBiases>( net, i, bias_values_.at( i ) );
    bias_svgs_.at( i ) = bias_cdfs_.at( i ).graph( 640, 480, bias_values_.at( i ) );
  }
}

template<class Network>
void NetworkGraph<Network>::reset_activations()
{
  clear_all( activation_values_ );
}

template<class Network>
string NetworkGraph<Network>::graph()
{
  string svg;
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    /* weight and bias graphs are precomputed until the network changes */
    svg.append( weight_svgs_.at( i ) );
    svg.append( bias_svgs_.at( i ) );
    svg.append( activation_cdfs_.at( i ).graph( 640, 480, activation_values_.at( i ) ) );

    svg.append( "<p>" );
  }
  return svg;
}

template<class Network>
template<int T_batch_size>
void NetworkGraph<Network>::add_activations(
  const typename Network::template Activations<T_batch_size>& activations )
{
  for ( size_t i = 0; i < Network::num_layers; ++i ) {
    extract<GetActivations>( activations, i, activation_values_.at( i ) );
  }
}

template class NetworkGraph<DNN>;
template void NetworkGraph<DNN>::add_activations<BATCH_SIZE>( const DNN::Activations<BATCH_SIZE>& activations );
