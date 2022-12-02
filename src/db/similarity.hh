#pragma once
#include <Eigen/Dense>

#include <algorithm>
#include <set>
#include <vector>

#include <assert.h>

//! Calculates the Jaccard Index (the basis for MinHash) naively; this is slow and should probably only be used for
//! checking faster methods.
template<std::integral T, std::size_t N>
float jaccard_index( const std::array<T, N>& a, const std::array<T, N>& b )
{
  static_assert( N > 0 );
  std::array<T, N> x( a );
  std::array<T, N> y( b );
  std::sort( x.begin(), x.end() );
  std::sort( y.begin(), y.end() );
  std::vector<T> intersection( 2 * N );
  auto it = std::set_intersection( x.begin(), x.end(), y.begin(), y.end(), intersection.begin() );
  intersection.resize( it - intersection.begin() );

  std::vector<T> union_( 2 * N );
  it = std::set_union( x.begin(), x.end(), y.begin(), y.end(), union_.begin() );
  union_.resize( it - union_.begin() );
  return intersection.size() / static_cast<float>( union_.size() );
}

//! Calculates the similarity between two MinHashes of sets.  This should be much faster than the raw jaccard index
//! calculation, but is only an approximation (the accuracy depends on the parameters configured for MinHash).
template<std::integral T, std::size_t N>
float minhash_similarity( const Eigen::Vector<T, N>& minhash_a, const Eigen::Vector<T, N>& minhash_b )
{
  return ( minhash_a - minhash_b ).unaryExpr( []( const auto x ) { return x == 0; } ).count()
         / static_cast<float>( N );
}
